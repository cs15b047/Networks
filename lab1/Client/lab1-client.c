/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "../Utils/utils.h"


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


int *process_packets(uint16_t num_recvd, struct rte_mbuf **pkts) {
    printf("Received burst of %u\n", (unsigned)num_recvd);
    struct rte_tcp_hdr *tcp_h;
    int *flow = (int*) malloc(num_recvd * sizeof(int));
    for (int i = 0; i < num_recvd; i++) {
        struct sockaddr_in src, dst;
        void *payload = NULL;
        size_t payload_length = 0;
        int f_num = parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);

        tcp_h = rte_pktmbuf_mtod_offset(pkts[i], struct rte_tcp_hdr *,
											   sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) );
        if (f_num != 0) {
            rte_pktmbuf_free(pkts[i]);
            flow[i] = f_num - 1;
        } else {
            printf("Ignoring bad MAC packet\n");
            flow[i] = -1;
        }
    }

    return flow;
}


bool all_flows_completed(bool *flow_completed) {
    for(size_t i = 0; i < flow_num; i++)
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

    uint16_t packets_recvd;
    uint32_t seq_num;
    uint32_t total_packets_sent[flow_num], total_packets_recvd[flow_num];
    bool flow_completed[flow_num];
    
    sliding_info window[flow_num];

    size_t port_id = 0;

    for(size_t i = 0; i < flow_num; i++)
    {
        window[i].next_seq = 0;
        window[i].last_recv_seq = 0;
        total_packets_sent[i] = 0;
        total_packets_recvd[i] = 0;
        flow_completed[i] = false;
    } 

    while (!all_flows_completed(flow_completed)) {
        if(window[port_id].last_recv_seq < NUM_PACKETS - 1) {
            // CREATE PACKETS
            uint32_t num_packets = 0;
            while( window[port_id].next_seq < NUM_PACKETS && window[port_id].next_seq - window[port_id].last_recv_seq < TCP_WINDOW_LEN) {
                seq_num = window[port_id].next_seq;
                pkt = create_packet(seq_num, port_id);
                pkts_send_buffer[num_packets] = pkt;
                
                window[port_id].next_seq++;
                num_packets++;
            }

            if(num_packets > 0) {
                // SEND PACKETS
                int pkts_sent = 0;
                pkts_sent = rte_eth_tx_burst(1, 0, pkts_send_buffer, num_packets);
                printf("Flow: %u, Sent packets : %u\n", port_id, pkts_sent);
                total_packets_sent[port_id] += pkts_sent;
            }

            // POLL ON RECEIVE PACKETS
            packets_recvd = rte_eth_rx_burst(1, 0, pkts_recv_buffer, TCP_WINDOW_LEN);
            if (packets_recvd > 0) {
                printf("Flow: %u, Received packets: %u\n", port_id, packets_recvd);

                // PROCESS PACKETS
                int *flows = process_packets(packets_recvd, pkts_recv_buffer);
                for(int f = 0; f<packets_recvd; f++) {
                    if(flows[f] != -1) {
                        window[flows[f]].last_recv_seq++;
                        total_packets_recvd[flows[f]]++;
                    }
                }
            }
        } else {
            flow_completed[port_id] = true;
        }
        
        port_id = (port_id+1) % flow_num;
    }

    for(size_t i = 0; i < flow_num; i++)
    {
        printf("Flow %u - Packets Sent: %u, Packets Received: %u\n", i, total_packets_sent[i], total_packets_recvd[i]);
    }
    // dump_latencies(&latency_dist);
    // return 0;
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

    if (argc == 3) {
        flow_num = (int) atoi(argv[1]);
        flow_size =  (int) atoi(argv[2]);
    } else {
        printf( "usage: ./lab1-client <flow_num> <flow_size>\n");
        return 1;
    }

    NUM_PACKETS = flow_size / packet_len;

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
