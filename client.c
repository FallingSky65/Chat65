#include "clientbase.c"

void* send_data(void* arg) {
    char buf[MAXDATASIZE]; buf[MAXDATASIZE-1] = '\0';
    int len;
    while (1){
        printf("%s: ", name);
        if (fgets(buf, MAXDATASIZE-1, stdin) == NULL) continue;
        len = strlen(buf);
        if (len == 1) {
            continue;
        }
        //len = strcspn(buf, "\n");
        buf[len] = '\0';

        if (buf[0] == '\\') {
            if (strcmp(buf, "\\exit") == 0) break;
        }

        if (send(server_socket, buf, len, 0) == -1)
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
    while (1){
        len = recv(server_socket, buf, NAMESIZE+MAXDATASIZE-1, 0);
        if (len == -1) {
            if (done == 1) break;
            perror("recv");
            exit(1);
        }
        if (len == 0) break;

        char msg[MAXDATASIZE];
        int msglen = 0;
        for (int i = 0; i < len; i++) {
            if (buf[i] == '\n') {
                msg[msglen] = '\0';
                printf("\r%s: %s\n%s: ", msg, msg+NAMESIZE, name);
                //printf("msg: %s\n", msg);
                msglen = 0;
            } else {
                msg[msglen++] = buf[i];
            }
        }

        buf[len] = '\0';
        //printf("\n got a msg\n");
        //printf("\r%s: %s%s: ", buf, buf+NAMESIZE, name);
        fflush(stdout);
    }
    close(server_socket);
    pthread_exit(NULL);
    return NULL;
}



int main(int argc, char *argv[])
{
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

    system("clear");

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

    system("clear");

    int arg = server_socket;

    pthread_create(&send_thread, NULL, send_data, &arg);
    pthread_create(&recv_thread, NULL, recv_data, &arg);

    pthread_join(send_thread, NULL);
    pthread_join(recv_thread, NULL);

    return 0;
}
