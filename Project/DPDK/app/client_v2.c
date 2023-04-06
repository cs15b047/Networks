#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "ff_config.h"
#include "ff_api.h"

#include "utils.h"

#define MAX_EVENTS 512

/* kevent set */
struct kevent kevSet;
/* events */
struct kevent events[MAX_EVENTS];
/* kq */
int kq;
int sockfd;

char html[] = "Hello from Client";

int loop(void *arg)
{
    /* Wait for events to happen */
    int nevents = ff_kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    int i;

    if (nevents < 0) {
        printf("ff_kevent failed:%d, %s\n", errno,
                        strerror(errno));
        return -1;
    }

    for (i = 0; i < nevents; ++i) {
        struct kevent event = events[i];
        int clientfd = (int)event.ident;

        printf("event fd:%d\n", clientfd);
         /* Handle disconnect */
        if (event.flags & EV_EOF|| event.flags&EV_ERROR) {
            /* Simply close socket */
            printf("event eof or error:%d,%d,%d\n", event.flags, EV_EOF, EV_ERROR);
            ff_close(clientfd);
        } else if (event.filter == EVFILT_WRITE) {
            printf("write:%d\n", clientfd);
            sleep(1);
            ff_write(clientfd, html, sizeof(html) - 1);
        } else {
            printf("unknown event: %8.8X\n", event.flags);
        }
    }
}


int main(int argc, char * argv[])
{
    ff_init(argc, argv);
    
    kq = ff_kqueue();
    if (kq < 0) {
        printf("ff_kqueue failed, errno:%d, %s\n", errno, strerror(errno));
        exit(1);
    }

    sockfd = ff_socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        printf("ff_socket failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    int on=1;
    ff_ioctl(sockfd, FIONBIO, &on);

    struct sockaddr_in server_sock;
    bzero(&server_sock,sizeof(server_sock));
    server_sock.sin_family = AF_INET;
    server_sock.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET,SERV_ADDR,&(server_sock.sin_addr));

    int ret = ff_connect(sockfd,(struct linux_sockaddr *)&server_sock,sizeof(server_sock));
    if (ret < 0 && errno != EINPROGRESS) {
        printf("ff_bind failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    EV_SET(&kevSet, sockfd, EVFILT_READ, EV_ADD, 0, MAX_EVENTS, NULL);
    // /* Update kqueue */
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);

    ff_run(loop, NULL);
    return 0;
}