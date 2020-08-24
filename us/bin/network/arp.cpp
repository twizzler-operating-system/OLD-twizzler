#include <map>
#include <iostream>
#include <unistd.h>

#include "flip.h"


static std::map<std::string, std::string> arp_lookup_table;

/*Helper functions*/
std::string convert_mac_to_string(mac_addr_t addr)
{
    char mac_char_ptr[MAX_MAC_CHAR_SIZE];
    memset(mac_char_ptr, 0, MAX_MAC_CHAR_SIZE);
    
    char t[3];
    
    for(int i = 0; i < 6; i++)
    {
        memset(t, '\0', 3);
        sprintf(t, "%x", addr.mac[i]);
        strcat(mac_char_ptr, t);
    }
    
    std::string mac_string = mac_char_ptr;
    
    return mac_string;
}

uint8_t str_to_hex(char *val)
{
    uint8_t return_val = 0;
    if(val[1] == '1')
        return_val+=1;
    else if(val[1] == '2')
        return_val+=2;
    else if(val[1] == '3')
        return_val+=3;
    else if(val[1] == '4')
        return_val+=4;
    else if(val[1] == '5')
        return_val+=5;
    else if(val[1] == '6')
        return_val+=6;
    else if(val[1] == '7')
        return_val+=7;
    else if(val[1] == '8')
        return_val+=8;
    else if(val[1] == '9')
        return_val+=9;
    else if(val[1] == 'a')
        return_val+=10;
    else if(val[1] == 'b')
        return_val+=11;
    else if(val[1] == 'c')
        return_val+=12;
    else if(val[1] == 'd')
        return_val+=13;
    else if(val[1] == 'e')
        return_val+=14;
    else if(val[1] == 'f')
        return_val+=15;
    
    if(val[0] == '1')
        return_val+=16;
    else if(val[0] == '2')
        return_val+=32;
    else if(val[0] == '3')
        return_val+=48;
    else if(val[0] == '4')
        return_val+=64;
    else if(val[0] == '5')
        return_val+=80;
    else if(val[0] == '6')
        return_val+=96;
    else if(val[0] == '7')
        return_val+=112;
    else if(val[0] == '8')
        return_val+=128;
    else if(val[0] == '9')
        return_val+=144;
    else if(val[0] == 'a')
        return_val+=160;
    else if(val[0] == 'b')
        return_val+=176;
    else if(val[0] == 'c')
        return_val+=192;
    else if(val[0] == 'd')
        return_val+=208;
    else if(val[0] == 'e')
        return_val+=224;
    else if(val[0] == 'f')
        return_val+=246;

    return return_val;
}


mac_addr_t convert_string_to_mac(std::string string_addr)
{
    mac_addr_t mac_addr;
    
    char temp[2];
    temp[0] = string_addr[0];
    temp[1] = string_addr[1];
    mac_addr.mac[0] = str_to_hex(temp);
    
    temp[0] = string_addr[2];
    temp[1] = string_addr[3];
    mac_addr.mac[1] = str_to_hex(temp);

    temp[0] = string_addr[4];
    temp[1] = string_addr[5];
    mac_addr.mac[2] = str_to_hex(temp);

    temp[0] = string_addr[6];
    temp[1] = string_addr[7];
    mac_addr.mac[3] = str_to_hex(temp);

    temp[0] = string_addr[6];
    temp[1] = string_addr[7];
    mac_addr.mac[3] = str_to_hex(temp);

    temp[0] = string_addr[8];
    temp[1] = string_addr[9];
    mac_addr.mac[4] = str_to_hex(temp);

    temp[0] = string_addr[10];
    temp[1] = string_addr[11];
    mac_addr.mac[5] = str_to_hex(temp);
    
    
    return mac_addr;
}





/*AddressResolutionProtocol APIs*/
void add_arp_entry(ipv4_addr_t ip_addr, mac_addr_t mac_addr)
{
    std::string ip_string = ip_addr.addr;
    std::string mac_string = convert_mac_to_string(mac_addr);

    arp_lookup_table.insert({ip_string, mac_string});
}


