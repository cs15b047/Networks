#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "rdma_utils.h"
#include "rdma_client_utils.h"
#include "sort_utils.h"

using namespace std;

vector<string> ip_addr;
vector<Connection*> conn_state;
vector<Client*> clients;

struct sockaddr_in generate_server_info(int64_t rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT + rank);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return serv_addr;
}

// rank runs on ip_addr[rank % ip_addr.size()] server
struct sockaddr_in get_server_info(int64_t rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_RDMA_PORT + rank);
    int64_t idx = rank % ip_addr.size();
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr[idx].c_str());
    return serv_addr;
}



void setup_client(int64_t dst_rank, struct Connection* conn_state) {
    struct sockaddr_in serv_addr = get_server_info(dst_rank);
    int64_t ret = client_prepare_connection(&serv_addr, conn_state);
    ret = client_pre_post_recv_buffer(conn_state);
    // cout << ret << endl;
}

void send_partition(vector<Record*>& partition_starts, vector<int64_t> partition_sizes, int64_t rank, int64_t num_workers) {
    cout << "Step 3: Sending partition pieces to all workers" << endl;
    for(int64_t dst_rank = 0; dst_rank < num_workers; dst_rank++) {
        if(dst_rank == rank) continue;

        char* buffer = (char *)partition_starts[dst_rank];
        size_t buffer_size = (size_t)((size_t)partition_sizes[dst_rank] * sizeof(Record));
        struct Connection* conn = conn_state[dst_rank];
        // cout << "Sending partition to rank " << dst_rank << endl;
        // cout << ret << endl;
        int64_t ret = client_xchange_metadata_with_server(buffer, buffer_size, conn);
        // cout << ret << endl;
        ret = client_remote_memory_ops(conn);
        // cout << ret << endl;
        // Send size first, then the partition
        cout << "Sending partition of size " << partition_sizes[dst_rank] << " to rank " << dst_rank << endl;
    }
    cout << "All partitions sent" << endl;

    // Cleanup
    for(int64_t i = 0; i < num_workers; i++) {
        if(i == rank) continue;
        int64_t ret = client_cleanup(conn_state[i]);
        cout << "Client cleanup: rc = " << ret << endl;
    }
}

void setup_server(int64_t rank) {
    struct sockaddr_in serv_addr = generate_server_info(rank);
    cout << "Starting server" << endl;
    int64_t ret = start_rdma_server(&serv_addr);
}

