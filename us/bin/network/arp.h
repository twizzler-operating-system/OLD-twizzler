#include <string>

#include<twz/obj.h>



//ARP constants
#define arp_meta_1_request 0b10010000
#define arp_meta_2_request 0b01000110 //first reserved bit set for request message

#define arp_meta_1_reply 0b10010000
#define arp_meta_2_reply 0b01000101 //second reserved bit set for reply message


/*AddressResolutionProtocol APIs*/
void add_arp_entry(ipv4_addr_t ip_addr, mac_addr_t mac_addr);
//void remove_arp_entry(ipv4_addr_t *addr); //when should we delete, how do we know entry is expired and obj_id has moved?
mac_addr_t arp_lookup(char* ip_addr);
void print_arp_table();

///*ARP Protocol Implementation*/
//void send_arp_request(ipv4_addr_t *addr);
//void send_arp_reply(eth_hdr_t *eth_hdr);
//void recv_arp_request(eth_hdr_t *eth_hdr);
//void recv_arp_reply(eth_hdr_t *eth_hdr);
