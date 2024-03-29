/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "../Utils/utils.h"


#define LATENCY_STATS 0
#define OVERALL_LATENCY_STATS 1
#define BANDWIDTH_STATS 2
#define MULTI_FLOW_BANDWIDTH_STATS 3


char *OUTPUT_DIR;
int ITER_NUM;
char *FILE_NAMES[5];

void write_to_file(char *filename, char *data, bool append) {
    FILE *fp;
    if (append) {
        fp = fopen(filename, "a");
    } else {
        fp = fopen(filename, "w");
    }
    if (fp == NULL) {
        printf("Error opening file %s for writing", filename);
    }
    fprintf(fp, "%s\n", data);
    fclose(fp);
}  


struct rte_mbuf *create_packet(uint32_t seq_num, size_t port_id) {
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_tcp_hdr *tcp_hdr;

    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
    if (pkt == NULL) {
        printf("Error allocating tx mbuf\n");
        return -EINVAL;
    }
    size_t header_size = 0;
    uint8_t *ptr = rte_pktmbuf_mtod(pkt, uint8_t *);
    /* add in an ethernet header */
    eth_hdr = (struct rte_ether_hdr *)ptr;
    set_eth_hdrs(eth_hdr, &DST_MAC);
    ptr += sizeof(*eth_hdr);
    header_size += sizeof(*eth_hdr);

    /* add in ipv4 header*/
    ipv4_hdr = (struct rte_ipv4_hdr *)ptr;
    set_ipv4_hdrs(ipv4_hdr, rte_cpu_to_be_32(DEFAULT_IP), rte_cpu_to_be_32(DEFAULT_IP), packet_len);
    ptr += sizeof(*ipv4_hdr);
    header_size += sizeof(*ipv4_hdr);

    /* add in tcp header*/
    tcp_hdr = (struct rte_tcp_hdr *)ptr;
    set_tcp_request_hdrs(tcp_hdr, ipv4_hdr, port_id, seq_num);
    ptr += sizeof(*tcp_hdr);
    header_size += sizeof(*tcp_hdr);

    set_payload(ptr, pkt, packet_len, header_size);

    return pkt;
}


void process_packets(uint16_t num_recvd, struct rte_mbuf **pkts, parsed_packet_info *packet_infos) {
    // printf("Received burst of %u\n", (unsigned)num_recvd);
    struct rte_tcp_hdr *tcp_h;
    

    for (int i = 0; i < num_recvd; i++) {
        struct sockaddr_in src, dst;
        void *payload = NULL;
        size_t payload_length = 0;
        int f_num = parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);

        tcp_h = rte_pktmbuf_mtod_offset(pkts[i], struct rte_tcp_hdr *,
											   sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) );
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


bool all_flows_completed(bool *flow_completed) {
    for(int i = 0; i < FLOW_NUM; i++)
    {
        if(!flow_completed[i]) return false;
    }

    return true;
}

