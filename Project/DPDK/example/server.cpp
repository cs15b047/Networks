/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "bits/stdc++.h"
#include "utils.h"
#define MAX_CLIENTS 2
using namespace std;

std::hash<int64_t> hasher;
vector<int> client_indices;
char ack_msg[] = "ACK";

struct client_info {
    int64_t data_len = -1;
    int64_t bytes_remaining = -1;
    int64_t total_elements_recvd = 0;
    vector<int64_t> data;
};

typedef struct client_info client_info_t;

vector<client_info_t> client_data;

int64_t create_five_tuple_hash(struct rte_ether_hdr *eth_h,
                               struct rte_ipv4_hdr *ip_h,
                               struct rte_tcp_hdr *tcp_h) {
    int64_t hash_data = 0;
    hash_data = eth_h->src_addr.addr_bytes[0] + eth_h->src_addr.addr_bytes[1] +
                eth_h->src_addr.addr_bytes[2] + eth_h->src_addr.addr_bytes[3] +
                eth_h->src_addr.addr_bytes[4] + eth_h->src_addr.addr_bytes[5];
    hash_data += eth_h->dst_addr.addr_bytes[0] + eth_h->dst_addr.addr_bytes[1] +
                 eth_h->dst_addr.addr_bytes[2] + eth_h->dst_addr.addr_bytes[3] +
                 eth_h->dst_addr.addr_bytes[4] + eth_h->dst_addr.addr_bytes[5];
    hash_data += ip_h->src_addr + ip_h->dst_addr;
    hash_data += tcp_h->src_port + tcp_h->dst_port;

    return hasher(hash_data);
}

static void print_vector(int64_t *data, size_t len) {
    int n = (len > 10) ? 50 : len;
    printf("Printing top %d elements of vector: \n", n);
    for (int i = 0; i < n; i++) {
        printf("%ld ", data[i]);
    }
    printf("\n");
}

void process_client(int64_t client_id, int64_t *local_data,
                    int payload_length) {
    // printf("Hash value: %ld\n", client_id);
    // check if hash value is in the client_indices else add
    if (std::find(client_indices.begin(), client_indices.end(), client_id) ==
        client_indices.end()) {
        client_info_t new_client_info;

        if (client_indices.size() == MAX_CLIENTS) {
            client_indices.clear();
            client_data.clear();
        }
        client_indices.push_back(client_id);
        client_data.push_back(new_client_info);
        printf("New client added with client id: %ld\n", client_id);
    }

    int client_idx =
        std::find(client_indices.begin(), client_indices.end(), client_id) -
        client_indices.begin();
    client_info_t *client_info = &client_data[client_idx];
    if (client_info->data_len == -1) {
        client_info->data_len = local_data[0];
        client_info->data.resize(client_info->data_len);
        client_info->bytes_remaining =
            local_data[0] * sizeof(client_info->data[0]);
    } else {
        int64_t *data_ptr = client_info->data.data();
        int64_t elements_recvd =
            MIN(payload_length, client_info->bytes_remaining) /
            sizeof(client_info->data[0]);
        memcpy(data_ptr + client_info->total_elements_recvd, local_data,
               elements_recvd * sizeof(client_info->data[0]));
        client_info->total_elements_recvd += elements_recvd;
        client_info->bytes_remaining -= payload_length;
        // printf("Received %ld elements for client %d\n",
        // client_info->total_elements_recvd, client_idx);
        if (client_info->total_elements_recvd == client_info->data_len) {
            printf("Received all elements for client %d\n", client_idx);
            print_vector(data_ptr, client_info->data_len);
            client_info->total_elements_recvd = 0;
            client_info->data_len = -1;
            client_info->bytes_remaining = -1;
        }
    }
}

struct rte_mbuf *create_ack(struct rte_ether_hdr *eth_h,
                            struct rte_ipv4_hdr *ip_h,
                            struct rte_tcp_hdr *tcp_h) {
    struct rte_mbuf *ack;
    // char *buf_ptr;
    struct rte_ether_hdr *eth_h_ack;
    struct rte_ipv4_hdr *ip_h_ack;
    struct rte_tcp_hdr *tcp_h_ack;

    ack = rte_pktmbuf_alloc(mbuf_pool);
    if (ack == NULL) {
        printf("Error allocating tx mbuf\n");
        return NULL;
    }
    size_t header_size = 0;
    uint8_t *ptr = rte_pktmbuf_mtod(ack, uint8_t *);

    /* add in an ethernet header */
    eth_h_ack = (struct rte_ether_hdr *)ptr;
    set_eth_hdrs(eth_h_ack, &eth_h->src_addr);
    ptr += sizeof(*eth_h_ack);
    header_size += sizeof(*eth_h_ack);

    /* add in ipv4 header*/
    ip_h_ack = (struct rte_ipv4_hdr *)ptr;
    set_ipv4_hdrs(ip_h_ack, ip_h->dst_addr, ip_h->src_addr, ack_len);
    header_size += sizeof(*ip_h_ack);
    ptr += sizeof(*ip_h_ack);

    uint32_t ack_seq = rte_be_to_cpu_32(tcp_h->sent_seq);
    /* add in tcp hdr*/
    tcp_h_ack = (struct rte_tcp_hdr *)ptr;
    set_tcp_response_hdrs(tcp_h_ack, ip_h_ack, tcp_h->dst_port, tcp_h->src_port,
                          ack_seq);
    header_size += sizeof(*tcp_h_ack);
    ptr += sizeof(*tcp_h_ack);

    set_payload(ptr, ack, ack_len, header_size, (int64_t *)ack_msg);
    return ack;
}

