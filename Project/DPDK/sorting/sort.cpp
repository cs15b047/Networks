#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "sort_utils.h"
#include "include/master.h"
#include "include/worker.h"

using namespace std;

vector<string> ip_addr;


struct partition_info {
    int64_t data_len = -1;
    int64_t bytes_remaining = -1;
    int64_t total_elements_recvd = 0;
    vector<int64_t> data;
    bool received = false;
};
typedef struct partition_info partition_info_t;

vector<int> partition_indices;
vector<partition_info_t> all_partition_data;
partition_info_t own_partition_data;

void process_data(struct rte_ether_hdr *eth_h,
                    struct rte_ipv4_hdr *ip_h,
                    struct rte_tcp_hdr *tcp_h,
                    int64_t *local_data,
                    int payload_length)
{

}
int MasterNode(int argc, char *argv[]) {
    int ret = MasterSetup(argc, argv);
	if (ret < 0) {
		printf("Error setting up master\n");
		return ret;
	}
    MasterStart();
	MasterStop();

    return 0;
}



static void WorkerStart() {
    send_partition(own_partition_data.data);
    print_stats();
}


int WorkerNode(int argc, char *argv[]) {
    int ret = WorkerSetup(argc, argv, own_partition_data.data_len);
    if (ret < 0) {
        printf("Error setting up master\n");
        return ret;
    }
    WorkerStart();
    WorkerStop();
    return 0;
}




void send_partition(vector<Record*>& partition_starts, vector<int> partition_sizes, int rank, int num_workers) {
    cout << "Step 3: Sending partition pieces to all workers" << endl;
    for(int dst_rank = 0; dst_rank < num_workers; dst_rank++) {
        if(dst_rank == rank) continue;
        // cout << "Sending partition to rank " << dst_rank << endl;
        // cout << ret << endl;
        // int ret = client_xchange_metadata_with_server((char *)partition_starts[dst_rank], (size_t)(partition_sizes[dst_rank] * sizeof(Record)), conn_state[dst_rank]);
        // cout << ret << endl;
        // ret = client_remote_memory_ops(conn_state[dst_rank]);
        // cout << ret << endl;
        // Send size first, then the partition
        cout << "Sending partition of size " << partition_sizes[dst_rank] << " to rank " << dst_rank << endl;
    }
    cout << "All partitions sent" << endl;
}



vector<int> receive_partitions(int num_workers, vector<Record>& merged_arr, uint64_t recv_ptr) {
    vector<int> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    cout << "Accepted all client connections, start receiving partitions" << endl;
    for(int i = 1; i < num_workers; i++) {
        // struct Client* client = clients[i];
        
        uint32_t partition_size = 0, bytes_to_recv;
        // int ret = send_server_metadata_to_client((char *) (merged_arr.data() + recv_ptr), bytes_to_recv, client);
        partition_size = bytes_to_recv / sizeof(Record);
        partition_sizes[i] = partition_size;
        recv_ptr += (uint64_t)partition_size;
        // cout << "Received partition of size " << partition_size << endl;
        // cout << "Recv_ptr = " << recv_ptr << endl;
    }
    cout << "Received all partitions" << endl;
    cout << "Recv ptr: " << recv_ptr << ", Merged array size = " << merged_arr.size() << endl;
    assert (recv_ptr <= merged_arr.size());
    merged_arr.resize(recv_ptr);

    return partition_sizes;
}


