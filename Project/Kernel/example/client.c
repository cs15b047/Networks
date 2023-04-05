// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <strings.h> // bzero()
#include <arpa/inet.h> // inet_addr
#include <sys/time.h> // gettimeofday
#include <assert.h>

#define PORT	 8080
#define MAX 	 80
#define MAXLINE 32768
#define PORT 8080
#define SA struct sockaddr

uint64_t get_current_time() {
	// use gettimeofday
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

void func(int sockfd)
{
	char buff[MAX];
	int n;
	for (;;) {
		bzero(buff, sizeof(buff));
		printf("Enter the string : ");
		n = 0;
		while ((buff[n++] = getchar()) != '\n')
			;
		write(sockfd, buff, sizeof(buff));
		bzero(buff, sizeof(buff));
		read(sockfd, buff, sizeof(buff));
		printf("From Server : %s", buff);
		if ((strncmp(buff, "exit", 4)) == 0) {
			printf("Client Exit...\n");
			break;
		}
	}
}

void send_data(int sockfd) {
	char *buff = calloc(MAXLINE, sizeof(char));
	size_t buffer_size = MAXLINE * sizeof(char);
	// set data
	bzero(buff, buffer_size);
	for (int i = 0; i < buffer_size; i++) {
		buff[i] = 'a'+ rand() % 26;
	}
	buff[buffer_size - 1] = '\0';

	uint64_t total_time = 0, iters = 1024 * 1024, total_bytes_sent = 0;

	for(int i = 0; i < iters; i++) {
		// send data
		uint64_t start, end;

		start = get_current_time();
		ssize_t sent_bytes = write(sockfd, buff, buffer_size);
		int read_bytes = read(sockfd, buff, buffer_size);
		end = get_current_time();
		total_time += end - start;
		total_bytes_sent += sent_bytes;
		// assert(read_bytes == 4);
		// assert(strcmp(buff, "ack\0") == 0);
		// printf("Sent + ack time: %lu\n", end - start);
	}


	printf("Sent %ld bytes in %lu microseconds --> %lf GB/s\n", total_bytes_sent, total_time, (total_bytes_sent /(double)1e3)/(double)total_time);
}

int main()
{
	int sockfd, connfd;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else {
		printf("Socket successfully created..\n");
		fflush(stdout);
	}
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("128.110.217.58");
	servaddr.sin_port = htons(PORT);

	// connect the client socket to server socket
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		printf("connection with the server failed...\n");
		exit(0);
	}
	else
		printf("connected to the server..\n");

	// function for chat
	send_data(sockfd);

	// close the socket
	close(sockfd);
}

