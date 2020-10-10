#include "ipv4.h"

#include "interface.h"


void ip_tx(const char* interface_name,
          ip_addr_t dst_ip,
          uint8_t ip_type,
          void* pkt_ptr,
          int pkt_size)
{
    interface_t* interface = get_interface_by_name(interface_name);

    ip_hdr_t* ip_hdr = (ip_hdr_t *)pkt_ptr;

    /* version and header length
     * version is 4 for ipv4
     * header length is assumed to be min ip header len (5 or 20bytes) */
    ip_hdr->ver_and_ihl = 0x45;

    /* type of service; set to 0 i.e. best effort */
    ip_hdr->tos = 0;

    /* total packet length */
    ip_hdr->tot_len = htons(pkt_size);

    /* fragmentation attributes */
    ip_hdr->identification = 0;
    ip_hdr->flags_and_offset = 0b010 << 13; //flag:010 (don't fragment); offset:0

    /* time to live */
    ip_hdr->ttl = 255; //default value

    /* payload protocol */
    ip_hdr->protocol = ip_type;

    /* header checksum */
    ip_hdr->hdr_checksum = 0; //is updated at the end

    /* source IP */
    memcpy(ip_hdr->src_ip.ip, interface->ip.ip, IP_ADDR_SIZE);

    /* destination IP */
    memcpy(ip_hdr->dst_ip.ip, dst_ip.ip, IP_ADDR_SIZE);

    uint8_t ihl = (ip_hdr->ver_and_ihl & 0b00001111) * 4; //in bytes
    ip_hdr->hdr_checksum = htons(checksum((unsigned char *)pkt_ptr, ihl));
}


void ip_rx(void* pkt_ptr)
{
    ip_hdr_t* ip_hdr = (ip_hdr_t *)pkt_ptr;

    /* verify header checksum */
    uint16_t recvd_checksum = ntohs(ip_hdr->hdr_checksum);
    ip_hdr->hdr_checksum = 0;

    uint8_t ihl = (ip_hdr->ver_and_ihl & 0b00001111) * 4; //in bytes
    uint16_t calculated_checksum = checksum((unsigned char *)ip_hdr, ihl);
    if (recvd_checksum != calculated_checksum) {
        fprintf(stderr, "Error ip_rx: checksum mismatch; packet dropped\n");
        return;
    }

    char* payload = (char *)pkt_ptr;
    payload += ihl;

    fprintf(stdout, "Rx IP Payload: ");
    fprintf(stdout, "%s\n", payload);
}
