#include <string>

#include<twz/obj.h>

#include "intr_props.h"

//#pragma pack(push,1)
//typedef struct orp_hdr
//{
//    short hw_type; // 1 for ethernet
//    short proto_type; //type for FLIP
//    char hw_addr_len; // value: 6 for MAC
//    char proto_addr_len; //value: 128 for obj_id
//    short opcode; //req or reply
//    mac_addr_t *src_mac;
//    unsigned int src_ip;
//    mac_addr_t *dst_mac;
//    unsigned int dst_ip;
//}orp_hdr_t;
//#pragma pack(pop)
//
//typedef struct orp_table
//{
//    std::map<std::string, std::string> orp_lookup_table;
//}orp_table_t;
//
//
//
///*ObjectResolutionProtocol APIs*/
//void init_orp_map(char *name, twzobj *orp_table_obj);
//void add_orp_entry(twzobj *orp_table_obj, ipv4_addr_t ip_addr, mac_addr_t mac_addr);
//void remove_orp_entry(twzobj *orp_table_obj, ipv4_addr_t *addr); //when should we delete, how do we know entry is expired and obj_id has moved?
//mac_addr_t orp_lookup(twzobj *orp_table_obj, ipv4_addr_t *addr);
////add API to display ORP Table
//
/////*ORP Protocol Implementation*/
////void send_orp_request(ipv4_addr_t *addr);
////void send_orp_reply(eth_hdr_t *eth_hdr);
////void recv_prp_request(eth_hdr_t *eth_hdr);
////void recv_orp_reply(eth_hdr_t *eth_hdr);
