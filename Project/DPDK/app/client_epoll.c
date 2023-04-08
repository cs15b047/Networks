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
#include <fcntl.h>

#include "ff_config.h"
#include "ff_api.h"
#include "ff_epoll.h"

#include "utils.h"

#define MAX_EVENTS 51200
#define MAXLINE 1024 * 1024
#define SA struct sockaddr

uint64_t get_current_time() {
	// use gettimeofday
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

struct epoll_event ev;
struct epoll_event events[MAX_EVENTS];

/* kq */
int epfd;
int sockfd;

uint64_t start, end;
uint64_t iters = 1000, total_bytes_sent = 0, total_bytes_recv = 0;
char buff[MAXLINE];
char ack[2] = "a";

void send_data(int sockfd) {
    int buffer_size = MAXLINE;
	// set data
	bzero(buff, buffer_size);
	for (int i = 0; i < buffer_size; i++) {
		buff[i] = 'a' + (i % 26);
	}
	buff[buffer_size - 1] = '\0';

    start = get_current_time();

	for(int i = 0; i < iters; i++) {
		// send data
		ssize_t sent_bytes = ff_write(sockfd, buff, buffer_size);
		total_bytes_sent += sent_bytes;
        ev.data.fd = sockfd + (i+1);
        ev.events = EPOLLIN;
        ff_epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
	}

}


int loop(void *arg)
{
    /* Wait for events to happen */
    int nevents = ff_epoll_wait(epfd,  events, MAX_EVENTS, 0);
    int i;
    int finished = 0;

    if (nevents <= 0) {
        return -1;
    }

    for (i = 0; i < nevents; ++i) {
        int eventfd = events[i].data.fd;
        // struct kevent event = events[i];
         /* Handle disconnect */
        if (events[i].events & EPOLLERR ) {
                /* Simply close socket */
                ff_epoll_ctl(epfd, EPOLL_CTL_DEL,  events[i].data.fd, NULL);
                ff_close(events[i].data.fd);
        } else if (events[i].events & EPOLLOUT) {
            send_data(sockfd);
        } else if (events[i].events & EPOLLIN) {
            ssize_t read_bytes = ff_read(sockfd, buff, MAXLINE);
            end = get_current_time();
            total_bytes_recv += read_bytes;
            if(total_bytes_recv >= iters * sizeof(ack)) {
		        double total_time = end - start;
                double avg_latency = (double)total_time / (double)iters;
                printf("Average latency: %.2f microseconds for %d bytes\n", avg_latency, MAXLINE);
            	printf("Sent %ld bytes in %.2f microseconds --> %lf GB/s\n", total_bytes_sent, total_time, (total_bytes_sent /(double)1e3)/(double)total_time);
                finished = 1;
            }
        } else {
            printf("unknown event: %8.8X\n", events[i].events);
        }
    }

    if(finished) {
        ff_close(sockfd);
        exit(EXIT_SUCCESS);
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

    int on=1;
    ff_ioctl(sockfd, FIONBIO, &on);

    struct sockaddr_in server_sock;
    bzero(&server_sock,sizeof(server_sock));
    server_sock.sin_family = AF_INET;
    server_sock.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET,SERV_ADDR,&(server_sock.sin_addr));

    int ret = ff_connect(sockfd,(struct linux_sockaddr *)&server_sock,sizeof(server_sock));
    if (ret < 0 && errno != EINPROGRESS) {
        printf("ff_connect failed, sockfd:%d, errno:%d, %s\n", sockfd, errno, strerror(errno));
        exit(1);
    }

    assert((epfd = ff_epoll_create(0)) > 0);
    ev.data.fd = sockfd;
    ev.events = EPOLLOUT | EPOLLIN;
    ff_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);


     ff_run(loop, NULL);
    return 0;
}