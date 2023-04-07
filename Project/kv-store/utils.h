#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h> // struct sockaddr_in
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <rdma/rsocket.h>

#define SERV_PORT 8080
#define SERV_ADDR "128.110.217.58"
#define MAXLINE 32768
#define SA struct sockaddr

struct kv_request {
    uint64_t type;
    uint64_t key;
    uint64_t value;
};

void print_request(struct kv_request& req) {
    std::cout << "type: " << req.type << ", key: " << req.key << ", value: " << req.value << std::endl;
}


#endif