/*Returns MAC adder where first index is NULL, if ARP resolution was not successful*/
mac_addr_t arp_lookup(char* ip_addr, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    mac_addr_t resolved_mac;
    memset(resolved_mac.mac, 0, MAC_ADDR_SIZE);
    
    if(arp_lookup_table.find(ip_addr) == arp_lookup_table.end())
    {
        fprintf(stderr, "MAC NOT found in ARP table, sending ARP request!\n");
        arp_send_request(ip_addr, interface_obj, tx_queue_obj);
        sleep(2); //wait two seconds for ARP Reply... FUTURE WORK: use RTT as wait time
        if(arp_lookup_table.find(ip_addr) == arp_lookup_table.end())
        {
            fprintf(stderr, "Could not resolve MAC\n");
            resolved_mac.mac[0] = NULL;
            return resolved_mac;
        }
        else
            fprintf(stderr, "MAC resolved with ARP\n");
    }
    else
        fprintf(stderr, "Found MAC in ARP table\n");
    
    resolved_mac = convert_string_to_mac(arp_lookup_table[ip_addr]);
    
    return resolved_mac;
}


void print_arp_table()
{
    fprintf(stderr, "Printing table:\n");
    for (auto& t : arp_lookup_table)
        std::cout << t.first << " "
                  << t.second << "\n";
}



/*ARP Protocol Implementation*/
void arp_send_request(char *dst_ip_addr, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    flip_send(arp_meta_1_request, arp_meta_2_request, NULL, ARP_TYPE, true, dst_ip_addr, NULL, interface_obj, tx_queue_obj);
    fprintf(stderr, "Sent ARP request\n");
    
}
void arp_send_reply(char *dst_ip_addr, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    flip_send(arp_meta_1_reply, arp_meta_2_reply, NULL, ARP_TYPE, false, dst_ip_addr, NULL, interface_obj, tx_queue_obj);
    fprintf(stderr, "Sent ARP reply\n");
}

void arp_recv(twzobj *interface_obj, twzobj *tx_queue_obj, void *pkt_ptr, mac_addr_t src_mac)
{    
    uint8_t *meta_ptr = (uint8_t *)(pkt_ptr);
    
    void *flip_ptr = (void *)meta_ptr + 2*(META_HDR_SIZE);
    
    
    if(*meta_ptr & 0b00010000)
    {
        char my_ip[MAX_IPV4_CHAR_SIZE];
        get_intr_ipv4(interface_obj, my_ip);
        uint32_t my_ip_int = ipv4_char_to_int(my_ip);
        uint32_t *packet_dst_ip= (uint32_t *)flip_ptr;
        flip_ptr += ADDR_SIZE_4_BYTES;
        
        ipv4_addr_t src_ip;
        
        if(*packet_dst_ip != my_ip_int)
        {
            fprintf(stderr, "ARP Packet does not match interface IP. Packet dropped.\n");
            return;
        }
        else if(*meta_ptr & 0b10000000)
        {
            meta_ptr += META_HDR_SIZE;
            //add source address to ARP table
            if(*meta_ptr & 0b01000000)
            {
                uint32_t *packet_src_ip = (uint32_t *)flip_ptr;
                uint32_t src_ip_int = *packet_src_ip;
                
                ipv4_int_to_char(src_ip_int, src_ip.addr);
                
                add_arp_entry(src_ip, src_mac);
            }
            else
            {
                fprintf(stderr, "error: ARP Packet have source IP address. Packet dropped.\n");
                return;
            }
            
            if(*meta_ptr & 0b00000010)
            {
                fprintf(stderr, "Got ARP Request\n");
                arp_send_reply(src_ip.addr, interface_obj, tx_queue_obj);
            }
                
            else if(*meta_ptr & 0b00000001)
                fprintf(stderr, "Got ARP Reply, ARP table updated.\n");
            else
                fprintf(stderr, "error: could not recognize type of ARP message. Packet dropped.\n");
        }
    }
    else
    {
        fprintf(stderr, "Error in ARP packet, no destination IP set. Drop packet.\n");
    }
}
