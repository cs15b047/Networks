#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

string ip_addr;

void generate_random_input(vector<int>& partition, int partition_size) {
    // Generate random input
    for (int i = 0; i < partition_size; i++) {
        partition[i] = rand() % INT_MAX;
    }
}

struct sockaddr_in get_server_info(){
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(5000);
    serv_addr.sin_addr.s_addr = inet_addr("10.10.1.3");
    return serv_addr;
}

void print_partition(vector<int>& partition) {
    for(int i = 0; i < partition.size(); i++)
        cout << i << " -> " << partition[i] << endl;
}

void send_partition(vector<int>& partition, int partition_size, int rank) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = get_server_info();
    connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    // Send size first, then the partition
    cout << "Sending partition of size " << partition_size << " to rank 0" << endl;
    send(sockfd, (const void *) &partition_size, sizeof(int), 0);
    int sent_bytes = send(sockfd, (const void *)partition.data(), (size_t)(partition_size * sizeof(int)), 0);
    
    cout << "Sent " << sent_bytes/sizeof(int) << " elements" << endl;
}

vector<int> receive_partitions(int num_workers, vector<int>& merged_arr, int recv_ptr) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = get_server_info();
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        exit(1);
    }
    listen(sockfd, num_workers * 10);

    vector<int> partition_sizes(num_workers);
    partition_sizes[0] = recv_ptr;

    for(int i = 1; i < num_workers; i++) {
        int addr_size = sizeof(serv_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *) &serv_addr, (socklen_t *)&addr_size);
        int partition_size = 0;
        int recvd_bytes1 = recv(newsockfd, (void *) &partition_size, sizeof(int), 0);
        long bytes_to_recv = partition_size * sizeof(int);
        cout << "Receiving " << bytes_to_recv << " bytes" << endl;
        while (bytes_to_recv > 0) {
            long recvd_bytes = recv(newsockfd, (void *) (merged_arr.data() + recv_ptr), (size_t)(bytes_to_recv), MSG_WAITALL);
            bytes_to_recv -= recvd_bytes;
            cout << "Received " << recvd_bytes/sizeof(int) << " elements" << endl;
            recv_ptr += recvd_bytes/sizeof(int);
        }
        assert(recvd_bytes1 == sizeof(int));
        partition_sizes[i] = partition_size;
        cout << "Received partition of size " << partition_size << endl;
        cout << "Recv_ptr = " << recv_ptr << endl;
    }
    cout << "Merged array size = " << merged_arr.size() << endl;
    assert(recv_ptr == merged_arr.size());

    return partition_sizes;
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

    if(argc != 5) {
        cout << "Usage: ./sort <num_workers> <N> <ip_addr> <rank>" << endl;
        exit(1);
    }
    srand(time(NULL));
    int num_workers = atoi(argv[1]);
    long N = atol(argv[2]);
    ip_addr = string(argv[3]);
    int rank = atoi(argv[4]);

    int partition_size = N / num_workers;
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
        send_partition(partition, partition_size, rank);
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
    }

    cout << "Exiting process " << rank << endl;
}