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
    struct Client* prev;
    struct Client* next;
};

struct Client* clients = NULL;

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

void* recv_data(void* arg) {
    char buf[NAMESIZE+MAXDATASIZE];
    int len;
    struct Client* client = (struct Client*)arg;
    while (1){
        len = recv(client->client_socket, buf+NAMESIZE, MAXDATASIZE-1, 0);
        if (len == -1) {
            perror("recv");
            exit(1);
        }
        if (len == 0) break;
        buf[NAMESIZE+len] = '\0';
        strncpy(buf, client->name, sizeof(client->name));
        printf("%s: %s\n", client->name, buf+NAMESIZE);

        for (struct Client* c = clients; c != NULL; c = c->next) {
            if (c != client) {
                if (send(c->client_socket, buf, NAMESIZE+len+1, 0) == -1)
                    perror("send");
            }
        }
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
    client->prev = NULL;
    client->next = clients;
    if (clients != NULL) clients->prev = client;
    clients = client;

    printf("%s joined the chat\n", client->name);

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

    printf("server: waiting for connections...\n");

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
}
