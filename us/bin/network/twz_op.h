#ifndef __TWZ_OP_H__
#define __TWZ_OP_H__

#include "common.h"

int twz_op_send(const char* interface_name,
                object_id_t object_id,
                uint8_t twz_op,
                char* data);

void twz_op_recv(const char* interface_name,
                 remote_info_t* remote_info,
                 char* payload);

void obj_mapping_insert(uint8_t* object_id,
                        uint8_t* mac_addr);

uint8_t* obj_mapping_get(uint8_t* object_id);

void obj_mapping_delete(uint8_t* object_id);

void obj_mapping_view();

void obj_mapping_clear();

bool object_exists_locally(const char* interface_name,
                           object_id_t object_id);

#endif
