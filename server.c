#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>

#include "params.h"

struct Client;
struct Client {
    int client_socket;
    char name[NAMESIZE];
    int namelen;
    int historyIndex;

    struct Client* prev;
    struct Client* next;
};

struct Client* clients = NULL;

struct Message {
    struct Client* sender;
    char contents[MAXDATASIZE];
    int len;
};

struct Message history[MSGHISTORY];
int historyIndex = 0;
pthread_mutex_t historyLock;

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_data(void) {
    char buf[NAMESIZE+MAXDATASIZE];
    for (struct Client* c = clients; c != NULL; c = c->next) {
        for (int i = c->historyIndex; i < historyIndex; i++) {
            //printf("debug: c = %s; msg = %s\n", c->name, history[i].contents);
            if (history[i].sender == c) continue;
            //memset(buf, '\0', sizeof(buf));
            strncpy(buf, history[i].sender->name, NAMESIZE);
            strncpy(buf+NAMESIZE, history[i].contents, MAXDATASIZE);
            if (send(c->client_socket, buf, NAMESIZE+history[i].len, 0) <= 0)
                perror("send");
        }
        c->historyIndex = historyIndex;
    }
}

void* recv_data(void* arg) {
    char msg[MAXDATASIZE];
    int len;
    struct Client* client = (struct Client*)arg;
    while (1) {
        len = recv(client->client_socket, msg, MAXDATASIZE-1, 0);
        if (len == -1) {
            perror("recv");
            exit(1);
        }
        if (len == 0) break;
        msg[len] = '\0';
        printf("%s: %s\n", client->name, msg);

        pthread_mutex_lock(&historyLock);

        if (historyIndex == MSGHISTORY) {
            printf("server ran out of memory for history\n");
            exit(1);
        }
        
        history[historyIndex].sender = client;
        strncpy(history[historyIndex].contents, msg, MAXDATASIZE);
        history[historyIndex].len = len;
        historyIndex++;

        send_data();
        
        pthread_mutex_unlock(&historyLock);
    }
    printf("%s exited the chat\n", client->name);
    if (client->prev != NULL) client->prev->next = client->next;
    if (client->next != NULL) client->next->prev = client->prev;
    close(client->client_socket);
    free(client);
    pthread_exit(NULL);
    return NULL;
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    struct Client* client = (struct Client*)malloc(sizeof(struct Client*));
    
    client->namelen = recv(client_socket, client->name, NAMESIZE-1, 0);
    if (client->namelen <= 0) {
        perror("recv");
        exit(1);
    }
    client->name[client->namelen] = '\0';

    client->client_socket = client_socket;
    client->historyIndex = 0;
    client->prev = NULL;
    client->next = clients;
    if (clients != NULL) clients->prev = client;
    clients = client;

    printf("%s joined the chat\n", client->name);

    pthread_mutex_lock(&historyLock);
    send_data();
    pthread_mutex_unlock(&historyLock);

    pthread_t client_thread;
    pthread_create(&client_thread, NULL, recv_data, client);
    pthread_join(client_thread, NULL);

    pthread_exit(NULL);
    return NULL;
}

int main(void) {
    int server_socket, client_socket;
    struct addrinfo hints, *server_info, *p;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size;
    struct sigaction sa;
    int yes = 1;
    char ip_str[INET6_ADDRSTRLEN];
    int ret_val;

    pthread_t client_thread;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((ret_val = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret_val));
        return 1;
    }

    // iterate linked list
    for (p = server_info; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(server_info); // free linked list
    
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    system("clear");
    printf("server: waiting for connections...\n");

    pthread_mutex_init(&historyLock, NULL);

    while (1) {
        client_addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), ip_str, sizeof(ip_str));
        printf("server: got connection from %s\n", ip_str);

        int arg = client_socket;
        pthread_create(&client_thread, NULL, handle_client, &arg);
    }

    pthread_mutex_destroy(&historyLock);
}
