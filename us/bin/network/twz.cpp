#include "twz.h"

#include "eth.h"
#include "ipv4.h"


void twz_tx(object_id_t object_id,
            uint8_t twz_op,
            uint16_t twz_type,
            void* pkt_ptr)
{
    twz_hdr_t* twz_hdr = (twz_hdr_t *)pkt_ptr;

    memcpy(twz_hdr->object_id.id, object_id.id, OBJECT_ID_SIZE);

    twz_hdr->op = twz_op;

    twz_hdr->type = htons(twz_type);
}


void twz_rx(const char* interface_name,
            remote_info_t* remote_info,
            void* pkt_ptr)
{
    twz_hdr_t* twz_hdr = (twz_hdr_t *)pkt_ptr;

    remote_info->object_id = twz_hdr->object_id;
    remote_info->twz_op = twz_hdr->op;

    uint16_t twz_type = ntohs(twz_hdr->type);

    char* payload = (char *)pkt_ptr;
    payload += TWZ_HDR_SIZE;

    switch (twz_type) {
        case IPV4:
            ip_rx(interface_name, remote_info, payload);
            break;

        default:
            fprintf(stderr, "twz_rx: unrecognized twizzler type %04X; "
                "packet dropped\n", twz_type);
    }
}
