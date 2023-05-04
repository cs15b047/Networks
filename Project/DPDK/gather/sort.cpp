#include "include/master.h"
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

vector<int> partition_indices;
vector<partition_info_t> all_partition_data;
partition_info_t own_partition_data;

size_t own_rank, num_workers;

bool all_partitions_received() {
    if (all_partition_data.size() < num_workers) {
        return false;
    }

    for (size_t i = 0; i < num_workers; i++) {
        if (!all_partition_data[i].received) {
            return false;
        }
    }
    return true;
}

void process_data(struct rte_ether_hdr *eth_h,
                    struct rte_ipv4_hdr *ip_h,
                    struct rte_tcp_hdr *tcp_h,
                    int64_t *local_data,
                    int payload_length) {
    int64_t partition_id = create_five_tuple_hash(eth_h, ip_h, tcp_h);
    if (std::find(partition_indices.begin(), partition_indices.end(), partition_id) ==
        partition_indices.end()) {
        partition_info_t empty_partition_info;

        if (partition_indices.size() == num_workers) {
            partition_indices.clear();
            all_partition_data.clear();
        }
        partition_indices.push_back(partition_id);
        all_partition_data.push_back(empty_partition_info);
        printf("New worker added with worker id: %ld\n", partition_id);
    }

    int partition_idx =
        std::find(partition_indices.begin(), partition_indices.end(), partition_id) -
        partition_indices.begin();
    partition_info_t *partition_info = &all_partition_data[partition_idx];
    if (partition_info->data_len == -1) {
        partition_info->data_len = local_data[0];
        partition_info->data.resize(partition_info->data_len);
        partition_info->bytes_remaining =
            local_data[0] * sizeof(partition_info->data[0]);
    } else {
        int64_t *data_ptr = partition_info->data.data();
        int64_t elements_recvd =
            MIN(payload_length, partition_info->bytes_remaining) /
            sizeof(partition_info->data[0]);
        memcpy(data_ptr + partition_info->total_elements_recvd, local_data,
               elements_recvd * sizeof(partition_info->data[0]));
        partition_info->total_elements_recvd += elements_recvd;
        partition_info->bytes_remaining -= payload_length;

        if (partition_info->total_elements_recvd == partition_info->data_len) {
            partition_info->received = true;
            partition_info->total_elements_recvd = 0;
            partition_info->data_len = -1;
            partition_info->bytes_remaining = -1;
        }
    }

    // if all elements in all_partition_data have received, then sort
    if(all_partitions_received()) {
        printf("All partitions received\n");
        vector<int64_t> all_data;
        for (size_t i = 0; i < num_workers; i++) {
            all_data.insert(all_data.end(), all_partition_data[i].data.begin(),
                            all_partition_data[i].data.end());
        }
        sort(all_data.begin(), all_data.end());
        if (verify_sorted(all_data)) {
            printf("Data is sorted\n");
        } else {
            printf("Data is not sorted\n");
        }
        MasterStop();
    }
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
    
    for(size_t i = 0; i < num_workers; i++) {
        if (i != own_rank) {
            send_partition(own_partition_data.data, &worker_macs[i], own_rank);
        }
    }

    // send_partition(own_partition_data.data);
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


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: ./sort <flow size gb> <rank> <num_workers>\n");
        return -1;
    }
    
    int64_t data_len = (int64_t)atoi(argv[1]);;
    own_rank = atoi(argv[2]);
    num_workers = atoi(argv[3]);

    own_partition_data.data_len = data_len;
    own_partition_data.data.resize(data_len);
    own_partition_data.received = true;
    generate_random_data(own_partition_data.data);
    //sort own_partition_data.data
    sort(own_partition_data.data.begin(), own_partition_data.data.end());
    printf("Worker %ld starting in 5 seconds\n", own_rank);
    sleep(5);
    
    if (own_rank == 0) {
        partition_indices.push_back(own_rank);
        all_partition_data.push_back(own_partition_data);
    }

    thread t1(MasterNode, argc, argv);
    thread t2(WorkerNode, argc, argv);

    t1.join();
    t2.join();
}