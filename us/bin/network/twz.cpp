#include "twz.h"

#include "eth.h"
#include "ipv4.h"


void twz_tx(object_id_t object_id,
          uint16_t twz_type,
          void* pkt_ptr)
{
    twz_hdr_t* twz_hdr = (twz_hdr_t *)pkt_ptr;

    memcpy(twz_hdr->object_id.id, object_id.id, OBJECT_ID_SIZE);

    twz_hdr->type = htons(twz_type);
}


void twz_rx(const char* interface_name,
          void* pkt_ptr)
{
    twz_hdr_t* twz_hdr = (twz_hdr_t *)pkt_ptr;

    fprintf(stdout, "Received twizzler packet (id: ");
    for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
        fprintf(stdout, "%02X", twz_hdr->object_id.id[i]);
    }
    fprintf(stdout, ")\n");

    uint16_t twz_type = ntohs(twz_hdr->type);

    char* payload = (char *)pkt_ptr;
    payload += TWZ_HDR_SIZE;

    switch (twz_type) {
        case IPV4:
            ip_rx(interface_name, payload);
            break;

        default:
            fprintf(stderr, "twz_rx: unrecognized twizzler type %04X; "
                "packet dropped\n", twz_type);
    }
}
