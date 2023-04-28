#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "rdma_utils.h"
#include "rdma_client_utils.h"

using namespace std;

string ip_addr;

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    generate(partition.begin(), partition.begin() + partition_size, []() {
        return rand() % INT_MAX;
    });
}

struct sockaddr_in generate_server_info(){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return serv_addr;
}

struct sockaddr_in get_server_info(string& ip_addr){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr.c_str());
    return serv_addr;
}

void print_partition(vector<int>& partition) {
    for(int i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i] << endl;
}

void setup_client() {
    struct sockaddr_in serv_addr = get_server_info(ip_addr);
    int ret = client_prepare_connection(&serv_addr);
    ret = client_pre_post_recv_buffer(); 
    cout << ret << endl;
}

void send_partition(vector<int>& partition, int partition_size, int rank, string& ip_addr) {
    setup_client();

    int ret = client_connect_to_server();
    cout << ret << endl;
    ret = client_xchange_metadata_with_server((char *)partition.data(), (size_t)(partition_size * sizeof(int)));
    cout << ret << endl;
    ret = client_remote_memory_ops();
    cout << ret << endl;
    // Send size first, then the partition
    cout << "Sending partition of size " << partition_size << " to rank 0" << endl;
}

void setup_server() {
    struct sockaddr_in serv_addr = generate_server_info();
    int ret = start_rdma_server(&serv_addr);
}

vector<int> receive_partitions(int num_workers, vector<int>& merged_arr, uint64_t recv_ptr) {
    if(num_workers > 1) setup_server();

    vector<int> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    vector<Client*> clients(num_workers, NULL);

    for(int i = 1; i < num_workers; i++) {
        clients[i] = new Client();
        struct Client* client = clients[i];
        int ret = setup_client_resources(client);
        cout << "Return value from setup_client_resources: " << ret << endl;
    }

    for(int i = 1; i < num_workers; i++) {
        int ret = accept_client_connection(clients[i]);
        cout << "Return value from accept_client_connection: " << ret << endl;
    }

    for(int i = 1; i < num_workers; i++) {
        struct Client* client = clients[i];
        
        uint32_t partition_size = 0, bytes_to_recv;
        int ret = send_server_metadata_to_client((char *) (merged_arr.data() + recv_ptr), bytes_to_recv, client);
        partition_size = bytes_to_recv / sizeof(int);
        partition_sizes[i] = partition_size;
        recv_ptr += (uint64_t)partition_size;
        cout << "Received partition of size " << partition_size << endl;
        cout << "Recv_ptr = " << recv_ptr << endl;
    }
    cout << "Merged array size = " << merged_arr.size() << endl;
    assert(recv_ptr == merged_arr.size());

    return partition_sizes;
}

void merge(vector<int>& merged_arr, vector<int>& partition_sizes, int num_workers, vector<int>& result) {
    long N = merged_arr.size();
    vector<long> partition_ptrs(num_workers);
    vector<long> cum_partition_sizes(num_workers);
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
    for(long i = 0; i < N; i++) {
        int min_val = INT_MAX;
        int min_idx = -1;
        // get the minimum value from each partition
        for(int j = 0; j < num_workers; j++) {
            long ptr = partition_ptrs[j];
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

    if(argc != 5) {
        cout << "Usage: ./sort <num_workers> <N> <ip_addr> <rank>" << endl;
        exit(1);
    }
    srand(time(NULL));
    int num_workers = atoi(argv[1]);
    long N = atol(argv[2]);
    ip_addr = string(argv[3]);
    int rank = atoi(argv[4]);

    long partition_size = N / num_workers;
    if (rank == num_workers - 1) {
        partition_size += (N % num_workers);
    }

    vector<int> partition(partition_size);
    generate_random_input(partition, partition_size);

    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << partition.size() << endl;

    // Sort the partition
    sort(partition.begin(), partition.end());

    cout << "Partition sorted" << endl;
    // Send the partition to the master- rank 0
    if(rank != 0){
        send_partition(partition, partition_size, rank, ip_addr);
        // client_disconnect_and_clean();
    } else {
        // Master rank 0 - allocate memory for the merged array
        partition.resize(N);
        // - Receive all the partitions
        vector<int> partition_sizes = receive_partitions(num_workers, partition, partition_size);
        // Merge the partitions instead of sorting whole array
        vector<int> result(N);
        cout << "Starting merge" << endl;
        merge(partition, partition_sizes, num_workers, result);

        assert(verify_sorted(result));
        // disconnect_and_cleanup();
    }

    cout << "Exiting process " << rank << endl;
}