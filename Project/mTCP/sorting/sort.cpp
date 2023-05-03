#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "utils.h"

#include <common/common.h>
#include "utils.h"

using namespace std;

size_t num_workers;
long N;
vector<int> own_partition;
vector<vector<int>> all_partitions;
static std::mutex mtx;
bool sorting_started = false;

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    srand(time(NULL));
    for (int i = 0; i < partition_size; i++) {
        partition[i] = rand() % 100;
    }
}

struct sockaddr_in get_server_info(){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    return serv_addr;
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

void print_partition(vector<int>& partition) {
    for(size_t i = 0; i < partition.size(); i++) {
        cout << partition[i] << " ";
    }
    cout << endl;
}


void print_partitions(vector<vector<int>>& partitions) {
    for(size_t i = 0; i < partitions.size(); i++) {
        cout << "Partition " << i << ": " << endl;
        print_partition(partitions[i]);
    }
}


vector<int> merge_partitions_2(vector<vector<int>>& all_partitions) {
    size_t k = all_partitions.size();
    vector<size_t> indices(k, 0);
    size_t total_elements = 0;
    for (size_t i = 0; i < k; i++) {
        total_elements += all_partitions[i].size();
    }
    
    vector<int> result(total_elements);
    size_t min_index;
    int min_value;
    for (size_t i = 0; i < total_elements; i++) {
        min_index = -1;
        min_value = INT_MAX;
        for (size_t j = 0; j < k; j++) {
            if (indices[j] < all_partitions[j].size() && all_partitions[j][indices[j]] < min_value) {
                min_index = j;
                min_value = all_partitions[j][indices[j]];
            }
        }
        result[i] = min_value;
        indices[min_index]++;
    }
    return result;
}

vector<int> merge_partitions(vector<vector<int>>& all_partitions) {
    // vector<int> output_vec = accumulate(all_partitions.begin(), all_partitions.end(), vector<int>());
    // sort(output_vec.begin(), output_vec.end());
    // return output_vec;

    vector<int> output_vec;
    for (const auto& vec : all_partitions) {
        copy(vec.begin(), vec.end(), back_inserter(output_vec));
    }
    sort(output_vec.begin(), output_vec.end());
    return output_vec;

}


static inline int 
ClientWrite(thread_context_t ctx, int sockid) {
    struct mtcp_epoll_event ev;
	int wr;
	int len = sizeof(int);
    size_t partition_size = own_partition.size();

    // Send partition size first, then the partition
	wr = mtcp_write(ctx->mctx, sockid, (char*) &partition_size, len);
	if (wr < len) {
		TRACE_ERROR("Socket %d: Sending HTTP request failed. "
				"try: %d, sent: %d\n", sockid, len, wr);
	}
    printf("Sent %d bytes\n", wr);


    len = own_partition.size() * sizeof(int);
    long bytes_to_send = len;
    long send_ptr = 0;
    while (bytes_to_send > 0) {
        do {
           wr = mtcp_write(ctx->mctx, sockid, (char*) (own_partition.data() + send_ptr), bytes_to_send);
        } while (wr < 0 && errno == EAGAIN);
        
        if (wr < len) {
            TRACE_ERROR("Socket %d: Sending HTTP request failed. "
                    "try: %d, sent: %d\n", sockid, len, wr);
        }
        bytes_to_send -= wr;
        send_ptr += wr / sizeof(int);
        
    }

    printf("Sent %ld bytes\n", len - bytes_to_send);
	
	ev.events = MTCP_EPOLLIN;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

    CloseClientConnection(ctx, sockid);
    ClientStop();
	return 0;
}

static inline int
ClientRead(thread_context_t ctx, int sockid) {
    return 0;
}

static void SendResponse(thread_context_t ctx, int sockid, char *response) {
	struct mtcp_epoll_event ev;

	size_t len = strlen(response);
	
	mtcp_write(ctx->mctx, sockid, response, len);
	TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, sent);

	ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
	ev.data.sockid = sockid;
	mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);
}

static int 
ServerRead(struct thread_context *ctx, int sockid)
{
	int rd;
    size_t partition_size;
    int len = sizeof(int);

	rd = mtcp_read(ctx->mctx, sockid, (char *) &(partition_size), len);
	if (rd <= 0) {
		return rd;
	}

    int curr_index = all_partitions.size();
    all_partitions.push_back(vector<int>(partition_size));

    printf("Current Core: %d\n", ctx->core);
	printf("Received %d bytes\n", rd);

    len = partition_size * sizeof(int);
	long bytes_to_recv = len;
    long recv_ptr = 0;
	while (bytes_to_recv > 0) {
        do {
		    rd = mtcp_read(ctx->mctx, sockid, (char *) ((all_partitions[curr_index].data()) + recv_ptr), bytes_to_recv);
        } while (rd < 0 && errno == EAGAIN);

        if (rd <= 0) {
            printf("Error: %d, errno: %d\n", rd, errno);
            return rd;
        }
	
		bytes_to_recv -= rd;
		recv_ptr += rd / sizeof(int);
	}

    printf("Received %ld bytes\n", len - bytes_to_recv);
   
    if(all_partitions.size() == num_workers) {
        mtx.lock();
        if (!sorting_started) {
            sorting_started = true;
            cout << "All partitions received. Merging..." << endl;
            vector<int> result = merge_partitions(all_partitions);
            assert(verify_sorted(result));
            cout << "Merge successful" << endl;
        }
        mtx.unlock();
        CloseServerConnection(ctx, sockid);
        ServerStop();
    }

    char resp[] = "OK";
    SendResponse(ctx, sockid, resp);
	return rd;
}



int main(int argc, char *argv[]) {

    if(argc != 4) {
        cout << "Usage: ./sort <num_workers> <N> <rank>" << endl;
        exit(1);
    }
    srand(time(NULL));
    num_workers = atoi(argv[1]);
    N = atol(argv[2]);
    size_t rank = atoi(argv[3]);
	char *conf_file = const_cast<char*>("./conf/sort.conf");

    size_t partition_size = N / num_workers;
    if (rank + 1 == num_workers) {
        partition_size += (N % num_workers);
    }

    own_partition.resize(partition_size);
    generate_random_input(own_partition, partition_size);


    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << own_partition.size() << endl;

    // Sort the partition
    sort(own_partition.begin(), own_partition.end());

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
        all_partitions.push_back(own_partition);
        ServerStart();
        ServerStop();

        // // - Receive all the partitions
        // receive_partitions();
        
    }

    cout << "Exiting process " << rank << endl;
}