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

using namespace std;

struct kv_request {
    uint64_t type;
    string key;
    string value;
};

void parse_request(char* buff, struct kv_request& req) {
    // Request format--> [type, key length, key, optional[value length, value]]
    uint64_t *type = (uint64_t*)buff;
    req.type = *type;
    // cout << "type: " << req.type << endl;
    uint64_t *key_len = (uint64_t*)(buff + sizeof(uint64_t));
    // cout << "key_len: " << *key_len << endl;
    char *key = (char*)(buff + 2*sizeof(uint64_t));
    req.key = string(key);
    // cout << "key: " << req.key << endl;

    if(req.type == 1) {
        uint64_t *value_len = (uint64_t*)(buff + 2*sizeof(uint64_t) + *key_len);
        req.value = string(buff + 3*sizeof(uint64_t) + *key_len);
    }
}

uint64_t generate_request(char* req, bool final = false) {
	uint64_t len = 0;
	uint64_t key_len = (rand() % 10) + 1;
	uint64_t* req_ptr = (uint64_t*)req;
	
	if(final) *req_ptr = UINT64_MAX;
	else *req_ptr = rand() % 2; // 0 - get, 1 - put

	req_ptr[1] = key_len;

	char* key_ptr = (char*)(req_ptr + 2);
	for(int i = 0; i < key_len; i++) {
		key_ptr[i] = 'a' + (rand() % 26);
	}
	key_ptr[key_len - 1] = '\0';
	// cout << "Key: " << key_ptr << endl;

	len = sizeof(uint64_t) * 2 + key_len;
	
	uint64_t type = *req_ptr;
	if(type == 1) {
		uint64_t value_len = rand() % 10 + 1;
		len += sizeof(uint64_t) + value_len;
		req_ptr = (uint64_t*)(key_ptr + key_len);
		*req_ptr = value_len;
		char* value_ptr = (char*)(req_ptr + 1);
		for(int i = 0; i < value_len; i++) {
			value_ptr[i] = 'a' + (rand() % 26);
		}
		value_ptr[value_len - 1] = '\0';
		// cout << "Value: " << value_ptr << endl;
	}

	// cout << "Length: " << len << endl;
	
    return len;
}


void print_request(struct kv_request& req) {
    std::cout << "type: " << req.type << ", key: " << req.key << ", value: " << req.value << std::endl;
}


#endif