/* >8 End Basic forwarding application lcore. */
static void
lcore_main()
{
    struct rte_mbuf *pkts_recv_buffer[TCP_WINDOW_LEN];
    struct rte_mbuf *pkts_send_buffer[TCP_WINDOW_LEN];
    struct rte_mbuf *pkt;

    uint64_t packets_recvd, packets_sent;
    uint64_t total_packets_sent[FLOW_NUM], total_packets_recvd[FLOW_NUM];
    bool flow_completed[FLOW_NUM];
    
    sliding_info window[FLOW_NUM];

    timer_info *timer = (timer_info*) malloc(NUM_PACKETS * sizeof(timer_info));
    parsed_packet_info *packet_infos = (parsed_packet_info*) malloc(TCP_WINDOW_LEN * sizeof(parsed_packet_info));

    size_t port_id = 0;

    for(int i = 0; i < FLOW_NUM; i++)
    {
        window[i].next_seq = 0;
        window[i].last_recv_seq = 0;
        total_packets_sent[i] = 0;
        total_packets_recvd[i] = 0;
        flow_completed[i] = false;
    } 

    struct timer_info overall_time;
    overall_time.start_time = raw_time();

    while (!all_flows_completed(flow_completed)) {
        if(window[port_id].last_recv_seq < NUM_PACKETS) {
            // CREATE PACKETS
            int64_t num_packets = 0, starting_seq_num = -1;
            while( window[port_id].next_seq < NUM_PACKETS && window[port_id].next_seq - window[port_id].last_recv_seq < TCP_WINDOW_LEN) {
                int64_t seq_num = window[port_id].next_seq;
                pkt = create_packet(seq_num, port_id);
                pkts_send_buffer[num_packets] = pkt;
                
                window[port_id].next_seq++;
                num_packets++;
                
                if(starting_seq_num == -1) {
                    starting_seq_num = seq_num;
                }
            }

            if(num_packets > 0) {
                // SEND PACKETS
                uint64_t start_time = raw_time();
                packets_sent = rte_eth_tx_burst(1, 0, pkts_send_buffer, num_packets);
                // printf("Flow: %u, Sent packets : %u\n", port_id, packets_sent);
                total_packets_sent[port_id] += packets_sent;
                for(int64_t i = 0; i < num_packets; i++) {
                    int64_t seq_num = starting_seq_num + i;
                    timer[seq_num].start_time = start_time;
                }
            }

            // POLL ON RECEIVE PACKETS
            packets_recvd = rte_eth_rx_burst(1, 0, pkts_recv_buffer, TCP_WINDOW_LEN);
            uint64_t end_time = raw_time();

            if (packets_recvd > 0) {
                // printf("Flow: %u, Received packets: %u\n", port_id, packets_recvd);

                // PROCESS PACKETS
                process_packets(packets_recvd, pkts_recv_buffer, packet_infos);
                for(uint64_t f = 0; f<packets_recvd; f++) {
                    if(packet_infos[f].flow_num != -1) {

                        window[packet_infos[f].flow_num].last_recv_seq++;
                        
                        total_packets_recvd[packet_infos[f].flow_num]++;
                        timer[packet_infos[f].ack_num].end_time = end_time;
                    }
                }
            }
        } else {
            flow_completed[port_id] = true;
        }
        
        port_id = (port_id+1) % FLOW_NUM;
    }

    overall_time.end_time = raw_time();

    for(int i = 0; i < FLOW_NUM; i++){
        printf("Flow %u - Packets Sent: %u, Packets Received: %u\n", i, total_packets_sent[i], total_packets_recvd[i]);
    }


    // calculate bandwidth
    uint64_t overall_time_in_ns = overall_time.end_time - overall_time.start_time;
    double overall_time_in_ms = overall_time_in_ns / 1000000.0;
    double bandwidth_in_mbps = (FLOW_SIZE * 8 * FLOW_NUM) / (overall_time_in_ms * 1000);

    printf("Overall Stats: Data: %u, Num Flows: %u, Time (ms): %f\n", FLOW_SIZE, FLOW_NUM, overall_time_in_ms);
    char *overall_stats_str;
    asprintf(&overall_stats_str, "%lu,%lu,%.3f", FLOW_SIZE, TCP_WINDOW_LEN, overall_time_in_ms);
    write_to_file(FILE_NAMES[OVERALL_LATENCY_STATS], overall_stats_str, true);

    printf("Bandwidth: %f Mbps\n", bandwidth_in_mbps);
    char *bandwidth_str;
    asprintf(&bandwidth_str, "%lu,%lu,%.3f", FLOW_SIZE, TCP_WINDOW_LEN, bandwidth_in_mbps);
    write_to_file(FILE_NAMES[BANDWIDTH_STATS], bandwidth_str, true);

    // calculate latencies
    uint64_t max_latency = 0, min_latency = INT32_MAX, avg_latency = 0, total_latency = 0;
    if(FLOW_NUM == 1) {
        for(uint32_t pkt = 0; pkt < NUM_PACKETS; pkt++) {
            uint64_t latency_in_ns = timer[pkt].end_time - timer[pkt].start_time;
            total_latency += latency_in_ns;
            if(latency_in_ns > max_latency) {
                max_latency = latency_in_ns;
            }
            if(latency_in_ns < min_latency) {
                min_latency = latency_in_ns;
            }
        }
        avg_latency = total_latency / NUM_PACKETS;
        printf("Latency: Max: %f ms, Min: %f ms, Avg: %f ms\n", max_latency/1000000.0, min_latency/1000000.0, avg_latency/1000000.0);
        char *latency_str;
        asprintf(&latency_str, "%lu,%lu,%.3f,%.3f,%.3f", FLOW_SIZE, TCP_WINDOW_LEN, avg_latency/1000000.0, max_latency/1000000.0, min_latency/1000000.0);
        write_to_file(FILE_NAMES[LATENCY_STATS], latency_str, true);
    } 

    char *multi_flow_bandwidth_str;
    asprintf(&multi_flow_bandwidth_str, "%lu,%lu,%.3f", FLOW_NUM, TCP_WINDOW_LEN, bandwidth_in_mbps / FLOW_NUM);
    write_to_file(FILE_NAMES[MULTI_FLOW_BANDWIDTH_STATS], multi_flow_bandwidth_str, true);
    

    free(packet_infos);
    free(timer);
    // return 0;
}

