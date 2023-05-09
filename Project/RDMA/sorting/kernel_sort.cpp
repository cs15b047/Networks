#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "sort_utils.h"
#include "socket_utils.h"

using namespace std;

struct Connection{
    int sockfd;
    struct sockaddr_in addr;
};

int server_fd;
long N;
int my_rank, num_workers;
string ip_addr_str;
vector<string> ip_addr;
vector<Connection*> conn_state;
vector<Connection*> clients;

void init_params(int argc, char* argv[]) {
    srand(time(NULL));
    num_workers = atoi(argv[1]);
    N = atol(argv[2]);
    ip_addr.clear();
    ip_addr_str = string(argv[3]);
    stringstream ss(ip_addr_str);
    while(ss.good()) {
        string substr;
        getline(ss, substr, ',');
        ip_addr.push_back(substr);
    }
    my_rank = atoi(argv[4]);
}

struct sockaddr_in generate_server_info(int rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_PORT + rank);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    return serv_addr;
}

// rank runs on ip_addr[rank % ip_addr.size()] server
struct sockaddr_in get_server_info(int srv_rank){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_PORT + srv_rank);
    int idx = srv_rank % ip_addr.size();
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr[idx].c_str());
    return serv_addr;
}

void setup_client(int dst_rank, struct Connection* conn_state) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = get_server_info(dst_rank);
    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Error connecting to server with rank: " << dst_rank << endl;
        exit(1);
    }
    conn_state->sockfd = sockfd;
    conn_state->addr = serv_addr;
}

uint64_t send_data(int sock, char* data, uint64_t size) {
    uint64_t total_sent = 0, mxm = 1000000000;
    int64_t sent = 0;
    while(total_sent < size) {
        sent = send(sock, data + total_sent, min(mxm, (int64_t)size - total_sent), MSG_WAITALL);
        if(sent < 0) cout << "Error: " << errno << endl;
        total_sent += sent;
        cout << "Sent " << sent << " bytes, Total: " << total_sent << endl;
    }
    return total_sent;
}

void send_partition(vector<Record*>& partition_starts, vector<int> partition_sizes) {
    cout << "Step 3: Sending partition pieces to all workers" << endl;
    for(int dst_rank = 0; dst_rank < num_workers; dst_rank++) {
        if(dst_rank == my_rank) continue;
        uint64_t size_to_send = (size_t)(partition_sizes[dst_rank] * sizeof(Record));
        int sent1 = send(conn_state[dst_rank]->sockfd, &size_to_send, sizeof(uint64_t), 0);
        cout << sent1 << endl;
        cout << "Sending " << size_to_send << " bytes to rank " << dst_rank << endl;
        uint64_t ret = send_data(conn_state[dst_rank]->sockfd, (char *)partition_starts[dst_rank], size_to_send);
        cout << "Expected to send: " << size_to_send << ", actually sent: " << ret << endl;
        assert(ret == size_to_send);
        cout << "Sent " << ret << " bytes to rank " << dst_rank << endl;
        // // Send size first, then the partition
        // cout << "Sent " << ret << " bytes to rank " << dst_rank << endl;
    }
    cout << "All partitions sent" << endl;
}

int setup_server() {
    struct sockaddr_in serv_addr = generate_server_info(my_rank);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int tr = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int));
    if(bind(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cout << "Bind failed" << endl;
        exit(0);
    }
    cout << "Starting server" << endl;
    int ret = listen(fd, 100);
    if(ret < 0) {
        cout << "Listen failed" << endl;
        exit(0);
    }
    return fd;
}

void recv_data(int sockfd, char* data, uint64_t size) {
    uint64_t recvd = 0, total_recvd = 0;
    while(total_recvd < size) {
        cout << "Waiting for recv"  << endl;
        recvd = recv(sockfd, data + total_recvd, size - total_recvd, MSG_WAITALL);
        cout << "Recvd" << endl;
        total_recvd += recvd;
        cout << "Received " << recvd << " bytes, Total: " << total_recvd << ", Expected Size: " << size << endl;
    }
}

vector<int> receive_partitions(vector<Record>& merged_arr, uint64_t recv_ptr) {
    vector<int> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    cout << "Accepted all client connections, start receiving partitions" << endl;
    int j = 1;
    for(int i = 0; i < num_workers; i++) {
        if (i == my_rank) continue;
        struct Connection* client = clients[i];

        // Receive size first, then the partition
        uint64_t partition_size = 0, bytes_to_recv = 0, total_bytes_recvd = 0;
        recv(client->sockfd, &bytes_to_recv, sizeof(uint64_t), 0);
        cout << "Receiving " << bytes_to_recv << " bytes from rank " << i << endl;
        
        // Receive from specific client
        recv_data(client->sockfd, (char *)(merged_arr.data() + recv_ptr), bytes_to_recv);
        partition_size = bytes_to_recv / sizeof(Record);
        cout << "Received " << bytes_to_recv << " bytes = " << partition_size << " records from rank " << i << endl;
        partition_sizes[j] = partition_size;
        j++;
        recv_ptr += (uint64_t)partition_size;
    }
    cout << "Received all partitions" << endl;
    cout << "Recv ptr: " << recv_ptr << ", Merged array size = " << merged_arr.size() << endl;
    assert (recv_ptr <= merged_arr.size());
    merged_arr.resize(recv_ptr);

    return partition_sizes;
}

