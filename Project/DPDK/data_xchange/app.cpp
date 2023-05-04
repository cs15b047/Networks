
#include "include/worker.h"
#include <unistd.h>

struct partition_info {
    int64_t data_len = -1;
    int64_t bytes_remaining = -1;
    int64_t total_elements_recvd = 0;
    vector<int64_t> data;
    bool received = false;
};

typedef struct partition_info partition_info_t;
unordered_map<int64_t, int> partition_map;
vector<partition_info_t> all_partition_data;
partition_info_t own_partition_data;
int num_workers;
int own_rank;

void process_data(struct rte_ether_hdr *eth_h,
                    struct rte_ipv4_hdr *ip_h,
                    struct rte_tcp_hdr *tcp_h,
                    int64_t *local_data,
                    int payload_length) {
    int64_t hash_val = create_five_tuple_hash(eth_h, ip_h, tcp_h);
    if (partition_map.find(hash_val) == partition_map.end()) {
        partition_map[hash_val] = local_data[0];
        printf("New worker added with worker id: %ld\n", hash_val);
    }

    int partition_idx = partition_map[hash_val];
    printf("Received %ld bytes for partition %d\n", payload_length, partition_idx);
}


static void WorkerStart() {
    printf("Worker %d starting in 5 seconds\n", own_rank);
    sleep(5);
    for(int i = 0; i < num_workers; i++) {
        if (i != own_rank) {
            send_partition(own_partition_data.data, &worker_macs[i], own_rank);
        }
    }
    while(1) {
        uint16_t port;
        RTE_ETH_FOREACH_DEV(port) {
            if(port != 1)
                continue;
         receive_packets(port);
        }
    }
    print_stats();
}


int WorkerNode(int argc, char *argv[]) {
    int ret = WorkerSetup(argc, argv);
    if (ret < 0) {
        printf("Error setting up master\n");
        return ret;
    }
    WorkerStart();
    WorkerStop();
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: ./sort <flow size gb> <rank> <num workers>\n");
        return -1;
    }
    setbuf(stdout, NULL);
    
    int64_t data_len = (int64_t)atoi(argv[1]);
    own_rank = atoi(argv[2]);
    num_workers = atoi(argv[3]);

    own_partition_data.data_len = data_len;
    own_partition_data.data.resize(data_len);
    own_partition_data.received = true;
    generate_random_data(own_partition_data.data);
    //sort own_partition_data.data
    sort(own_partition_data.data.begin(), own_partition_data.data.end());
    WorkerNode(argc, argv);
    // if (rank == 0) {
    //     partition_indices.push_back(rank);
    //     all_partition_data.push_back(own_partition_data);
    //     return MasterNode(argc, argv);
    // } else {
    //     return WorkerNode(argc, argv);
    // }
}