long get_time(chrono::high_resolution_clock::time_point start, chrono::high_resolution_clock::time_point end) {
    return chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

// Step 0 - Generate random input
void step0(vector<Record> &partition, long partition_size) {
    auto start = chrono::high_resolution_clock::now();

    generate_random_input(partition, partition_size);
    
    auto gen_random_input_end = chrono::high_resolution_clock::now();

    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << partition.size() << endl;

    cout << "Step 1- Starting local sort" << endl;
}

// Step 1- sort local data
void step1(vector<Record> &partition) {
    auto sort_start = chrono::high_resolution_clock::now();
    sort(partition.begin(), partition.end());
    auto sort_end = chrono::high_resolution_clock::now();
    cout << "Step 1- Local sort done" << endl;
}

// Step 2 - Divide the data into num_workers partitions based on data value
void step2(vector<Record> &partition, long partition_size, int num_workers,
           vector<Record*>& partition_starts, vector<int>& partition_sizes) {
    auto partition_start = chrono::high_resolution_clock::now();
    partition_data(partition, partition_starts, partition_sizes, num_workers);
    auto partition_end = chrono::high_resolution_clock::now();
    assert(verify_partitioning(partition_sizes, partition_size));
    auto verify_partitioning_end = chrono::high_resolution_clock::now();
    cout << "Step 2- Partitioning done" << endl;
}

void step3(vector<Record*>& partition_starts, vector<int>& partition_sizes, int N, int rank, int num_workers, vector<Record>& local_partition) {
     cout << "Step 3a- Connection setup done" << endl;
    // Step 3.1 - Send data to other workers

    auto shuffle_start = chrono::high_resolution_clock::now();
    thread send_thread = thread([&]() {
        send_partition(partition_starts, partition_sizes, rank, num_workers);
    });

    // Step 3.2 - Receive data from other workers
    uint64_t new_size = 2 * N/num_workers;
    // change partition to only contain local data:
    local_partition = vector<Record>(partition_starts[rank], partition_starts[rank] + partition_sizes[rank]);
    uint64_t local_size = local_partition.size();
    // print_partition(local_partition);
    // cout << "Local partition size: " << local_size << endl;
    local_partition.resize(new_size);
    auto resize_end = chrono::high_resolution_clock::now();
    vector<int> partition_sizes_recv = receive_partitions(num_workers, local_partition, local_size);
    auto shuffle_recv_end = chrono::high_resolution_clock::now();

    send_thread.join();
    auto shuffle_end = chrono::high_resolution_clock::now();
}

// Step 4- merge all partitions
void step4(vector<Record> &local_partition, vector<int> &partition_sizes_recv, int num_workers, vector<Record> &result) {
    uint64_t local_size_recv = local_partition.size();
    result.resize(local_size_recv);
    auto merge_start = chrono::high_resolution_clock::now();
    merge(local_partition, partition_sizes_recv, num_workers, result);
    auto merge_end = chrono::high_resolution_clock::now();
    cout << "Step 4- Merge done" << endl;
    assert(verify_sorted(result));
    auto verify_sorted_end = chrono::high_resolution_clock::now();
}

// void print_stats() {
//     cout << "Local sort: " << get_time(sort_start, sort_end) << endl;
//     cout << "Shuffle: total: " << get_time(shuffle_start, shuffle_end) << ", recv: " << get_time(shuffle_start, shuffle_recv_end) << ", resize: " << get_time(shuffle_start, resize_end) << endl;
//     cout << "Merge: " << get_time(merge_start, merge_end) << endl;
//     cout << "Total time: " << get_time(start, merge_end) << " ms" << endl;
// }

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
    // if(num_workers > 1) setup_server(rank);

    vector<Record> partition(partition_size), local_partition, result;
    vector<Record*> partition_starts;
    vector<int> partition_sizes;

    // Step 0 - Generate random input
    step0(partition, partition_size);

    // Step 1- sort local data
    step1(partition);

    // Step 2 - Divide the data into num_workers partitions based on data value
    step2(partition, partition_size, num_workers, partition_starts, partition_sizes);

    // Step 3 - Send data to other workers
    step3(partition_starts, partition_sizes, N, rank, num_workers, local_partition);

    // Step 4- merge all partitions
    step4(local_partition, partition_sizes, num_workers, result);


    cout << "Exiting process " << rank << endl;
}