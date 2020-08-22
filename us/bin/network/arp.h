#include <string>

#include<twz/obj.h>



//ARP constants
#define arp_meta_1_request 0b10010000
#define arp_meta_2_request 0b01000110 //first reserved bit set for request message

#define arp_meta_1_reply 0b10010000
#define arp_meta_2_reply 0b01000101 //second reserved bit set for reply message


//typedef struct arp_table
//{
//std::map<std::string, std::string> arp_lookup_table;
//}arp_table_t;



/*ObjectResolutionProtocol APIs*/
//std::map<std::string, std::string> init_arp_map();
void add_arp_entry(ipv4_addr_t ip_addr, mac_addr_t mac_addr);
void print_arp_table();
//void remove_arp_entry(twzobj *arp_table_obj, ipv4_addr_t *addr); //when should we delete, how do we know entry is expired and obj_id has moved?
//mac_addr_t arp_lookup(twzobj *arp_table_obj, ipv4_addr_t *addr);
//add API to display ARP Table

///*ARP Protocol Implementation*/
//void send_arp_request(ipv4_addr_t *addr);
//void send_arp_reply(eth_hdr_t *eth_hdr);
//void recv_arp_request(eth_hdr_t *eth_hdr);
//void recv_arp_reply(eth_hdr_t *eth_hdr);
