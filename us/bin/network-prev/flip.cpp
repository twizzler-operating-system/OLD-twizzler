#include <iostream>
#include <string>

#include "flip.h"

uint32_t ipv4_char_to_int(char *ip_addr)
{
    char *ip_addr_copy = (char *)calloc((strlen(ip_addr)+1), 0);
    strcat(ip_addr_copy,ip_addr);
    
    
    uint32_t int_ip = 0;
    uint32_t temp = 0;
    
    char dot[1];
    dot[0] = '.';
    
    char *token;
    token = strtok(ip_addr_copy, dot);
    
    int_ip = atoi(token);
    token = strtok(NULL, dot);
    while(token != NULL)
    {
        temp = int_ip;
        int_ip = (temp << 8) | atoi(token);
        token = strtok(NULL, dot);
    }
    
    free(ip_addr_copy);
    return int_ip;
}

void ipv4_int_to_char(uint32_t ip_addr, char *ip_addr_str)
{
    uint32_t first_octet = 0xFF000000 & ip_addr;
    first_octet = first_octet >> 24;
    
    uint32_t second_octet = 0x00FF0000 & ip_addr;
    second_octet = second_octet >> 16;
    
    uint32_t third_octet = 0x0000FF00 & ip_addr;
    third_octet = third_octet >> 8;
    
    uint32_t forth_octet = 0x000000FF & ip_addr;
    
    memset(ip_addr_str, 0, MAX_IPV4_CHAR_SIZE);
    
    char t[4];
    memset(t, 0, 4);
    sprintf(t, "%d", first_octet);
    strcat(ip_addr_str, t);
    strcat(ip_addr_str, ".");
    memset(t, 0, 4);
    sprintf(t, "%d", second_octet);
    strcat(ip_addr_str, t);
    strcat(ip_addr_str, ".");
    memset(t, 0, 4);
    sprintf(t, "%d", third_octet);
    strcat(ip_addr_str, t);
    strcat(ip_addr_str, ".");
    memset(t, 0, 4);
    sprintf(t, "%d", forth_octet);
    strcat(ip_addr_str, t);
}

/*This function returns the size of the packet buffer needed for metaheaders + chosen flip headers + ethernet header + payload, in bytes
 pre-req: this function cannot be called when contructing ESP*/
int total_pkt_size(uint8_t meta1, uint8_t meta2, uint8_t meta3, int payload_size)
{
    int total_size = 0;
    total_size += payload_size;
    
    total_size += SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD;
    
    if(meta1)
    {
        total_size += META_HDR_SIZE;
        if(meta1 & 0b01000000)
        {
            fprintf(stderr, "error: calling total_pkt_size, but contructing an ESP. Please use either flip_send_esp_14bit_data() or flip_send_esp_6bit_data() functions.");
            exit(1);
        }
        if(meta1 & 0b00100000)
            //version: 1 byte
            total_size += VERSIZON_SIZE;
        
        if((meta1 & 0b00011000) == 0b00011000)
           //destiation: 128 bits -> 16 bytes
           total_size += ADDR_SIZE_16_BYTES;
        else if(meta1 & 0b00001000)
           //destination: 2 bytes
            total_size += ADDR_SIZE_2_BYTES;
        else if(meta1 & 0b00010000)
            //destination: 4 bytes
            total_size += ADDR_SIZE_4_BYTES;
        
        if(meta1 & 0b00000100)
            //type: 2 bytes
            total_size += TYPE_SIZE;
        if(meta1 & 0b00000010)
        {
            //TTL: 1 byte
            //total_size += TTL_SIZE;
            fprintf(stderr, "error: TTL bit set, but TTL not yet supported by FLIP");
        }
        if(meta1 & 0b00000001)
        {
            //flow: 4 bytes
            //total_size += FLOW_SIZE;
            fprintf(stderr, "erro: flow bit set, but flow identification not yet supported by FLIP");
        }
    }
    if(meta2)
    {
        total_size += META_HDR_SIZE;
        
        if((meta2 & 0b00011000) == 0b00011000)
            //source: 128 bits -> 16 bytes
            total_size += ADDR_SIZE_16_BYTES;
        if(meta2 & 0b00100000)
            //source: 2 bytes
            total_size += ADDR_SIZE_2_BYTES;
        if(meta2 & 0b01000000)
            //source: 4 bytes
            total_size += ADDR_SIZE_4_BYTES;
        
        if(meta2 & 0b00010000)
            //length: 2 bytes
            total_size += LENGTH_SIZE;
        if(meta2 & 0b00001000)
        {
            //checksum: 2 bytes
            //total_size += CHECKSUM_SIZE;
            fprintf(stderr, "error: checksum bit set, but checksum not yet supported by FLIP... CRC done at L2 for now.");
        }
        //fragmentation is a flag and does not require a header field
    }
    if(meta3)
    {
        fprintf(stderr, "Error: FLIP's third metaheader is for fragmentation. Fragmentation is not yet supported.");
        exit(1);
    }
    
    if(total_size > PACKET_BUFFER_MAX)
    {
        fprintf(stderr, "ERROR: sending packet bigger then allowed size max paylaod for ethernet");
        exit(1);
    }
    
    return total_size;
}

