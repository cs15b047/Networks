#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utils.h"

#include <common/common.h>
#include "utils.h"

using namespace std;


struct partition_info {
    size_t partition_size;
    vector<int> partition;
};

struct partition_info partition_info_global;
int num_workers;
size_t recv_ptr;
vector<int> partition_sizes;


void print_partition(vector<int>& partition) {
    for(size_t i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i] << endl;
}


static inline int 
ClientWrite(thread_context_t ctx, int sockid) {
    struct mtcp_epoll_event ev;
	int wr;
	int len = sizeof(int);

    // Send partition size first, then the partition
	wr = mtcp_write(ctx->mctx, sockid, (char*) &(partition_info_global.partition_size), len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
    printf("Sent %d bytes\n", wr);
    len = partition_info_global.partition_size * sizeof(int);

	wr = mtcp_write(ctx->mctx, sockid, (char*) (partition_info_global.partition.data()),len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
    printf("Sent %d bytes\n", wr);
	
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

	return 0;
}

static inline int
ClientRead(thread_context_t ctx, int sockid) {
    return 0;
}

static int 
ServerRead(struct thread_context *ctx, int sockid)
{
	struct mtcp_epoll_event ev;
	int rd, len, sent;

	rd = mtcp_read(ctx->mctx, sockid, (char *) &(partition_info_global.partition_size), sizeof(int));
	if (rd <= 0) {
		return rd;
	}

    partition_sizes.push_back(partition_info_global.partition_size);
	printf("Received %d bytes\n", rd);
	printf("Received Array Length: %ld\n", partition_info_global.partition_size);

	long bytes_to_recv = partition_info_global.partition_size * sizeof(int);
	while (bytes_to_recv > 0) {
		rd = mtcp_read(ctx->mctx, sockid, (char *) ((partition_info_global.partition.data()) + recv_ptr), bytes_to_recv);
		if (rd <= 0) {
			return rd;
		}

		bytes_to_recv -= rd;
		recv_ptr += rd / sizeof(int);
        printf("Received %d bytes\n", rd);
	}

    char response[100];
    strcpy(response, "Received partition");
	len = strlen(response);
	
	sent = mtcp_write(ctx->mctx, sockid, response, len);
	TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, sent);

	assert(sent == len);

	ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

    print_partition(partition_info_global.partition);
	return rd;
}

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    srand(time(NULL));
    for (int i = 0; i < partition_size; i++) {
        partition[i] = rand() % INT_MAX;
    }
}

struct sockaddr_in get_server_info(){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    return serv_addr;
}

void send_partition() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = get_server_info();
    connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    // Send size first, then the partition
    cout << "Sending partition of size " << partition_info_global.partition_size << " to rank 0" << endl;
    send(sockfd, (const void *) &(partition_info_global.partition_size), sizeof(int), 0);
    int sent_bytes = send(sockfd, (const void *)(partition_info_global.partition.data()), (size_t)(partition_info_global.partition_size * sizeof(int)), 0);
    
    cout << "Sent " << sent_bytes/sizeof(int) << " elements" << endl;
}

void receive_partitions() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = get_server_info();
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        exit(1);
    }
    listen(sockfd, 10);

    for(int i = 1; i < num_workers; i++) {
        int addr_size = sizeof(serv_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *) &serv_addr, (socklen_t *)&addr_size);
        int partition_size = 0;
        int recvd_bytes1 = recv(newsockfd, (void *) &partition_size, sizeof(int), 0);
        long bytes_to_recv = partition_size * sizeof(int);
        cout << "Receiving " << bytes_to_recv << " bytes" << endl;
        while (bytes_to_recv > 0) {
            long recvd_bytes = recv(newsockfd, (void *) ((partition_info_global.partition.data()) + recv_ptr), (size_t)(bytes_to_recv), MSG_WAITALL);
            bytes_to_recv -= recvd_bytes;
            cout << "Received " << recvd_bytes/sizeof(int) << " elements" << endl;
            recv_ptr += recvd_bytes/sizeof(int);
        }
        assert(recvd_bytes1 == sizeof(int));
        partition_sizes[i] = partition_size;
        cout << "Received partition of size " << partition_size << endl;
        cout << "Recv_ptr = " << recv_ptr << endl;
    }
    cout << "Merged array size = " << partition_info_global.partition.size() << endl;
    assert(recv_ptr == partition_info_global.partition.size());
}

