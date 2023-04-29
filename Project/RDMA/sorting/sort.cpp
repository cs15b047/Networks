#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "rdma_utils.h"
#include "rdma_client_utils.h"

using namespace std;

vector<string> ip_addr;

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    generate(partition.begin(), partition.begin() + partition_size, []() {
        return rand() % INT_MAX;
    });
}

struct sockaddr_in generate_server_info(int rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT + rank);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return serv_addr;
}

struct sockaddr_in get_server_info(int rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT + rank);
    int idx = rank % ip_addr.size();
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr[idx].c_str());
    return serv_addr;
}

void print_partition(vector<int>& partition) {
    for(int i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i] << endl;
}

void setup_client(int dst_rank) {
    struct sockaddr_in serv_addr = get_server_info(dst_rank);
    int ret = client_prepare_connection(&serv_addr);
    ret = client_pre_post_recv_buffer(); 
    cout << ret << endl;
}

void send_partition(vector<int*>& partition_starts, vector<int> partition_sizes, int rank, int num_workers) {
    cout << "Step 3: Sending partition pieces to all workers" << endl;
    for(int dst_rank = 0; dst_rank < num_workers; dst_rank++) {
        if(dst_rank == rank) continue;
        cout << "Sending partition to rank " << dst_rank << endl;
        setup_client(dst_rank);
        int ret = client_connect_to_server();
        cout << ret << endl;
        ret = client_xchange_metadata_with_server((char *)partition_starts[dst_rank], (size_t)(partition_sizes[dst_rank] * sizeof(int)));
        cout << ret << endl;
        ret = client_remote_memory_ops();
        cout << ret << endl;
        // Send size first, then the partition
        cout << "Sending partition of size " << partition_sizes[dst_rank] << " to rank " << dst_rank << endl;
    }
}

void setup_server(int rank) {
    struct sockaddr_in serv_addr = generate_server_info(rank);
    cout << "Starting server" << endl;
    int ret = start_rdma_server(&serv_addr);
}

vector<int> receive_partitions(int num_workers, vector<int>& merged_arr, uint64_t recv_ptr) {
    vector<int> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    vector<Client*> clients(num_workers, NULL);
    cout << "Receiving partitions" << endl;

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
    cout << "Recv ptr: " << recv_ptr << ", Merged array size = " << merged_arr.size() << endl;
    assert (recv_ptr <= merged_arr.size());
    merged_arr.resize(recv_ptr);

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

bool verify_partitioning(vector<int>& partition_sizes, long N) {
    long sum = 0;
    int num_workers = partition_sizes.size();
    cout << "Num workers: " <<  num_workers << endl;
    for(int i = 0; i < num_workers; i++) {
        sum += (long)partition_sizes[i];
    }
    cout << "Sum of sizes: " << sum << ", Expected size: " << N << endl;
    return sum == N;
}

void partition_data(vector<int>& data, vector<int*>& partition_starts, vector<int>& partition_sizes, int num_workers) {
    partition_starts.resize(num_workers, NULL);
    partition_sizes.resize(num_workers, 0);

    int range_span = INT_MAX / num_workers;

    // range for worker i: [i * range_span, (i + 1) * range_span)
    // for worker num_workers - 1: [i * range_span, INT_MAX]
    int itr = 0;
    for(int i = 0; i < num_workers; i++) {
        int lower = i * range_span;
        int upper = (i == num_workers - 1) ? INT_MAX : (i + 1) * range_span;
        partition_starts[i] = &(data.data()[itr]);
        if(i == num_workers - 1) {
            partition_sizes[i] = (int)data.size() - itr;
        } else {
            partition_sizes[i] = upper_bound(data.begin() + itr, data.end(), upper) - (data.begin() + itr);
        }
        itr += partition_sizes[i];
    }
}

long get_time(chrono::high_resolution_clock::time_point start, chrono::high_resolution_clock::time_point end) {
    return chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

int main(int argc, char *argv[]) {

    if(argc != 5) {
        cout << "Usage: ./sort <num_workers> <N> <ip_addr> <rank>" << endl;
        exit(1);
    }
    long N;
    int rank, num_workers;
    srand(time(NULL));
    num_workers = atoi(argv[1]);
    N = atol(argv[2]);
    ip_addr.clear();
    string ip_addr_str = string(argv[3]);
    stringstream ss(ip_addr_str);
    while(ss.good()) {
        string substr;
        getline(ss, substr, ',');
        ip_addr.push_back(substr);
    }
    rank = atoi(argv[4]);

    // Ranks are attached 


    long partition_size = N / num_workers;
    if (rank == num_workers - 1) {
        partition_size += (N % num_workers);
    }

    // Start server to receive data from other workers
    if(num_workers > 1) setup_server(rank);

    // Step 0 - Generate random input
    auto start = chrono::high_resolution_clock::now();

    vector<int> partition(partition_size);
    generate_random_input(partition, partition_size);
    
    auto gen_random_input_end = chrono::high_resolution_clock::now();

    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << partition.size() << endl;

    // Step 1- sort local data
    auto sort_start = chrono::high_resolution_clock::now();
    sort(partition.begin(), partition.end());
    auto sort_end = chrono::high_resolution_clock::now();
    cout << "Step 1- Local sort done" << endl;

    // Step 2 - Divide the data into num_workers partitions based on data value
    vector<int*> partition_starts;
    vector<int> partition_sizes;
    auto partition_start = chrono::high_resolution_clock::now();
    partition_data(partition, partition_starts, partition_sizes, num_workers);
    auto partition_end = chrono::high_resolution_clock::now();
    assert(verify_partitioning(partition_sizes, partition_size));
    auto verify_partitioning_end = chrono::high_resolution_clock::now();
    cout << "Step 2- Partitioning done" << endl;

    // Step 3.1 - Send data to other workers
    auto shuffle_start = chrono::high_resolution_clock::now();
    thread send_thread = thread([&]() {
        send_partition(partition_starts, partition_sizes, rank, num_workers);
    });

    // Step 3.2 - Receive data from other workers
    uint64_t new_size = 2 * N/num_workers;
    // change partition to only contain local data:
    vector<int> local_partition = vector<int>(partition_starts[rank], partition_starts[rank] + partition_sizes[rank]);
    uint64_t local_size = local_partition.size();
    // print_partition(local_partition);
    cout << "Local partition size: " << local_size << endl;
    local_partition.resize(new_size);
    vector<int> partition_sizes_recv = receive_partitions(num_workers, local_partition, local_size);

    send_thread.join();
    auto shuffle_end = chrono::high_resolution_clock::now();

    // Step 4- merge all partitions
    uint64_t local_size_recv = local_partition.size();
    vector<int> result(local_size_recv);
    auto merge_start = chrono::high_resolution_clock::now();
    merge(local_partition, partition_sizes_recv, num_workers, result);
    auto merge_end = chrono::high_resolution_clock::now();
    assert(verify_sorted(result));
    auto verify_sorted_end = chrono::high_resolution_clock::now();

    cout << "Local sort: " << get_time(sort_start, sort_end) << endl;
    cout << "Shuffle: " << get_time(shuffle_start, shuffle_end) << endl;
    cout << "Merge: " << get_time(merge_start, merge_end) << endl;
    cout << "Total time: " << get_time(start, merge_end) << " ms" << endl;

    cout << "Exiting process " << rank << endl;
}