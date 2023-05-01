#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_ip.h>

#include <rte_common.h>

#include <time.h>
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define FLOW_NUM 1
#define TCP_WINDOW_LEN 20
#define MIN(a,b) (((a)<(b))?(a):(b))
uint64_t NUM_PACKETS = 100;

/* Define the mempool globally */
struct rte_mempool *mbuf_pool = NULL;
static struct rte_ether_addr my_eth;
static size_t message_size = 1000;
uint64_t FLOW_SIZE = 10000;


int packet_len = 1000;
int ack_len = 10;

// Specify the dst mac address and default ip here here:
struct rte_ether_addr DST_MAC = {{0x14,0x58,0xD0,0x58,0x7F,0xF3}};
const char* DEFAULT_IP = "127.0.0.1";

static uint64_t raw_time(void) {
    struct timespec tstart={0,0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    uint64_t t = (uint64_t)(tstart.tv_sec*1.0e9 + tstart.tv_nsec);
    return t;

}

static uint64_t time_now(uint64_t offset) {
    return raw_time() - offset;
}

uint32_t
checksum(unsigned char *buf, uint32_t nbytes, uint32_t sum)
{
	unsigned int	 i;

	/* Checksum all the pairs of bytes first. */
	for (i = 0; i < (nbytes & ~1U); i += 2) {
		sum += (uint16_t)ntohs(*((uint16_t *)(buf + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	if (i < nbytes) {
		sum += buf[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	return sum;
}

uint32_t
wrapsum(uint32_t sum)
{
	sum = ~sum & 0xFFFF;
	return htons(sum);
}


static int parse_packet(struct sockaddr_in *src,
                        struct sockaddr_in *dst,
                        int64_t **payload,
                        size_t *payload_len,
                        struct rte_mbuf *pkt)
{
    // packet layout order is (from outside -> in):
    // ether_hdr
    // ipv4_hdr
    // udp_hdr --> tcp_hdr
    // client timestamp
    uint8_t *p = rte_pktmbuf_mtod(pkt, uint8_t *);
    size_t header = 0;

    // check the ethernet header
    struct rte_ether_hdr * const eth_hdr = (struct rte_ether_hdr *)(p);
    p += sizeof(*eth_hdr);
    header += sizeof(*eth_hdr);
    uint16_t eth_type = ntohs(eth_hdr->ether_type);
    struct rte_ether_addr mac_addr = {};

    rte_eth_macaddr_get(1, &mac_addr);
    if (!rte_is_same_ether_addr(&mac_addr, &eth_hdr->dst_addr)) {
        printf("Bad MAC address:\n");
        printf("Packet MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            eth_hdr->dst_addr.addr_bytes[0], eth_hdr->dst_addr.addr_bytes[1],
			eth_hdr->dst_addr.addr_bytes[2], eth_hdr->dst_addr.addr_bytes[3],
			eth_hdr->dst_addr.addr_bytes[4], eth_hdr->dst_addr.addr_bytes[5]);
        printf("My MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            mac_addr.addr_bytes[0], mac_addr.addr_bytes[1],
            mac_addr.addr_bytes[2], mac_addr.addr_bytes[3],
            mac_addr.addr_bytes[4], mac_addr.addr_bytes[5]);
        return 0;
    }
    if (RTE_ETHER_TYPE_IPV4 != eth_type) {
        printf("Bad ether type\n");
        return 0;
    }

    // check the IP header
    struct rte_ipv4_hdr *const ip_hdr = (struct rte_ipv4_hdr *)(p);
    p += sizeof(*ip_hdr);
    header += sizeof(*ip_hdr);

    // In network byte order.
    in_addr_t ipv4_src_addr = ip_hdr->src_addr;
    in_addr_t ipv4_dst_addr = ip_hdr->dst_addr;

    if (IPPROTO_TCP != ip_hdr->next_proto_id) {
        printf("Bad next proto_id\n");
        return 0;
    }
    
    src->sin_addr.s_addr = ipv4_src_addr;
    dst->sin_addr.s_addr = ipv4_dst_addr;
    
    // check tcp header
    struct rte_tcp_hdr * const tcp_hdr = (struct rte_tcp_hdr *)(p);
    p += sizeof(*tcp_hdr);
    header += sizeof(*tcp_hdr);
// #ifdef DEBUG
//     uint32_t ack_num = rte_cpu_to_be_32(tcp_hdr->recv_ack);
//     printf("Received packet with ack: %u\n", ack_num);
// #endif
    // In network byte order.
    in_port_t tcp_src_port = tcp_hdr->src_port;
    in_port_t tcp_dst_port = tcp_hdr->dst_port;
    int ret = rte_cpu_to_be_16(tcp_hdr->dst_port) - 5000;

    src->sin_port = tcp_src_port;
    dst->sin_port = tcp_dst_port;
    
    src->sin_family = AF_INET;
    dst->sin_family = AF_INET;
    
    *payload_len = pkt->pkt_len - header;
    *payload = (int64_t *)p;
    return ret;

}
/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */

/* Main functional part of port initialization. 8< */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0)
	{
		printf("Error during getting device (port %u) info: %s\n",
			   port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++)
	{
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
										rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++)
	{
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
										rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Starting Ethernet port. 8< */
	retval = rte_eth_dev_start(port);
	/* >8 End of starting of ethernet port. */
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	retval = rte_eth_macaddr_get(port, &my_eth);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
		   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
		   port, RTE_ETHER_ADDR_BYTES(&my_eth));

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	/* End of setting RX port in promiscuous mode. */
	if (retval != 0)
		return retval;

	return 0;
}
/* >8 End of main functional part of port initialization. */


////////////////////////////// HEADERS //////////////////////////////

static void set_eth_hdrs(struct rte_ether_hdr *eth_hdr, struct rte_ether_addr *dst_mac) {
    rte_ether_addr_copy(&my_eth, &eth_hdr->src_addr);
    rte_ether_addr_copy(dst_mac, &eth_hdr->dst_addr);
    eth_hdr->ether_type = rte_be_to_cpu_16(RTE_ETHER_TYPE_IPV4);
}

static void set_ipv4_hdrs(struct rte_ipv4_hdr *ipv4_hdr, rte_be32_t src_addr, rte_be32_t dst_addr, size_t pkt_len) {
    ipv4_hdr->version_ihl = 0x45;
    ipv4_hdr->type_of_service = 0x0;
    ipv4_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + pkt_len);
    ipv4_hdr->packet_id = rte_cpu_to_be_16(1);
    ipv4_hdr->fragment_offset = 0;
    ipv4_hdr->time_to_live = 64;
    ipv4_hdr->next_proto_id = IPPROTO_TCP;
    
    ipv4_hdr->src_addr = src_addr;
    ipv4_hdr->dst_addr = dst_addr;

    uint32_t ipv4_checksum = wrapsum(checksum((unsigned char *)ipv4_hdr, sizeof(struct rte_ipv4_hdr), 0));
    ipv4_hdr->hdr_checksum = rte_cpu_to_be_32(ipv4_checksum);
}

static void set_tcp_request_hdrs(struct rte_tcp_hdr *tcp_hdr, struct rte_ipv4_hdr *ipv4_hdr, size_t port_id, uint32_t seq_num) {
    uint16_t srcp = 5001 + port_id;
    uint16_t dstp = 5001 + port_id;
    tcp_hdr->src_port = rte_cpu_to_be_16(srcp);
    tcp_hdr->dst_port = rte_cpu_to_be_16(dstp);
    tcp_hdr->rx_win = rte_cpu_to_be_16(TCP_WINDOW_LEN * packet_len); // window size = # of packets * packet length
    tcp_hdr->sent_seq = rte_cpu_to_be_32(seq_num); // seq[flow] denotes sequence number per flow

    uint16_t tcp_cksum = rte_ipv4_udptcp_cksum(ipv4_hdr, (void *)tcp_hdr);

    // printf("Udp checksum is %u\n", (unsigned)udp_cksum);
    tcp_hdr->cksum = rte_cpu_to_be_16(tcp_cksum);
}

static void set_tcp_response_hdrs(struct rte_tcp_hdr *tcp_hdr, struct rte_ipv4_hdr *ipv4_hdr, uint16_t src_port, uint16_t dst_port, uint32_t ack_seq) {
	tcp_hdr->src_port = src_port;
	tcp_hdr->dst_port = dst_port;
	tcp_hdr->recv_ack = rte_cpu_to_be_32(ack_seq); // Acknowledgement number is sequence number of the packet ack'd

	uint16_t tcp_cksum =  rte_ipv4_udptcp_cksum(ipv4_hdr, (void *)tcp_hdr);
	tcp_hdr->cksum = rte_cpu_to_be_16(tcp_cksum);
}

////////////////////////////// PAYLOAD //////////////////////////////

static void set_payload(uint8_t *ptr, struct rte_mbuf *pkt, size_t pkt_len, size_t header_size, int64_t* data) {
     /* set the payload */
    memcpy(ptr, data, pkt_len);

    pkt->l2_len = RTE_ETHER_HDR_LEN;
    pkt->l3_len = sizeof(struct rte_ipv4_hdr);
    // pkt->ol_flags = PKT_TX_IP_CKSUM | PKT_TX_IPV4;
    pkt->data_len = header_size + pkt_len;
    pkt->pkt_len = header_size + pkt_len;
    pkt->nb_segs = 1;
}


typedef struct sliding_info {
    uint64_t next_seq;
    uint64_t last_recv_seq;
} sliding_info;


typedef struct parsed_packet_info {
    int64_t flow_num;
    uint64_t ack_num;
} parsed_packet_info;

typedef struct timer_info {
    uint64_t start_time;
    uint64_t end_time;
} timer_info;

#endif /* UTILS_H */