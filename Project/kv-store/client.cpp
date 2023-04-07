#include "utils.h"

using namespace std;

#define MAX 	 80
#define MAXLINE 32768
#define SA struct sockaddr

uint64_t get_current_time() {
	// use gettimeofday
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

struct kv_request generate_request() {
    struct kv_request req;
    req.type = rand() % 2;
    req.key = rand() % 100;
    if (req.type == 1) { // put
        req.value = rand() % 100;
    }
    return req;
}


void send_data(int sockfd) {
    char buff[MAXLINE];
    int buffer_size = MAXLINE;

	uint64_t total_time = 0, iters = 1024 * 256, total_bytes_sent = 0;

	for(int i = 0; i < iters; i++) {
		// send data
		uint64_t start, end;

        struct kv_request req = generate_request();
		if(i == iters - 1) req.type = -1; // exit signal

		start = get_current_time();
		ssize_t sent_bytes = rwrite(sockfd, &req, sizeof(req));
		int read_bytes = rread(sockfd, buff, buffer_size);
		uint64_t* resp = (uint64_t*)buff;
		end = get_current_time();
		total_time += end - start;
		total_bytes_sent += sent_bytes;
		// assert(read_bytes == 4);
		// assert(strcmp(buff, "ack\0") == 0);
		// printf("Sent + ack time: %lu\n", end - start);
	}

	double avg_latency = (double)total_time / (double)iters;

	cout << "Average latency: " << avg_latency << " microseconds" << endl;

	printf("Sent %ld bytes in %lu microseconds --> %lf GB/s\n", total_bytes_sent, total_time, (total_bytes_sent /(double)1e3)/(double)total_time);
}

int main()
{
	int sockfd, connfd;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = rsocket(AF_INET, SOCK_STREAM, 0);
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
	servaddr.sin_addr.s_addr = inet_addr(SERV_ADDR);
	servaddr.sin_port = htons(SERV_PORT);

	// connect the client socket to server socket
	if (rconnect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		printf("connection with the server failed...\n");
		exit(0);
	}
	else
		printf("connected to the server..\n");

	// function for chat
	send_data(sockfd);

	// close the socket
	rclose(sockfd);
}