void setup_stats_files() {
    size_t file_names = sizeof(FILE_NAMES) / sizeof(FILE_NAMES[0]);
    for(int i = 0; i < file_names; i++) {
        FILE_NAMES[i] = (char *) malloc(500 * sizeof(char));
    }

    sprintf(FILE_NAMES[LATENCY_STATS], "%s/singleflow-latency/per_packet_latency_iter_%lu.csv", OUTPUT_DIR, ITER_NUM);
    sprintf(FILE_NAMES[OVERALL_LATENCY_STATS], "%s/singleflow-latency/overall_latency_iter_%lu.csv", OUTPUT_DIR, ITER_NUM);
    sprintf(FILE_NAMES[BANDWIDTH_STATS], "%s/singleflow-bandwidth/bandwidth_iter_%lu.csv", OUTPUT_DIR, ITER_NUM);
    sprintf(FILE_NAMES[MULTI_FLOW_BANDWIDTH_STATS], "%s/multiflow-bandwidth/multbandwidth_iter_%lu.csv", OUTPUT_DIR, ITER_NUM);
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
	unsigned nb_ports;
	uint16_t portid;

    if (argc == 6) {
        FLOW_NUM = (int) atoi(argv[1]);
        FLOW_SIZE =  (uint64_t) rte_str_to_size(argv[2]);
        TCP_WINDOW_LEN = (int) atoi(argv[3]);
        OUTPUT_DIR = argv[4];
        ITER_NUM = (int) atoi(argv[5]);
    } else {
        printf( "usage: ./lab1-client <flow num> <flow size> <window len> <output dir> <iter num>\n");
        return 1;
    }
    packet_len = (packet_len < FLOW_SIZE) ? packet_len: FLOW_SIZE;
    NUM_PACKETS = FLOW_SIZE / packet_len;

    printf("Flow Num: %lu, Flow Size: %lu, Window Len: %lu, Iter Num: %lu\n", FLOW_NUM, FLOW_SIZE, TCP_WINDOW_LEN, ITER_NUM);
    setup_stats_files();

	/* Initializion the Environment Abstraction Layer (EAL). 8< */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	/* >8 End of initialization the Environment Abstraction Layer (EAL). */

	argc -= ret;
	argv += ret;

    nb_ports = rte_eth_dev_count_avail();
	/* Allocates mempool to hold the mbufs. 8< */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
										MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
	/* >8 End of allocating mempool to hold mbuf. */

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initializing all ports. 8< */
	RTE_ETH_FOREACH_DEV(portid)
	if (portid == 1 && port_init(portid, mbuf_pool) != 0)
		rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n",
				 portid);
	/* >8 End of initializing all ports. */

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the main core only. Called on single lcore. 8< */
	lcore_main();
	/* >8 End of called on single lcore. */
    printf("Done!\n");
	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
