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


void flip_send(twzobj *hdr_obj, uint8_t meta1, uint8_t meta2, uint8_t meta3, uint8_t type, char *dst_ip, char *payload)
{
    /*Create meta-header*/
    flip_l3_metahdr_t *flip_hdr = (flip_l3_metahdr_t *) twz_object_base(hdr_obj);
    
    flip_hdr->meta_hdr1 = meta1;
    flip_hdr->meta_hdr2 = meta2;
    flip_hdr->meta_hdr3 = meta3;
    
    
    /*Create first header with active fields*/
    flip_l3_hdr1_t first_flip_hdr; //should we preset the fields to zero, if we are not allocating memory for each field?

    //check if ESP bit is set
    if(meta1 & 0b01000000)
    {
        if(meta1 & 0b10000000)
        {
            flip_esp_14_bit_data esp_data;
            first_flip_hdr.esp_data = esp_data;
            flip_hdr->next_hdr = first_flip_hdr;
            return;
        }

    }
    //check if version bit is set
    if(meta1 & 0b00100000)
    {
        //assume 0
        first_flip_hdr.version = 0;
    }
    //check if destination bits is set for 2 byte address
    if(meta1 & 0b00001000)
    {
        //struct for this must be created, or memory must be allocated... right now only 4 byte supported
    }
    //check if destination bits is set for 4 byte address
    else if(meta1 & 0b00010000)
    {
        uint32_t int_dst_ip = ipv4_char_to_int(dst_ip);
        first_flip_hdr.destination = int_dst_ip;
    }
    //check if destination bits is set for variable length address
    else if(meta1 & 0b00011000)
    {
        //struct for this must be created, or memory must be allocated... right now only 4 byte supported
    }
    //check if type bits is set
    if(meta1 & 0b00000100)
    {
        first_flip_hdr.type = type;
    }
    //check if TTL bits is set
    if(meta1 & 0b00000010)
    {
        //not yet supported
    }
    //check if flow bits is set
    if(meta1 & 0b00000001)
    {
        //not yet supported
    }
    //check if continuation bit is set
    if(meta1 & 0b10000000)
    {
        /*Create second header with active fields*/
        flip_l3_hdr2_t second_flip_hdr;

        //check if source bits is set for 2 byte address
        if(meta2 & 0b00100000)
        {
            //struct for this must be created, or memory must be allocated... right now only 4 byte supported
        }
        //check if source bits is set for 4 byte address
        else if(meta2 & 0b01000000)
        {
            /*!!!!!!CREATE METHOD TO GRAB IP ADDRESS */
            char src_ip[MAX_IPV4_CHAR_SIZE] = "4.4.4.4";//eventually needs to be grabbed with function
            uint32_t int_src_ip = ipv4_char_to_int(src_ip);
            first_flip_hdr.destination = int_src_ip;
        }
        //check if source bits is set for variable length address
        else if(meta2 & 0b01100000)
        {
            //struct for this must be created, or memory must be allocated... right now only 4 byte supported
        }

        //check if length bit is set
        if(meta2 & 0b00010000)
        {
            //call function to calculate lenght of packet and store the data
        }

        //check if checksum bit is set
        if(meta2 & 0b00001000)
        {
            //call function to calculate checksum of packet and store the data
        }
        //check if don't fragment bit is set
        if(meta2 & 0b00000100)
        {
            //no fragmentation supported
        }
        /*last 2 bits are reserved for future use*/

        //check if continuation bit is set
        if(meta2 & 0b10000000)
        {
            /*Create third header with active fields*/
            flip_l3_hdr3_t third_flip_hdr;
            /*last header used for fragmentation, which isn't currently supported*/

            second_flip_hdr.next_hdr = third_flip_hdr;
        }
        first_flip_hdr.next_hdr = second_flip_hdr;
    }
    flip_hdr->next_hdr = first_flip_hdr;

    
    
    /*
     !!!!!!!!!!!! MUST BE ABLE TO ADD PAYLOAD TO CORRECT HEADER, BASED ON RATHER OR NOT CONTINUATION BITS ARE SET
     */
    
    
    
    
}
