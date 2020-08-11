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


void flip_send(twzobj *hdr_obj, uint8_t meta1, uint8_t meta2, uint8_t meta3, char *dst_ip, char *data)
{
    flip_l3_hdr1_t hdr1;
    hdr1.version = 4;
    
}
