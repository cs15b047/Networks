/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "bits/stdc++.h"
#include "utils.h"
#include <cmath>

using namespace std;

#define LATENCY_STATS 0
#define OVERALL_LATENCY_STATS 1
#define BANDWIDTH_STATS 2
#define MULTI_FLOW_BANDWIDTH_STATS 3

struct rte_mbuf *pkts_recv_buffer[TCP_WINDOW_LEN];
struct rte_mbuf *pkts_send_buffer[TCP_WINDOW_LEN];
struct rte_mbuf *pkt;

uint64_t packets_recvd, packets_sent;
uint64_t total_packets_sent[FLOW_NUM], total_packets_recvd[FLOW_NUM];
bool flow_completed[FLOW_NUM];

sliding_info window[FLOW_NUM];

timer_info *timer;
parsed_packet_info *packet_infos;
struct timer_info overall_time;


struct rte_mbuf *create_packet(uint32_t seq_num, size_t port_id, int64_t *data,
                               size_t pkt_len, struct rte_ether_addr *dst_mac) {
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_tcp_hdr *tcp_hdr;

    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
    if (pkt == NULL) {
        printf("Error allocating tx mbuf\n");
        return NULL;
    }
    size_t header_size = 0;
    uint8_t *ptr = rte_pktmbuf_mtod(pkt, uint8_t *);
    /* add in an ethernet header */
    eth_hdr = (struct rte_ether_hdr *)ptr;
    set_eth_hdrs(eth_hdr, dst_mac);
    ptr += sizeof(*eth_hdr);
    header_size += sizeof(*eth_hdr);

    /* add in ipv4 header*/
    ipv4_hdr = (struct rte_ipv4_hdr *)ptr;
    set_ipv4_hdrs(ipv4_hdr, rte_cpu_to_be_32(0), rte_cpu_to_be_32(0), pkt_len);
    ptr += sizeof(*ipv4_hdr);
    header_size += sizeof(*ipv4_hdr);

    /* add in tcp header*/
    tcp_hdr = (struct rte_tcp_hdr *)ptr;
    set_tcp_request_hdrs(tcp_hdr, ipv4_hdr, port_id, seq_num);
    ptr += sizeof(*tcp_hdr);
    header_size += sizeof(*tcp_hdr);

    set_payload(ptr, pkt, pkt_len, header_size, data);

    return pkt;
}

void send_packet(size_t port_id, int64_t *data, size_t data_len, struct  rte_ether_addr *dst_mac) {
    // CREATE PACKETS
    int64_t num_packets = 0, starting_seq_num = -1;
    int64_t *data_ptr = data;
    size_t bytes_sent = 0;

    while (bytes_sent < data_len && window[port_id].next_seq < NUM_PACKETS &&
           window[port_id].next_seq - window[port_id].last_recv_seq <
               TCP_WINDOW_LEN) {
        int64_t seq_num = window[port_id].next_seq;
        pkt = create_packet(seq_num, port_id, data_ptr, packet_len, dst_mac);
        data_ptr += packet_len / sizeof(data_ptr[0]);
        bytes_sent += packet_len;
        pkts_send_buffer[num_packets] = pkt;

        window[port_id].next_seq++;
        num_packets++;

        if (starting_seq_num == -1) {
            starting_seq_num = seq_num;
        }
    }

    if (num_packets > 0) {
        // SEND PACKETS
        uint64_t start_time = raw_time();
        uint64_t packets_sent =
            rte_eth_tx_burst(1, 0, pkts_send_buffer, num_packets);
        // printf("Flow: %u, Sent packets : %u\n", port_id,
        // total_packets_sent[port_id]);
        total_packets_sent[port_id] += packets_sent;
        for (int64_t i = 0; i < num_packets; i++) {
            int64_t seq_num = starting_seq_num + i;
            timer[seq_num].start_time = start_time;
        }
    }
}

void process_packets(uint16_t num_recvd, struct rte_mbuf **pkts,
                     parsed_packet_info *packet_infos) {
    // printf("Received burst of %u\n", (unsigned)num_recvd);
    struct rte_tcp_hdr *tcp_h;

    for (int i = 0; i < num_recvd; i++) {
        struct sockaddr_in src, dst;
        int64_t *payload = NULL;
        size_t payload_length = 0;
        int f_num =
            parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);

        tcp_h = rte_pktmbuf_mtod_offset(pkts[i], struct rte_tcp_hdr *,
                                        sizeof(struct rte_ether_hdr) +
                                            sizeof(struct rte_ipv4_hdr));
        if (f_num != 0) {
            rte_pktmbuf_free(pkts[i]);
            packet_infos[i].flow_num = f_num - 1;
            packet_infos[i].ack_num = rte_be_to_cpu_32(tcp_h->recv_ack);
        } else {
            printf("Ignoring bad MAC packet\n");
            packet_infos[i].flow_num = -1;
        }
    }
}

