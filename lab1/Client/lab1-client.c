/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "../Utils/utils.h"

/* >8 End Basic forwarding application lcore. */
static void
lcore_main()
{
    struct rte_mbuf *pkts[BURST_SIZE];
    struct rte_mbuf *pkt;
    // char *buf_ptr;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_tcp_hdr *tcp_hdr;

	struct sliding_hdr *sld_h_ack;
    uint16_t nb_rx;
    uint64_t reqs = 0;
    // uint64_t cycle_wait = intersend_time * rte_get_timer_hz() / (1e9);
    
    // TODO: add in scaffolding for timing/printing out quick statistics
    int outstanding[flow_num];
    uint16_t seq[flow_num];
    // track window starting and ending
    uint16_t window_start[flow_num];
    uint16_t window_end[flow_num];

    size_t port_id = 0;
    for(size_t i = 0; i < flow_num; i++)
    {
        outstanding[i] = 0;
        seq[i] = 0;
        window_start[i] = -1;
        window_end[i] = -1;
    } 

    while (seq[port_id] < NUM_PING) {
        // send a packet
        pkt = rte_pktmbuf_alloc(mbuf_pool);
        if (pkt == NULL) {
            printf("Error allocating tx mbuf\n");
            return -EINVAL;
        }
        size_t header_size = 0;
        uint8_t *ptr = rte_pktmbuf_mtod(pkt, uint8_t *);
        uint32_t seq_num = seq[port_id];
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

#ifdef DEBUG
        printf("Sending packet with seq: %u\n", seq_num);
#endif
        int pkts_sent = 0;
        unsigned char *pkt_buffer = rte_pktmbuf_mtod(pkt, unsigned char *);
        pkts_sent = rte_eth_tx_burst(1, 0, &pkt, 1);
        if(pkts_sent == 1)
        {
            seq[port_id]++;
            outstanding[port_id] ++;
        }
        
        uint64_t last_sent = rte_get_timer_cycles();
        // printf("Sent packet at %u, %d is outstanding, intersend is %u\n", (unsigned)last_sent, outstanding, (unsigned)intersend_time);

        /* now poll on receiving packets */
        nb_rx = 0;
        reqs += 1;
        while ((outstanding[port_id] > 0)) {
            nb_rx = rte_eth_rx_burst(1, 0, pkts, BURST_SIZE);
            if (nb_rx == 0) {
                continue;
            }

            printf("Received burst of %u\n", (unsigned)nb_rx);
            for (int i = 0; i < nb_rx; i++) {
                struct sockaddr_in src, dst;
                void *payload = NULL;
                size_t payload_length = 0;
                int p = parse_packet(&src, &dst, &payload, &payload_length, pkts[i]);
                if (p != 0) {

                    rte_pktmbuf_free(pkts[i]);
                    outstanding[p-1]--;
                } else {
                    rte_pktmbuf_free(pkts[i]);
                }
            }
        }

        port_id = (port_id+1) % flow_num;
    }
    printf("Sent %"PRIu64" packets.\n", reqs);
    // dump_latencies(&latency_dist);
    // return 0;
}
/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */

int main(int argc, char *argv[])
{

	unsigned nb_ports;
	uint16_t portid;

    if (argc == 3) {
        flow_num = (int) atoi(argv[1]);
        flow_size =  (int) atoi(argv[2]);
    } else {
        printf( "usage: ./lab1-client <flow_num> <flow_size>\n");
        return 1;
    }

    NUM_PING = flow_size / packet_len;

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
