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

#include "utils.h"

#define MAXLINE 1024 * 10


#define SA struct sockaddr

char ack[2] = "a";

uint64_t get_current_time() {
	// use gettimeofday
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

// Function designed for chat between client and server.
void func(int connfd)
{
	char buff[MAXLINE];
	int n;
	// infinite loop for chat
	for (;;) {
		bzero(buff, MAXLINE);

		// read the message from client and copy it in buffer
		read(connfd, buff, sizeof(buff));
		// print buffer which contains the client contents
		printf("From client: %s\t To client : ", buff);
		bzero(buff, MAXLINE);
		n = 0;
		// copy server message in the buffer
		while ((buff[n++] = getchar()) != '\n')
			;

		// and send that buffer to client
		write(connfd, buff, sizeof(buff));

		// if msg contains "Exit" then server exit and chat ended.
		if (strncmp("exit", buff, 4) == 0) {
			printf("Server Exit...\n");
			break;
		}
	}
}

void receive_data(int sockfd) {
	char *buff = calloc(MAXLINE, sizeof(char));
	size_t buffer_size = MAXLINE * sizeof(char);
	// receive data and send ack
	uint64_t start, end;

	uint64_t total_bytes_recvd = 0;

	while(1) {
		start = get_current_time();
		ssize_t read_bytes = read(sockfd, buff, buffer_size);
		// printf("Got %ld bytes of data\n", read_bytes);
		ssize_t sent_bytes = write(sockfd, ack, sizeof(ack));
		end = get_current_time();

		// printf("Got %ld bytes of data and sent back ack in %lu microseconds\n", read_bytes, end - start);
	}
}

// Driver function
int main()
{
	int sockfd, connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	// Binding newly created socket to given IP and verification
	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
		printf("socket bind failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully binded..\n");

	// Now server is ready to listen and verification
	if ((listen(sockfd, 5)) != 0) {
		printf("Listen failed...\n");
		exit(0);
	}
	else
		printf("Server listening..\n");
	len = sizeof(cli);

	// Accept the data packet from client and verification
	connfd = accept(sockfd, (SA*)&cli, &len);
	if (connfd < 0) {
		printf("server accept failed...\n");
		exit(0);
	}
	else
		printf("server accept the client...\n");

	// Function for chatting between client and server
	receive_data(connfd);

	// After chatting close the socket
	close(sockfd);
}