void receive_packets() {
    uint64_t packets_recvd =
        rte_eth_rx_burst(1, 0, pkts_recv_buffer, TCP_WINDOW_LEN);
    uint64_t end_time = raw_time();

    if (packets_recvd > 0) {

        // PROCESS PACKETS
        process_packets(packets_recvd, pkts_recv_buffer, packet_infos);
        for (uint64_t f = 0; f < packets_recvd; f++) {
            if (packet_infos[f].flow_num != -1) {

                window[packet_infos[f].flow_num].last_recv_seq++;

                total_packets_recvd[packet_infos[f].flow_num]++;
                timer[packet_infos[f].ack_num].end_time = end_time;
            }
        }
    }
}

bool all_flows_completed(bool *flow_completed) {
    for (int i = 0; i < FLOW_NUM; i++) {
        if (!flow_completed[i])
            return false;
    }

    return true;
}

void init_window() {
    for (int i = 0; i < FLOW_NUM; i++) {
        window[i].next_seq = 0;
        window[i].last_recv_seq = 0;
        total_packets_sent[i] = 0;
        total_packets_recvd[i] = 0;
        flow_completed[i] = false;
    }
}

void print_stats() {
    for (int i = 0; i < FLOW_NUM; i++) {
        printf("Flow %u - Packets Sent: %lu, Packets Received: %lu\n", i,
               total_packets_sent[i], total_packets_recvd[i]);
    }

    // calculate bandwidth
    uint64_t overall_time_in_ns =
        overall_time.end_time - overall_time.start_time;
    double overall_time_in_ms = overall_time_in_ns / 1000000.0;
    double bandwidth_in_mbps =
        (FLOW_SIZE * 8 * FLOW_NUM) / (overall_time_in_ms * 1000);

    printf("Bandwidth: %f Mbps\n", bandwidth_in_mbps);
    // calculate latencies
    uint64_t max_latency = 0, min_latency = INT32_MAX, avg_latency = 0,
             total_latency = 0;
    if (FLOW_NUM == 1) {
        for (uint32_t pkt = 0; pkt < NUM_PACKETS; pkt++) {
            uint64_t latency_in_ns =
                timer[pkt].end_time - timer[pkt].start_time;
            total_latency += latency_in_ns;
            if (latency_in_ns > max_latency) {
                max_latency = latency_in_ns;
            }
            if (latency_in_ns < min_latency) {
                min_latency = latency_in_ns;
            }
        }
        avg_latency = total_latency / NUM_PACKETS;
        printf("Latency: Max: %f ms, Min: %f ms, Avg: %f ms\n",
               max_latency / 1000000.0, min_latency / 1000000.0,
               avg_latency / 1000000.0);
    }
}

static void send_partition(vector<int64_t> &partition, struct rte_ether_addr *dst_mac, int worker_rank) {
    int64_t partition_len = partition.size();
    vector<int64_t> partition_id_info = {worker_rank, partition_len};
    timer = (timer_info *)malloc(NUM_PACKETS * sizeof(timer_info));
    packet_infos = (parsed_packet_info *)malloc(TCP_WINDOW_LEN *
                                                sizeof(parsed_packet_info));
    size_t port_id = 0;

    init_window();
    printf("Starting main loop\n");
    overall_time.start_time = raw_time();
    while (!all_flows_completed(flow_completed)) {
        if (window[port_id].last_recv_seq < NUM_PACKETS) {
            send_packet(port_id, partition_id_info.data(), sizeof(partition_id_info[0]) * partition_id_info.size(), dst_mac);
            send_packet(port_id, partition.data(), sizeof(partition[0]) * partition_len, dst_mac);
            // POLL ON RECEIVE PACKETS
            receive_packets();
        } else {
            flow_completed[port_id] = true;
        }

        port_id = (port_id + 1) % FLOW_NUM;
    }
    overall_time.end_time = raw_time();

    free(packet_infos);
    free(timer);
}

int WorkerSetup(int argc, char *argv[], int64_t data_len) {
    setbuf(stdout, NULL);
    unsigned nb_ports;
    uint16_t portid;

    FLOW_SIZE = data_len * sizeof(int64_t);
    // packet_len = (packet_len < FLOW_SIZE) ? packet_len: FLOW_SIZE;
    NUM_PACKETS = 1 + ceil(FLOW_SIZE * 1.0 / packet_len);

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

    return 0;
}


void WorkerStop() {
    rte_eal_cleanup();
    printf("Exiting..\n");
    exit(0);
}