void check_numa() {
	uint16_t port;
    /*
     * Check that the port is on the same NUMA node as the polling thread
     * for best performance.
     */
    RTE_ETH_FOREACH_DEV(port)
    if (rte_eth_dev_socket_id(port) >= 0 &&
        rte_eth_dev_socket_id(port) != (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node to "
               "polling thread.\n\tPerformance will "
               "not be optimal.\n",
               port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
}

int extract_headers(struct rte_mbuf *pkt, struct rte_ether_hdr *&eth_h,
                    struct rte_ipv4_hdr *&ip_h, struct rte_tcp_hdr *&tcp_h) {
    eth_h = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    if (eth_h->ether_type != rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4)) {
        return -1;
    }

    ip_h = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
                                   sizeof(struct rte_ether_hdr));

    tcp_h = rte_pktmbuf_mtod_offset(pkt, struct rte_tcp_hdr *,
                                    sizeof(struct rte_ether_hdr) +
                                        sizeof(struct rte_ipv4_hdr));
    return 0;
}

/* Basic forwarding application lcore. 8< */
static void lcore_main(void) {
    uint16_t port;
    struct rte_mbuf *bufs[BURST_SIZE];
    struct rte_mbuf *pkt;
    struct rte_ether_hdr *eth_h;
    struct rte_ipv4_hdr *ip_h;
    struct rte_tcp_hdr *tcp_h;
    uint8_t i;
    int ret;
    uint8_t nb_replies = 0;
    struct sockaddr_in src, dst;
    int tcp_port_id;
    int64_t *local_data = NULL;
    size_t payload_length = 0;

    struct rte_mbuf *acks[BURST_SIZE];
    struct rte_mbuf *ack;

    check_numa();
    /* Main work of application loop. 8< */
    for (;;) {
        RTE_ETH_FOREACH_DEV(port) {
            if (port != 1)
                continue;

            const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
            if (unlikely(nb_rx == 0))
                continue;

            // Process received packets
            for (i = 0; i < nb_rx; i++) {
                pkt = bufs[i];
                local_data = NULL;
                payload_length = 0;

                tcp_port_id =
                    parse_packet(&src, &dst, &local_data, &payload_length, pkt);
                if (tcp_port_id == 0) {
                    printf("Ignoring Bad MAC packet\n");
                    rte_pktmbuf_free(pkt);
                    continue;
                }

                ret = extract_headers(pkt, eth_h, ip_h, tcp_h);
                if (ret < 0) {
                    rte_pktmbuf_free(pkt);
                    continue;
                }

                int64_t client_id = create_five_tuple_hash(eth_h, ip_h, tcp_h);
                process_client(client_id, local_data, payload_length);

                ack = create_ack(eth_h, ip_h, tcp_h);
                if (ack == NULL) {
                    printf("Error allocating tx mbuf\n");
                    return;
                }

                acks[nb_replies++] = ack;
                rte_pktmbuf_free(bufs[i]);
            }

            if (nb_replies > 0) {
                rte_eth_tx_burst(port, 0, acks, nb_replies);
            }
        }
    }
}

void setup_server(int argc, char *argv[]) {
    // struct rte_mempool *mbuf_pool;
    unsigned nb_ports = 1;
    uint16_t portid;

    /* Initializion the Environment Abstraction Layer (EAL). 8< */

    int ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    /* >8 End of initialization the Environment Abstraction Layer (EAL). */

    argc -= ret;
    argv += ret;

    nb_ports = rte_eth_dev_count_avail();
    /* Allocates mempool to hold the mbufs. 8< */
    mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    /* >8 End of allocating mempool to hold mbuf. */

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initializing all ports. 8< */
    RTE_ETH_FOREACH_DEV(portid)
    if (portid == 1 && port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", portid);
    /* >8 End of initializing all ports. */

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int main(int argc, char *argv[]) {

    setup_server(argc, argv);
    /* Call lcore_main on the main core only. Called on single lcore. 8< */
    lcore_main();
    /* >8 End of called on single lcore. */

    /* clean up the EAL */
    rte_eal_cleanup();

    return 0;
}
