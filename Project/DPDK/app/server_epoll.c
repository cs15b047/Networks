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

#include "ff_config.h"
#include "ff_api.h"
#include "ff_epoll.h"

#include "utils.h"

#define MAX_EVENTS 512000
#define MAXLINE 1024 * 1024 * 100

struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];



int epfd;
int sockfd;


char ack[2] = "a";
uint64_t total_bytes_sent = 0, total_bytes_recv = 0;
char buf[MAXLINE];

void process_client(int clientfd) {
    ssize_t readlen = ff_read(clientfd, buf, sizeof(buf));
    if (readlen < 0){
        printf("ff_read failed:%d, %s\n", errno,
            strerror(errno));
    }
    uint64_t num_packets = readlen / MAXLINE;

    for(int i = 0; i < num_packets; i++){
        ssize_t writelen = ff_write(clientfd, ack, sizeof(ack));
        if (writelen < 0){
            printf("ff_write failed:%d, %s\n", errno,
                strerror(errno));
        } 
        // total_bytes_sent += writelen;
        // total_bytes_recv += readlen;

        // printf("total_bytes_sent: %lu, total_bytes_recv: %lu\n", total_bytes_sent, total_bytes_recv);
    }
}

void process_client2(int clientfd) {
    size_t readlen = ff_read(clientfd, buf, sizeof(buf));
    if(readlen > 0) {
        ff_write(clientfd, ack, sizeof(ack));
    } else {
        ff_epoll_ctl(epfd, EPOLL_CTL_DEL, clientfd, NULL);
        ff_close(clientfd);
    }
}

int loop(void *arg)
{
    /* Wait for events to happen */
    int nevents = ff_epoll_wait(epfd,  events, MAX_EVENTS, 0);
    int i;

    if (nevents < 0) {
        printf("ff_kevent failed:%d, %s\n", errno,
                        strerror(errno));
        return -1;
    }

    for (i = 0; i < nevents; ++i) {
        int eventfd = events[i].data.fd;
        /* Handle new connect */
        if (eventfd == sockfd) {
            while (1) {
                int nclientfd = ff_accept(sockfd, NULL, NULL);
                if (nclientfd < 0) {
                    break;
                }

                /* Add to event list */
                ev.data.fd = nclientfd;
                ev.events  = EPOLLIN;
                if (ff_epoll_ctl(epfd, EPOLL_CTL_ADD, nclientfd, &ev) != 0) {
                    printf("ff_epoll_ctl failed:%d, %s\n", errno,
                        strerror(errno));
                    break;
                }
            }
        } else { 
            if (events[i].events & EPOLLERR ) {
                /* Simply close socket */
                ff_epoll_ctl(epfd, EPOLL_CTL_DEL,  eventfd, NULL);
                ff_close(eventfd);
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
    ff_init(argc, argv);

    sockfd = ff_socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("ff_socket failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    /* Set non blocking */
    int on = 1;
    ff_ioctl(sockfd, FIONBIO, &on);

    struct sockaddr_in my_addr;
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SERV_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = ff_bind(sockfd, (struct linux_sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        printf("ff_bind failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    ret = ff_listen(sockfd, MAX_EVENTS);
    if (ret < 0) {
        printf("ff_listen failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }
    
    assert((epfd = ff_epoll_create(0)) > 0);
    ev.data.fd = sockfd;
    ev.events = EPOLLIN;
    ff_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    ff_run(loop, NULL);
    return 0;
}