void merge(vector<int>& merged_arr, vector<int>& partition_sizes, int num_workers, vector<int>& result) {
    int N = merged_arr.size();
    vector<int> partition_ptrs(num_workers);
    vector<int> cum_partition_sizes(num_workers);
    cum_partition_sizes[0] = partition_sizes[0];
    for(int i = 1; i < num_workers; i++) {
        cum_partition_sizes[i] = cum_partition_sizes[i - 1] + partition_sizes[i];
    }
    assert(cum_partition_sizes[num_workers - 1] == N);

    // assign pointers to the start of each partition
    partition_ptrs[0] = 0;
    for(int i = 1; i < num_workers; i++) {
        partition_ptrs[i] = cum_partition_sizes[i - 1];
    }

    // Perform a k-way merge (k = num_workers) using extra space
    for(int i = 0; i < N; i++) {
        int min_val = INT_MAX;
        int min_idx = -1;
        // get the minimum value from each partition
        for(int j = 0; j < num_workers; j++) {
            int ptr = partition_ptrs[j];
            if(ptr < cum_partition_sizes[j] && merged_arr[ptr] <= min_val) {
                min_val = merged_arr[ptr];
                min_idx = j;
            }
        }
        result[i] = min_val;
        partition_ptrs[min_idx]++;
    }
}

bool verify_sorted(vector<int>& arr) {
    int sz = arr.size();
    for(int i = 1; i < sz; i++) {
        if(arr[i - 1] > arr[i]) {
            cout << "Erring element: idx: " << i << " --> " << arr[i - 1] << " " << arr[i] << endl;
            return false;
        }
    }
    return true;
}


int main(int argc, char *argv[]) {

    if(argc != 4) {
        cout << "Usage: ./sort <num_workers> <N> <rank>" << endl;
        exit(1);
    }
    srand(time(NULL));
    num_workers = atoi(argv[1]);
    long N = atol(argv[2]);
    int rank = atoi(argv[3]);
	char *conf_file = const_cast<char*>("./conf/sort.conf");

    partition_info_global.partition_size = N / num_workers;
    if (rank == num_workers - 1) {
        partition_info_global.partition_size += (N % num_workers);
    }

    partition_info_global.partition.resize(partition_info_global.partition_size);
    generate_random_input(partition_info_global.partition, partition_info_global.partition_size);

    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << partition_info_global.partition.size() << endl;

    // Sort the partition
    sort(partition_info_global.partition.begin(), partition_info_global.partition.end());

    cout << "Partition sorted" << endl;
    // Send the partition to the master- rank 0
    if(rank != 0){
        char *host = const_cast<char*>(SERV_ADDR);
        ret = ClientSetup(host, SERV_PORT, conf_file);
        if (!ret) {
            TRACE_CONFIG("Failed to setup mtcp\n");
            exit(EXIT_FAILURE);
        }
        ClientStart();
        ClientStop();
    } else {
        int ret = ServerSetup(SERV_PORT, conf_file);
        if (!ret) {
            TRACE_CONFIG("Failed to setup mtcp\n");
            exit(EXIT_FAILURE);
        }

        recv_ptr = partition_info_global.partition_size;
        partition_sizes.push_back(partition_info_global.partition_size);
        partition_info_global.partition.resize(N);

        ServerStart();
        ServerStop();

        // // - Receive all the partitions
        // receive_partitions();
        // // Merge the partitions instead of sorting whole array
        // vector<int> result(N);
        // cout << "Starting merge" << endl;
        // merge(partition_info_global.partition, partition_sizes, num_workers, result);

        // assert(verify_sorted(result));
    }

    cout << "Exiting process " << rank << endl;
}