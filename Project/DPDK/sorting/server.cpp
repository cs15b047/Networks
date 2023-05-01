#include "server.h"


struct partition_info {
    int64_t data_len = -1;
    int64_t bytes_remaining = -1;
    int64_t total_elements_recvd = 0;
    vector<int64_t> data;
};
typedef struct partition_info partition_info_t;

vector<int> partition_indices;
vector<partition_info_t> partition_data;

void process_data(struct rte_ether_hdr *eth_h,
                    struct rte_ipv4_hdr *ip_h,
                    struct rte_tcp_hdr *tcp_h,
                    int64_t *local_data,
                    int payload_length) {
    int64_t partition_id = create_five_tuple_hash(eth_h, ip_h, tcp_h);
    if (std::find(partition_indices.begin(), partition_indices.end(), partition_id) ==
        partition_indices.end()) {
        partition_info_t empty_partition_info;

        if (partition_indices.size() == MAX_CLIENTS) {
            partition_indices.clear();
            partition_data.clear();
        }
        partition_indices.push_back(partition_id);
        partition_data.push_back(empty_partition_info);
        printf("New client added with client id: %ld\n", partition_id);
    }

    int partition_idx =
        std::find(partition_indices.begin(), partition_indices.end(), partition_id) -
        partition_indices.begin();
    partition_info_t *partition_info = &partition_data[partition_idx];
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
            printf("Received all elements for client %d\n", partition_idx);
            print_vector(data_ptr, partition_info->data_len);
            partition_info->total_elements_recvd = 0;
            partition_info->data_len = -1;
            partition_info->bytes_remaining = -1;
        }
    }
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[]) {

    int ret = ServerSetup(argc, argv);
	if (ret < 0) {
		printf("Error setting up server\n");
		return ret;
	}
    ServerStart();
	ServerStop();

    return 0;
}
