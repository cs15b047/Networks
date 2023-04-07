#include "utils.h"


using namespace std;

unordered_map<uint64_t, uint64_t> kv_store;

uint64_t handle_request(struct kv_request& req) {
    // 0 - get, 1 - put
    if(req.type == 0) {
        return kv_store[req.key];
    }
    else if(req.type == 1) {
        kv_store[req.key] = req.value;
        return 0;
    }
    return UINT64_MAX;
}

void process_requests(int sockfd) {
    struct kv_request req;
    uint64_t resp;
    for (;;) {
        int read_bytes = rread(sockfd, &req, sizeof(struct kv_request));
        // print_request(req);
        if(req.type == -1) break;
        resp = handle_request(req);
        rwrite(sockfd, &resp, sizeof(resp));
    }
    rclose(sockfd);
}


int main(int argc, char** argv) {

    kv_store.clear();

    int sockfd, connfd;
	socklen_t len;
	struct sockaddr_in servaddr, cli;

	// socket create and verification
	sockfd = rsocket(AF_INET, SOCK_STREAM, 0);
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
    if ((rbind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded..\n");

    vector<thread> threads;
    threads.clear();

    while(1) {
        // Now server is ready to listen and verification
        if ((rlisten(sockfd, 5)) != 0) {
            printf("Listen failed...\n");
            exit(0);
        }
        else
            printf("Server listening..\n");
        len = sizeof(cli);

        // Accept the data packet from client and verification
        connfd = raccept(sockfd, (SA*)&cli, &len);
        if (connfd < 0) {
            printf("server accept failed...\n");
            exit(0);
        }
        else
            printf("server accept the client...\n");

        // Function for chatting between client and server

        thread conn_thread(process_requests, connfd);
        threads.push_back(move(conn_thread));
    }

    for (auto &t : threads) {
        t.join();
    }

}