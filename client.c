#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "params.h"

char name[NAMESIZE];
int done = 0;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void* send_data(void* arg) {
    char buf[MAXDATASIZE]; buf[MAXDATASIZE-1] = '\0';
    int len;
    int server_socket = *(int*)arg;
    while (1){
        printf("%s: ", name);
        if (fgets(buf, MAXDATASIZE-1, stdin) == NULL) continue;
        len = strcspn(buf, "\n");
        buf[len] = '\0';

        if (buf[0] == '\\') {
            if (strcmp(buf, "\\exit") == 0) break;
        }

        if (send(server_socket, buf, len+1, 0) == -1)
            perror("send");
    }
    done = 1;
    close(server_socket);
    pthread_exit(NULL);
    return NULL;
}

void* recv_data(void* arg) {
    char buf[NAMESIZE+MAXDATASIZE];
    int len;
    int server_socket = *(int*)arg;
    while (1){
        if ((len = recv(server_socket, buf, MAXDATASIZE-1, 0)) == -1) {
            if (done == 1) break;
            perror("recv");
            exit(1);
        }
        if (len == 0) break;
        buf[len] = '\0';
        printf("\r%s: %s\n%s: ", buf, buf+NAMESIZE, name);
        fflush(stdout);
    }
    close(server_socket);
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[])
{
    int server_socket;  
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    pthread_t send_thread;
    pthread_t recv_thread;

    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    {
        printf("Username: ");
        if (fgets(name, NAMESIZE-1, stdin) == NULL) exit(1);
        int namelen = strcspn(name, "\n");
        name[namelen] = '\0';
        if (send(server_socket, name, namelen+1, 0) == -1)
            perror("send");
    }

    int arg = server_socket;

    pthread_create(&send_thread, NULL, send_data, &arg);
    pthread_create(&recv_thread, NULL, recv_data, &arg);

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    return 0;
}
