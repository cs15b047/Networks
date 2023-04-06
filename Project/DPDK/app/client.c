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

#include "utils.h"

#define MAX_EVENTS 512
#define MAX_POLL 10
#define MAX 	 80
#define MAXLINE 32768
#define SA struct sockaddr

uint64_t get_current_time() {
	// use gettimeofday
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

/* kevent set */
struct kevent kevSet;
struct kevent events[MAX_EVENTS];

/* kq */
int kq;
int sockfd;

void send_data(int sockfd) {
	char *buff = calloc(MAXLINE, sizeof(char));
	size_t buffer_size = MAXLINE * sizeof(char);
	// set data
	bzero(buff, buffer_size);
	for (int i = 0; i < buffer_size; i++) {
		buff[i] = 'a'+ rand() % 26;
	}
	buff[buffer_size - 1] = '\0';

	uint64_t total_time = 0, iters = 1024 * 10, total_bytes_sent = 0;

	for(int i = 0; i < iters; i++) {
		// send data
		uint64_t start, end;

		start = get_current_time();
		ssize_t sent_bytes = ff_write(sockfd, buff, buffer_size);
		int read_bytes = ff_read(sockfd, buff, buffer_size);
		end = get_current_time();
		total_time += end - start;
		total_bytes_sent += buffer_size;
		// assert(read_bytes == 4);
		// assert(strcmp(buff, "ack\0") == 0);
		// printf("Sent + ack time: %lu\n", end - start);
	}


	printf("Sent %ld bytes in %lu microseconds --> %lf GB/s\n", total_bytes_sent, total_time, (total_bytes_sent /(double)1e3)/(double)total_time);
}


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
         /* Handle disconnect */
        if (event.flags & EV_EOF || event.flags & EV_ERROR) {
            ff_close(sockfd);
        } else if (event.filter == EVFILT_WRITE) {
            send_data(sockfd);
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

    EV_SET(&kevSet, sockfd, EVFILT_WRITE, EV_ADD, 0, MAX_EVENTS, NULL);
    ff_kevent(kq, &kevSet, 1, NULL, 0, NULL);

    ff_run(loop, NULL);
    return 0;
}