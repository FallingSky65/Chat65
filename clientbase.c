#include <stdio.h>
#include <stdlib.h>
#include <sys/_endian.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "packet.h"

char name[NAMESIZE];
int done = 0;

struct Message {
    int status;
    char sender_name[NAMESIZE];
    char contents[MAXDATASIZE];
};

struct Message history[MSGHISTORY] = {0};
int historyIndex = 0;
int historyUpdated = 0;
pthread_mutex_t historyLock;

int server_socket;

pthread_t recv_thread;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void init_connect(char *server_ip) {
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server_ip, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
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
        exit(2);
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
}

void* recv_data(void* arg) {
    Packet packet;
    char buffer[sizeof(packet)];
    while (done == 0) {
        int offset = 0;
        while (offset < sizeof(buffer)) {
            int count = recv(server_socket, buffer+offset, sizeof(buffer)-offset, 0);
            if (count == -1) {
                if (done > 0) break;
                perror("recv");
                exit(1);
            }
            if (count == 0) {
                done = 1;
                break;
            }
            offset += count;
        }
        if (done > 0) break;

        memcpy(&packet, buffer, sizeof(packet));

        packet.index = ntohl(packet.index);
        if (packet.index >= MSGHISTORY) {
            printf("packet index is not right\n");
            exit(1);
        }

        if (packet.index > historyIndex) historyIndex = packet.index;

        if (history[packet.index].status > 0) continue;
        
        memcpy(history[packet.index].sender_name, packet.sender, NAMESIZE);
        memcpy(history[packet.index].contents, packet.message, MAXDATASIZE);
        history[packet.index].status = 1;
        historyUpdated = 1;
    }
    done = 1;
    pthread_exit(NULL);
    return NULL;
}
