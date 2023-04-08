#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

// Server side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <netinet/in.h> // struct sockaddr_in
#include <stdint.h>

#include <sys/epoll.h>

#include "utils.h"

#define MAX_EVENTS 51200
#define MAXLINE 1024
#define SA struct sockaddr

int epfd;
int sockfd;

struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];

char ack[2] = "a";
uint64_t total_bytes_sent = 0, total_bytes_recv = 0;
char buf[MAXLINE];

void process_client(int clientfd) {
    ssize_t readlen = read(clientfd, buf, sizeof(buf));
    if (readlen < 0){
        printf("read failed:%d, %s\n", errno,
            strerror(errno));
    }
    uint64_t num_packets = readlen / MAXLINE;

    for(int i = 0; i < num_packets; i++){
        ssize_t writelen = write(clientfd, ack, sizeof(ack));
        if (writelen < 0){
            printf("write failed:%d, %s\n", errno,
                strerror(errno));
        } 
        // total_bytes_sent += writelen;
        // total_bytes_recv += readlen;

        // printf("total_bytes_sent: %lu, total_bytes_recv: %lu\n", total_bytes_sent, total_bytes_recv);
    }
}

void process_client2(int clientfd) {
    size_t readlen = read(clientfd, buf, sizeof(buf));
    size_t writelen = -1;
    if(readlen > 0) {
        writelen = write(clientfd, ack, sizeof(ack));
    } else {
        epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
        close(clientfd);
    }
    printf("readlen: %lu, writelen: %lu\n", readlen, writelen);
}

int loop(void *arg)
{
    /* Wait for events to happen */
    int nevents = epoll_wait(epfd,  events, MAX_EVENTS, -1);
    int i;

    if (nevents < 0) {
        return -1;
    }


    for (i = 0; i < nevents; ++i) {
        int eventfd = events[i].data.fd;
        /* Handle new connect */
        if (eventfd == sockfd) {
            while (1) {
                int nclientfd = accept(sockfd, NULL, NULL);
                if (nclientfd < 0 && errno != EAGAIN) {
                    break;
                } else {
                    printf("accept clientfd:%d\n", nclientfd);
                }

                /* Add to event list */
                ev.data.fd = nclientfd;
                ev.events  = EPOLLIN;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, nclientfd, &ev) != 0) {
                    printf("epoll_ctl failed:%d, %s\n", errno,
                        strerror(errno));
                    break;
                }
            }
        } else { 
            if (events[i].events & EPOLLERR ) {
                /* Simply close socket */
                epoll_ctl(epfd, EPOLL_CTL_DEL,  eventfd, NULL);
                close(eventfd);
            } else if (events[i].events & EPOLLIN) {
                process_client2(eventfd);
            } else {
                printf("unknown event: %8.8X\n", events[i].events);
            }
        }
    }

    return 0;
}

int main(int argc, char * argv[])
{
    // init(argc, argv);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("socket failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    int epfd = epoll_create(100);
    if (epfd < 0) {
        printf("epoll_create failed, epfd:%d, errno:%d, %s\n", epfd, errno, strerror(errno));
        exit(1);
    }


    /* Set non blocking */
    int on = 1;
    ioctl(sockfd, FIONBIO, &on);

    struct sockaddr_in my_addr;
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SERV_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = bind(sockfd, (SA*)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        printf("bind failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    ret = listen(sockfd, MAX_EVENTS);
    if (ret < 0) {
        printf("listen failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    } else {
        printf("listen success, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
    }
    
    ev.data.fd = sockfd;
    ev.events = EPOLLIN;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    assert(ret == 0);

    while(1) {
        loop(NULL);
    }
    return 0;
}