vector<int64_t> receive_partitions(int64_t num_workers, vector<Record>& merged_arr, uint64_t recv_ptr) {
    vector<int64_t> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    cout << "Accepted all client connections, start receiving partitions" << endl;
    for(int64_t i = 1; i < num_workers; i++) {
        struct Client* client = clients[i];
        
        uint32_t partition_size = 0, bytes_to_recv;
        int64_t ret = send_server_metadata_to_client((char *) (merged_arr.data() + recv_ptr), bytes_to_recv, client);
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

void setup_server_connection(int64_t num_workers) {
    cout << "Step 2: Setting up server connections" << endl;
    clients.resize(num_workers, NULL);
    for(int64_t i = 1; i < num_workers; i++) {
        clients[i] = new Client();
        struct Client* client = clients[i];
        cout << "Setting up client " << i << endl;
        int64_t ret = setup_client_resources(client);
        cout << "Client setup: " << ret << endl;
    }
    for(int64_t i = 1; i < num_workers; i++) {
        int64_t ret = accept_client_connection(clients[i]);
        cout << "Accepted client connection " << i << endl;
    }
    cout << "Accepted all client connections" << endl;
}

void setup_client_connection(int64_t num_workers, int64_t rank) {
    cout << "Workers: " << num_workers << ", rank: " << rank << endl;
    cout << "Step 2: Setting up client connections" << endl;
    conn_state.resize(num_workers, NULL);
    for(int64_t i = 0; i < num_workers; i++) {
        conn_state[i] = new Connection();
    }
    for(int64_t dst_rank = 0; dst_rank < num_workers; dst_rank++){
        if(dst_rank == rank) continue;
        cout << "Setting up client connection to server " << dst_rank << endl;
        setup_client(dst_rank, conn_state[dst_rank]);
        int64_t ret = client_connect_to_server(conn_state[dst_rank]);
        cout << "Connected to server " << dst_rank << endl;
    }
    cout << "Connected to all servers" << endl;
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
    int64_t rank, num_workers;
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

    vector<Record> partition(partition_size);
    generate_random_input(partition, partition_size);
    
    auto gen_random_input_end = chrono::high_resolution_clock::now();

    cout << "Random input generated. Starting partition sort" << endl;
    cout << "Partition size = " << partition.size() << endl;

    thread server_conn_thread = thread([&](){
        if(num_workers > 1) setup_server_connection(num_workers);
    });
    thread client_conn_thread = thread([&](){
        if(num_workers > 1) setup_client_connection(num_workers, rank);
    });

    cout << "Step 1- Starting local sort" << endl;

    // Step 1- sort local data
    auto sort_start = chrono::high_resolution_clock::now();
    sort(partition.begin(), partition.end());
    auto sort_end = chrono::high_resolution_clock::now();
    cout << "Step 1- Local sort done" << endl;

    // Step 2 - Divide the data into num_workers partitions based on data value
    vector<Record*> partition_starts;
    vector<int64_t> partition_sizes;
    auto partition_start = chrono::high_resolution_clock::now();
    partition_data(partition, partition_starts, partition_sizes, num_workers);
    auto partition_end = chrono::high_resolution_clock::now();
    assert(verify_partitioning(partition_sizes, partition_size));
    auto verify_partitioning_end = chrono::high_resolution_clock::now();
    cout << "Step 2- Partitioning done" << endl;

    server_conn_thread.join();
    client_conn_thread.join();
    cout << "Step 3a- Connection setup done" << endl;

    // Step 3.1 - Send data to other workers
    auto shuffle_start = chrono::high_resolution_clock::now();
    thread send_thread = thread([&]() {
        send_partition(partition_starts, partition_sizes, rank, num_workers);
    });

    // Step 3.2 - Receive data from other workers
    uint64_t new_size = (uint64_t) (1.1 * (double) N/num_workers);
    // change partition to only contain local data:
    vector<Record> local_partition = vector<Record>(partition_starts[rank], partition_starts[rank] + partition_sizes[rank]);
    uint64_t local_size = local_partition.size();
    // print_partition(local_partition);
    // cout << "Local partition size: " << local_size << endl;
    local_partition.resize(new_size);
    auto resize_end = chrono::high_resolution_clock::now();
    vector<int64_t> partition_sizes_recv = receive_partitions(num_workers, local_partition, local_size);
    auto shuffle_recv_end = chrono::high_resolution_clock::now();

    send_thread.join();
    auto shuffle_end = chrono::high_resolution_clock::now();

    partition.clear();
    partition.shrink_to_fit();

    // Step 4- merge all partitions
    uint64_t local_size_recv = local_partition.size();
    vector<Record> result(local_size_recv);
    auto merge_start = chrono::high_resolution_clock::now();
    merge(local_partition, partition_sizes_recv, num_workers, result);
    auto merge_end = chrono::high_resolution_clock::now();
    cout << "Step 4- Merge done" << endl;
    assert(verify_sorted(result));
    auto verify_sorted_end = chrono::high_resolution_clock::now();

    cout << "Local sort: " << get_time(sort_start, sort_end) << endl;
    cout << "Shuffle: total: " << get_time(shuffle_start, shuffle_end) << ", recv: " << get_time(shuffle_start, shuffle_recv_end) << ", resize: " << get_time(shuffle_start, resize_end) << endl;
    cout << "Merge: " << get_time(merge_start, merge_end) << endl;
    cout << "Total time: " << get_time(start, merge_end) << " ms" << endl;

    cout << "Exiting process " << rank << endl;
}