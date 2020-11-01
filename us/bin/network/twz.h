#ifndef __TWZ_H__
#define __TWZ_H__

#include "common.h"

#define TWZ_HDR_SIZE 19 //bytes

#define NOOP 0
#define TWZ_ADVERT 1
#define TWZ_READ_REQ 2
#define TWZ_READ_REPLY 3
#define TWZ_WRITE_REQ 4
#define TWZ_WRITE_REPLY 5

typedef struct __attribute__((__packed__)) twz_hdr {
    object_id_t object_id;
    uint8_t op;
    uint16_t type;
} twz_hdr_t;

void twz_tx(object_id_t object_id,
            uint8_t twz_op,
            uint16_t twz_type,
            void* pkt_ptr);

void twz_rx(const char* interface_name,
            remote_info_t* remote_info,
            void* pkt_ptr);

#endif