void setup_server_connection() {
    // cout << "Step 2: Setting up server connections" << endl;
    clients.resize(num_workers, NULL);

    vector<thread> conn_threads(num_workers);
    for(int i = 0; i < num_workers; i++) {
        if (i == my_rank) continue;
        clients[i] = new Connection();
        struct Connection* client = clients[i];

        // Accept connection from client - in serial order! - TODO: change to parallel
        conn_threads[i] = thread([client, i]{
            int addr_size = sizeof(struct sockaddr_in);
            int newsockfd = accept(server_fd, (struct sockaddr *) &(client->addr), (socklen_t *)&addr_size);
            if(newsockfd < 0) {
                cout << "Error accepting client connection" << endl;
                exit(1);
            }
            client->sockfd = newsockfd;
        });
    }
    for(int i = 0; i < num_workers; i++) {
        if (i == my_rank) continue;
        conn_threads[i].join();
    }
    cout << "Accepted all client connections" << endl;
}

void setup_client_connection() {
    // cout << "Step 2: Setting up client connections" << endl;
    conn_state.resize(num_workers, NULL);
    for(int i = 0; i < num_workers; i++) {
        conn_state[i] = new Connection();
    }
    vector<thread> conn_threads;
    for(int dst_rank = 0; dst_rank < num_workers; dst_rank++){
        if(dst_rank == my_rank) continue;
        struct Connection* conn = conn_state[dst_rank];
        thread conn_thread = thread([dst_rank, conn]{
            // cout << "Setting up client connection to server " << dst_rank << endl;
            setup_client(dst_rank, conn);
            // cout << "Connected to server " << dst_rank << endl;
        });
        conn_threads.push_back(move(conn_thread));
    }
    for(int i = 0; i < conn_threads.size(); i++) {
        conn_threads[i].join();
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
    init_params(argc, argv);

    long partition_size = N / num_workers;
    if (my_rank == num_workers - 1) {
        partition_size += (N % num_workers);
    }

    // Start server to receive data from other workers
    server_fd = -1;
    if(num_workers > 1) server_fd = setup_server();

    // Step 0 - Generate random input

    vector<Record> partition(partition_size);
    generate_random_input(partition, partition_size);
    
    auto gen_random_input_end = chrono::high_resolution_clock::now();

    // cout << "Random input generated. Starting partition sort" << endl;
    // cout << "Partition size = " << partition.size() << endl;


    // cout << "Step 1- Starting local sort" << endl;

    // Step 1- sort local data
    auto start = chrono::high_resolution_clock::now();
    auto local_ops_start = chrono::high_resolution_clock::now();
    auto sort_start = chrono::high_resolution_clock::now();
    sort(partition.begin(), partition.end());
    auto sort_end = chrono::high_resolution_clock::now();
    // cout << "Step 1- Local sort done" << endl;
    
    thread server_conn_thread = thread([&](){
        if(num_workers > 1) setup_server_connection();
    });
    thread client_conn_thread = thread([&](){
        if(num_workers > 1) setup_client_connection();
    });

    // Step 2 - Divide the data into num_workers partitions based on data value
    vector<Record*> partition_starts;
    vector<int> partition_sizes;
    auto partition_start = chrono::high_resolution_clock::now();
    partition_data(partition, partition_starts, partition_sizes, num_workers);
    auto partition_end = chrono::high_resolution_clock::now();
    assert(verify_partitioning(partition_sizes, partition_size));
    auto verify_partitioning_end = chrono::high_resolution_clock::now();
    cout << "Step 2- Partitioning done" << endl;

    server_conn_thread.join();
    client_conn_thread.join();
    auto local_ops_end = chrono::high_resolution_clock::now();
    cout << "Step 3a- Connection setup done" << endl;

    // Step 3.1 - Send data to other workers
    auto shuffle_start = chrono::high_resolution_clock::now();
    thread send_thread = thread([&]() {
        send_partition(partition_starts, partition_sizes);
    });
    cout << "Step 3- Receive data from other workers" << endl;
    // Step 3.2 - Receive data from other workers
    uint64_t new_size = (uint64_t)((double)1.1 * (double)N/num_workers);
    // change partition to only contain local data:
    vector<Record> local_partition = vector<Record>(partition_starts[my_rank], partition_starts[my_rank] + partition_sizes[my_rank]);
    uint64_t local_size = local_partition.size();
    local_partition.resize(new_size);
    auto resize_end = chrono::high_resolution_clock::now();
    vector<int> partition_sizes_recv = receive_partitions(local_partition, local_size);
    auto shuffle_recv_end = chrono::high_resolution_clock::now();

    send_thread.join();
    auto shuffle_end = chrono::high_resolution_clock::now();

    auto mem_ops_start = chrono::high_resolution_clock::now();
    partition.clear();
    partition.shrink_to_fit();

    // Step 4- merge all partitions
    uint64_t local_size_recv = local_partition.size();
    // vector<Record> result(local_size_recv);
    auto mem_ops_end = chrono::high_resolution_clock::now();

    auto merge_start = chrono::high_resolution_clock::now();
    // merge(local_partition, partition_sizes_recv, num_workers, result);
    auto merge_end = chrono::high_resolution_clock::now();
    cout << "Step 4- Merge done" << endl;
    // assert(verify_sorted(result));
    auto verify_sorted_end = chrono::high_resolution_clock::now();

    close(server_fd);

    cout << "Local ops: " << get_time(local_ops_start, local_ops_end) << endl;
    cout << "\tLocal sort: " << get_time(sort_start, sort_end) << endl;
    cout << "Shuffle: total: " << get_time(shuffle_start, shuffle_end) << ", \n\trecv: " << get_time(shuffle_start, shuffle_recv_end) << ", resize: " << get_time(shuffle_start, resize_end) << endl;
    cout << "Memory ops: " << get_time(mem_ops_start, mem_ops_end) << endl;
    cout << "Merge: " << get_time(merge_start, merge_end) << endl;
    cout << "Total time: " << get_time(start, merge_end) << " ms" << endl;

    cout << "Exiting process " << my_rank << endl;
}