/*These are not supported by flip_send() function and must be implemented seperatly*/
void flip_send_esp_14bit_data();
void flip_send_esp_6bit_data();

/*NOTE:
 If destination address is IPV4 addres, ARP lookup will happen, unless this is a broadcast pkt
 type is L2 type + FLIP layer type.. currently the same*/
void flip_send(uint8_t meta1, uint8_t meta2, uint8_t meta3, uint16_t type, bool broadcast_pkt, char *dst_ip, char *data, twzobj *interface_obj, twzobj *tx_queue_obj)
{
    /*Calculate size of packet buffer*/
    uint16_t len_data = 0;
    if(data)
        len_data =strlen(data);
    int buff_size = total_pkt_size(meta1, meta2, meta3, len_data);
    if(buff_size >PACKET_BUFFER_MAX)
    {
        fprintf(stderr, "Error in flip_send: sending packet bigger then allowed paylod size for ethernet standard\n");
        return;
    }
    
    /*Create packet buffer object to store packet that will the transfered*/
    twzobj pkt_buffer_obj;
    if(twz_object_new(&pkt_buffer_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0)
        fprintf(stderr, "error creating packet buffer object");
    //prep object to allocate data
    twz_object_build_alloc(&pkt_buffer_obj, 0);
    void *p = twz_object_alloc(&pkt_buffer_obj, buff_size);
    void *pkt_ptr = twz_object_lea(&pkt_buffer_obj, p);
    
    
    /*Store Payload Data*/
    if(data)
    {
        char *payload = (char *)pkt_ptr;
        payload += (buff_size - len_data);
        strcpy(payload, data);
    }
    
    
    /*Create FLIP Metaheader Header*/
    //leave space on the right for the ethernet header
    uint8_t *flip_ptr = (uint8_t * )pkt_ptr;
    flip_ptr += SIZE_OF_ETH_HDR_EXCLUDING_PAYLOAD;
    
    *flip_ptr = meta1;
    flip_ptr += META_HDR_SIZE;
    
    if(meta2)
    {
        *flip_ptr = meta2;
        flip_ptr += META_HDR_SIZE;
    }
    
    if(meta3)
    {
        fprintf(stderr, "ERROR: sending packet with 3rd metaheader. Fragmentation not yet supported");
        exit(1);
    }

    
    
    
    /***********************Create FLIP header**************************/
    
    /*Add fields based on first metaheader*/
    if(meta1 & 0b01000000)
    {
        fprintf(stderr, "error: Please use either flip_send_esp_14bit_data() or flip_send_esp_6bit_data() functions for ESP pkts.");
        exit(1);
    }
    if(meta1 & 0b00100000)
    {
        uint8_t version = CURRENT_VERSION;
        uint8_t *version_ptr = (uint8_t *)flip_ptr;
        *version_ptr = version;
        flip_ptr += VERSIZON_SIZE;
    }
    
    if((meta1 & 0b00011000) == 0b00011000)
    {
        fprintf(stderr, "error: implementation for 16 byte address not yet done");
    }
    else if(meta1 & 0b00001000)
    {
       fprintf(stderr, "error: implementation for 2 byte address not yet done");
    }
    else if(meta1 & 0b00010000)
    {
        uint32_t ip_dst_dec = ipv4_char_to_int(dst_ip);
        uint32_t *ip_dst_ptr = (uint32_t *)flip_ptr;
        *ip_dst_ptr = ip_dst_dec;
        flip_ptr += ADDR_SIZE_4_BYTES;
    }
    
    if(meta1 & 0b00000100)
    {
        uint16_t *type_ptr = (uint16_t *)flip_ptr;
        *type_ptr = type;
        flip_ptr += TYPE_SIZE;
    }
    if(meta1 & 0b00000010)
        fprintf(stderr, "error: TTL bit set, but TTL not yet supported by FLIP. Space will not be allocated in FLIP header for TTL field.");

    if(meta1 & 0b00000001)
        fprintf(stderr, "error: flow bit set, but flow identification not yet supported by FLIP. Space will not be allocated in FLIP header for flow field.");

    /*Add fields based on second metaheader*/
    if(meta2)
    {
        if((meta2 & 0b00011000) == 0b00011000)
        {
            fprintf(stderr, "error: implementation for 16 byte address not yet done");
        }
        if(meta2 & 0b00100000)
        {
           fprintf(stderr, "error: implementation for 2 byte address not yet done");
        }
        if(meta2 & 0b01000000)
        {
            char src_ip[MAX_IPV4_CHAR_SIZE];
            get_intr_ipv4(interface_obj, src_ip);
            
            uint32_t ip_src_dec = ipv4_char_to_int(src_ip);
            uint32_t *ip_src_ptr = (uint32_t *)flip_ptr;
            *ip_src_ptr = ip_src_dec;
            flip_ptr += ADDR_SIZE_4_BYTES;
        }
               
        if(meta2 & 0b00010000)
        {
            uint16_t *len_ptr = (uint16_t *)flip_ptr;
            *len_ptr = len_data;
            fprintf(stderr, "len %d\n", *len_ptr);
            flip_ptr += LENGTH_SIZE;
        }
        if(meta2 & 0b00001000)
        {
            fprintf(stderr, "error: checksum bit set, but checksum not yet supported by FLIP. Space will not be allocated in FLIP header for checksum field.");
        }
    }
    
    /*Add fields based on third metaheader*/
    //third metaheader fields not yet supported
    
    
    /*Pass packet buffer down to Ethernet Layer*/
    mac_addr_t dest_mac;
    
    if(broadcast_pkt)
    {
        memset((void*)dest_mac.mac, BROAD_MAC_BIT, MAC_ADDR_SIZE*(sizeof(char)));
        l2_send(dest_mac, tx_queue_obj, interface_obj, pkt_ptr, type, buff_size);
    }
    else
    {
        dest_mac = arp_lookup(dst_ip, interface_obj, tx_queue_obj);
        if(dest_mac.mac[0] == NULL) //first bit set to NULL if ARP failed
            fprintf(stderr, "Could not resolve IP Address, packet dropped.");
        else
            l2_send(dest_mac, tx_queue_obj, interface_obj, pkt_ptr, type, buff_size);
    }
}


void flip_recv(twzobj *interface_obj, void *pkt_ptr, mac_addr_t src_mac)
{
    uint8_t *meta_ptr = (uint8_t *)(pkt_ptr);
    
    //store the start of the FLIP header
    void *flip_ptr = (void *)meta_ptr + META_HDR_SIZE;
    
    //move start of FLIP header based on how many bits of meta-header we have
    if(*meta_ptr & 0b10000000)
    {
        flip_ptr += META_HDR_SIZE;
        
        //check if third meta header is set
        if(*(meta_ptr + META_HDR_SIZE) & 0b10000000)
            flip_ptr += META_HDR_SIZE;
    }
    
    
    if(*meta_ptr & 0b01000000)
    {
        fprintf(stderr, "error: ESP bit set for packet of type 0x2020. Packet dropped.\n");
        return;
    }
    if(*meta_ptr & 0b00100000)
    {
        uint8_t version = CURRENT_VERSION;
        uint8_t *version_ptr = (uint8_t *)flip_ptr;
        if(*version_ptr != version)
        {
            fprintf(stderr, "error: FLIP packet does not have supported version. Packet dropped.\n");
            return;
        }
        flip_ptr += VERSIZON_SIZE;
    }
    
    if((*meta_ptr & 0b00011000) == 0b00011000)
    {
        fprintf(stderr, "error: recieved FLIP packet with 16 byte destination address. This is not yet supported. Packet dropped.\n");
        return;
    }
    else if(*meta_ptr & 0b00001000)
    {
       fprintf(stderr, "error: recieved FLIP packet with 2 byte destination address. This is not yet supported. Packet dropped.\n");
        return;
    }
    else if(*meta_ptr & 0b00010000)
    {
        char my_ip[MAX_IPV4_CHAR_SIZE];
        get_intr_ipv4(interface_obj, my_ip);
        
        uint32_t my_ip_int = ipv4_char_to_int(my_ip);
        
        uint32_t *packet_dst_ip= (uint32_t *)flip_ptr;
        
        if(*packet_dst_ip != my_ip_int)
        {
            fprintf(stderr, "Received packet with destination IP that does not match interface IP address. Packet dropped.\n");
            return;
        }
        flip_ptr += ADDR_SIZE_4_BYTES;
    }
    
    if(*meta_ptr & 0b00000100)
    {
        uint16_t *type_ptr = (uint16_t *)flip_ptr;
        uint16_t type = *type_ptr;
        flip_ptr += TYPE_SIZE;
        fprintf(stderr, "WARNING: Got packet with type set. This condition needs to still be implemented to send packet to correct function based on the type. Right now having a type set in the FLIP header does not do anything. Packet is NOT dropped, but continues to be decapsulated.\n");
    }
    if(*meta_ptr & 0b00000010)
    {
        fprintf(stderr, "error: TTL bit set on recieved FLIP packet. TTL not yet supported. Packed dropped.\n");
        return;
    }

    if(*meta_ptr & 0b00000001)
    {
        fprintf(stderr, "error: flow bit set on recieved packet, but flow identification not yet supported by FLIP. Packet dropped.\n");
        return;
    }
    
    //Check if continuation bit is set for metaheader
    if(*meta_ptr & 0b10000000)
    {
        meta_ptr += META_HDR_SIZE;
        
        if((*meta_ptr & 0b00011000) == 0b00011000)
        {
            fprintf(stderr, "error: recieved FLIP packet with 16 byte source address. This is not yet supported. Packet dropped.\n");
            return;
        }
        if(*meta_ptr & 0b00100000)
        {
            fprintf(stderr, "error: recieved FLIP packet with 2 byte source address. This is not yet supported. Packet dropped.\n");
            return;
        }
        if(*meta_ptr & 0b01000000)
        {
            //Source IPv4 address is set
            //Pass this value to to a function if it needs it to reply to message
            
            uint32_t *packet_src_ip = (uint32_t *)flip_ptr;
            uint32_t src_ip_int = *packet_src_ip;
            
            ipv4_addr_t src_ip;
            ipv4_int_to_char(src_ip_int, src_ip.addr);
            
            add_arp_entry(src_ip, src_mac);
         
            flip_ptr += ADDR_SIZE_4_BYTES;
        }
        
        if(*meta_ptr & 0b00010000)
        {
            //length of data included.
            //how do we want to use this information, pass it to upper layer?
            
            flip_ptr += LENGTH_SIZE;
        }
        if(*meta_ptr & 0b00001000)
        {
            fprintf(stderr, "error: Recieved packet with checksum bit set. Checksum not yet supported by FLIP. Packet is NOT dropped, but continues to be decapsulated.\n");
        }
        
        if(*meta_ptr & 0b10000000)
        {
            fprintf(stderr, "error: recieved FLIP packet with 3rd metaheader. This is not yet supported. Packet dropped.\n");
            return;
        }
        
        
    }
    
    //FUTURE WORK: Pass this to above layer
    char *payload = (char *)flip_ptr;
    fprintf(stderr, "Got data: %s\n", payload);

}