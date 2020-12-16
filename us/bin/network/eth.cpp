#include "eth.h"

#include "arp.h"
#include "interface.h"
#include "ipv4.h"
#include "twz.h"

void eth_tx_2(const char *interface_name,
  mac_addr_t dst_mac,
  uint16_t eth_type,
  void *eth_pkt_ptr,
  uint16_t eth_pkt_size,
  void *payload_ptr,
  uint16_t payload_len)
{
	bool has_payload = payload_ptr && payload_len > 0;
	interface_t *interface = get_interface_by_name(interface_name);

	eth_hdr_t *eth_hdr = (eth_hdr_t *)eth_pkt_ptr;

	/* source MAC */
	memcpy(eth_hdr->src_mac.mac, interface->mac.mac, MAC_ADDR_SIZE);

	/* destination MAC */
	memcpy(eth_hdr->dst_mac.mac, dst_mac.mac, MAC_ADDR_SIZE);

	/* Ethernet type */
	eth_hdr->type = htons(eth_type);

	/* create a pointer to the packet headers */
	struct packet_queue_entry pqe;
	pqe.qe.info = get_id();
	pqe.ptr = twz_ptr_swizzle(&interface->tx_queue_obj, eth_pkt_ptr, FE_READ);
	pqe.len = eth_pkt_size;
	pqe.flags = has_payload ? 0 : PACKET_FLAGS_EOP;
	/* enqueue packet (pointer) to primary tx queue */
	queue_submit(&interface->tx_queue_obj, (struct queue_entry *)&pqe, 0);

	/* create a pointer to the packet payload */
	pqe.qe.info = get_id();
	pqe.ptr = twz_ptr_swizzle(&interface->tx_queue_obj, payload_ptr, FE_READ);
	pqe.len = payload_len;
	pqe.flags = PACKET_FLAGS_EOP;
	/* enqueue packet (pointer) to primary tx queue */
	queue_submit(&interface->tx_queue_obj, (struct queue_entry *)&pqe, 0);

	/* for debugging */
	// fprintf(stderr, "[debug] Tx ETH Frame: ");
	// for (uint16_t i = 0; i < eth_pkt_size; ++i) {
	//    fprintf(stderr, "%02X ", *((uint8_t *)eth_pkt_ptr + i));
	//}
	// fprintf(stderr, "\n");
}

void eth_tx(const char *interface_name,
  mac_addr_t dst_mac,
  uint16_t eth_type,
  void *eth_pkt_ptr,
  uint16_t eth_pkt_size)
{
	interface_t *interface = get_interface_by_name(interface_name);

	eth_hdr_t *eth_hdr = (eth_hdr_t *)eth_pkt_ptr;

	/* source MAC */
	memcpy(eth_hdr->src_mac.mac, interface->mac.mac, MAC_ADDR_SIZE);

	/* destination MAC */
	memcpy(eth_hdr->dst_mac.mac, dst_mac.mac, MAC_ADDR_SIZE);

	/* Ethernet type */
	eth_hdr->type = htons(eth_type);

	/* create a pointer to the packet */
	struct packet_queue_entry pqe;
	pqe.qe.info = get_id();
	pqe.ptr = twz_ptr_swizzle(&interface->tx_queue_obj, eth_pkt_ptr, FE_READ);
	pqe.len = eth_pkt_size;
	pqe.flags = PACKET_FLAGS_EOP;

	/* enqueue packet (pointer) to primary tx queue */
	queue_submit(&interface->tx_queue_obj, (struct queue_entry *)&pqe, 0);

	/* for debugging */
	// fprintf(stderr, "[debug] Tx ETH Frame: ");
	// for (uint16_t i = 0; i < eth_pkt_size; ++i) {
	//    fprintf(stderr, "%02X ", *((uint8_t *)eth_pkt_ptr + i));
	//}
	// fprintf(stderr, "\n");
}

void eth_rx(const char *interface_name)
{
	interface_t *interface = get_interface_by_name(interface_name);
	twzobj *rx_queue_obj = &interface->rx_queue_obj;

	fprintf(stderr, "Started packet receive thread for interface %s\n", interface_name);

	while(true) {
		/* store pointer to received packet */
		struct packet_queue_entry pqe;

		/* dequeue packet (pointer) from primary rx queue */
		queue_receive(rx_queue_obj, (struct queue_entry *)&pqe, 0);

		/* packet structure from the nic starts with a packet_header struct that
		 * contains information followed by the actual packet data. */
		struct packet_header *ph = (struct packet_header *)twz_object_lea(rx_queue_obj, pqe.ptr);
		ph += 1;

		/* decapsulate then send to higher layers */
		eth_hdr_t *eth_pkt_ptr = (eth_hdr_t *)ph;
		mac_addr_t src_mac = eth_pkt_ptr->src_mac;
		mac_addr_t dst_mac = eth_pkt_ptr->dst_mac;
		uint16_t eth_type = ntohs(eth_pkt_ptr->type);

		if(!compare_mac_addr(dst_mac, interface->mac)) {
			fprintf(stderr, "eth_rx: wrong destination; packet dropped (");
			fprintf(stderr,
			  "SRC: %02X:%02X:%02X:%02X:%02X:%02X ",
			  src_mac.mac[0],
			  src_mac.mac[1],
			  src_mac.mac[2],
			  src_mac.mac[3],
			  src_mac.mac[4],
			  src_mac.mac[5]);
			fprintf(stderr,
			  "DST: %02X:%02X:%02X:%02X:%02X:%02X ",
			  dst_mac.mac[0],
			  dst_mac.mac[1],
			  dst_mac.mac[2],
			  dst_mac.mac[3],
			  dst_mac.mac[4],
			  dst_mac.mac[5]);
			fprintf(stderr, "TYPE: 0x%04X)\n", eth_type);

		} else {
			char *payload = (char *)eth_pkt_ptr;
			payload += ETH_HDR_SIZE;

			remote_info_t remote_info;
			remote_info.remote_mac = src_mac;
			remote_info.twz_op = NOOP;

			switch(eth_type) {
				case TWIZZLER:
					twz_rx(interface_name, &remote_info, payload);
					break;

				case ARP:
					arp_rx(interface_name, payload);
					break;

				case IPV4:
					ip_rx(interface_name, &remote_info, payload);
					break;

				default:
					fprintf(stderr,
					  "eth_rx: unrecognized Ethernet type 0x%04X; "
					  "packet dropped\n",
					  eth_type);
			}
		}

		/* enqueue an entry in the completion rx queue to signal rx is complete
		 * so the packet memory can be freed */
		queue_complete(rx_queue_obj, (struct queue_entry *)&pqe, 0);
	}
}
