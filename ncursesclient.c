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
#include <ncurses.h>

#include "clientbase.c"

int screen_height, screen_width;

pthread_mutex_t lock;

pthread_t print_chats_thread;

void* print_chats(void* arg) {
    while (done == 0) {
        if (historyUpdated == 0) continue;
        pthread_mutex_lock(&lock);
        int top = screen_height - 2;
        for (int i = historyIndex; i >= 0; i--) {
            if (history[i].status == 0) exit(69);
            int len = 1 + strlen(history[i].sender_name) + strlen(history[i].contents);
            top -= (len + screen_width - 1)/screen_width;
            if (top < 0) break;
            mvprintw(top, 0, "%s: %s", history[i].sender_name, history[i].contents);
            clrtoeol();
        }
        move(screen_height-1, 0);
        refresh();
        pthread_mutex_unlock(&lock);
        historyUpdated = 0;
    }
    done = 1;
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr,"usage: client hostname port\n");
        exit(1);
    }
    init_connect(argv[1], argv[2]);
    int ch;

    pthread_mutex_init(&lock, NULL);

    initscr(); // begin curses
    cbreak();
    //raw();
    noecho();

    clear();

    getmaxyx(stdscr, screen_height, screen_width);

    char buf[MAXDATASIZE];
    int bufi = 0; buf[bufi] = '\0';

    move(screen_height-2, 0);
    hline('-', screen_width);
    move(screen_height-1, 0);
    refresh();

    pthread_create(&recv_thread, NULL, recv_data, NULL);
    pthread_create(&print_chats_thread, NULL, print_chats, NULL);
    
    while ((ch = getch()) != 27) { // while ch != escape
        if (32 <= ch && ch < 127 && bufi + 1 < MAXDATASIZE) { // while SPACE <= ch < DEL
            if (bufi + 1 >= MAXDATASIZE) {
                beep();
            } else {
                buf[bufi++] = (char)ch;
                buf[bufi] = '\0';
            }
        }
        if (ch == 127 && bufi > 0) { // DEL
            buf[--bufi] = '\0';
        }
        if (ch == 10 && bufi > 0) { // enter/NL
            if (send(server_socket, buf, bufi+1, 0) == -1)
                perror("send");
            bufi = 0;
            buf[bufi] = '\0';
        }

        pthread_mutex_lock(&lock);
        
        move(screen_height-2, 0);
        hline('-', screen_width);
        if (bufi <= screen_width)
            mvprintw(screen_height-1, 0, "%s", buf);
        else
            mvprintw(screen_height-1, 0, "...%s", buf+bufi-screen_width+3);
        clrtoeol();
        refresh();

        pthread_mutex_unlock(&lock);
    }
    done = 1;
    close(server_socket);
    pthread_join(print_chats_thread, NULL);
    pthread_join(recv_thread, NULL);
    pthread_mutex_destroy(&lock);
    
    endwin();

    printf("exited successfully\n");

    return 0;
}
