#include "utils.h"


using namespace std;

unordered_map<string, string> kv_store;

string handle_request(struct kv_request& req) {
    // 0 - get, 1 - put
    if(req.type == 0) {
        if(kv_store.find(req.key) == kv_store.end()) {
            return "not found";
        }
        return kv_store[req.key];
    }
    else if(req.type == 1) {
        kv_store[req.key] = req.value;
        return "ok";
    }
    return "error";
}

void process_requests(int sockfd) {
    struct kv_request req;
    string resp;
    char buff[MAXLINE];
    for (;;) {
        int read_bytes = read(sockfd, buff, MAXLINE);
        parse_request(buff, req);
        // print_request(req);
        if(req.type == -1) break;
        resp = handle_request(req);
        write(sockfd, resp.c_str(), resp.length() + 1);
    }
    close(sockfd);
}


int main(int argc, char** argv) {

    kv_store.clear();

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

    vector<thread> threads;
    threads.clear();

    while(1) {
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

        thread conn_thread(process_requests, connfd);
        threads.push_back(move(conn_thread));
    }

    for (auto &t : threads) {
        t.join();
    }

}