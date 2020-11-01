#include "twz_op.h"

#include "interface.h"
#include "twz.h"
#include "send.h"

#define TWIZZLER_PORT 9090
#define CONTROLLER_PORT 9091
#define CONTROLLER_ADDR "10.0.0.4"


std::map<uint8_t*,uint8_t*> obj_mapping;
std::mutex obj_mapping_mutex;


void obj_mapping_insert(uint8_t* object_id,
                        uint8_t* mac_addr)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();
    uint8_t* key = (uint8_t *)malloc(sizeof(uint8_t)*OBJECT_ID_SIZE);
    uint8_t* value = (uint8_t *)malloc(sizeof(uint8_t)*MAC_ADDR_SIZE);
    memcpy(key, object_id, OBJECT_ID_SIZE);
    memcpy(value, mac_addr, MAC_ADDR_SIZE);
    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        obj_mapping[key] = value;
    } else {
        it->second = value;
    }
    obj_mapping_mutex.unlock();
}


uint8_t* obj_mapping_get(uint8_t* object_id)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();
    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        obj_mapping_mutex.unlock();
        return NULL;
    } else {
        obj_mapping_mutex.unlock();
        return it->second;
    }
}


void obj_mapping_delete(uint8_t* object_id)
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();
    bool found;
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        found = true;
        for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
            if (it->first[i] != object_id[i]) {
                found = false;
                break;
            }
        }
        if (found) {
            free(it->first);
            free(it->second);
            obj_mapping.erase(it);
            break;
        }
    }
    obj_mapping_mutex.unlock();
}


void obj_mapping_view()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();
    fprintf(stdout, "OBJ MAPPING Table:\n");
    fprintf(stdout, "------------------------------------------\n");
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        int i;
        for (i = 0; i < OBJECT_ID_SIZE-1; ++i) {
            fprintf(stdout, "%02X", it->first[i]);
        }
        fprintf(stdout, "%02X -> ", it->first[i]);

        for (i = 0; i < MAC_ADDR_SIZE-1; ++i) {
            fprintf(stdout, "%02X:", it->second[i]);
        }
        fprintf(stdout, "%02X\n", it->second[i]);
    }
    fprintf(stdout, "------------------------------------------\n");
    obj_mapping_mutex.unlock();
}


void obj_mapping_clear()
{
    std::map<uint8_t*,uint8_t*>::iterator it;

    obj_mapping_mutex.lock();
    for (it = obj_mapping.begin(); it != obj_mapping.end(); ++it) {
        obj_mapping.erase(it);
    }
    obj_mapping_mutex.unlock();
}


bool object_exists_locally(const char* interface_name,
                           object_id_t object_id)
{
    interface_t* interface = get_interface_by_name(interface_name);

    uint8_t* mac_addr = obj_mapping_get(object_id.id);

    if (mac_addr == NULL) return false;

    for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
        if (mac_addr[i] != interface->mac.mac[i]) {
            return false;
        }
    }

    return true;
}


int twz_op_send(const char* interface_name,
                object_id_t object_id,
                uint8_t twz_op,
                char* data)
{
    interface_t* interface = get_interface_by_name(interface_name);

    switch (twz_op) {
        case TWZ_ADVERT:
            if (true) {
                char* msg = (char *)malloc(sizeof(char)*45);
                char* p = msg;
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    msg += sprintf(msg, "%02X", object_id.id[i]);
                }
                for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
                    msg += sprintf(msg, "%02X", interface->mac.mac[i]);
                }
                msg += sprintf(msg, ":");
                fprintf(stdout, "Advertising object id ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stdout, "%02X", object_id.id[i]);
                }
                fprintf(stdout, " (%s)\n", p);
                int ret =  send_udp_packet(interface_name,
                                           object_id,
                                           NOOP,
                                           string_to_ip_addr(CONTROLLER_ADDR),
                                           TWIZZLER_PORT,
                                           CONTROLLER_PORT,
                                           p);
                free(p);
                return ret;
            }

        case TWZ_READ_REQ:
            fprintf(stdout, "Sending READ REQ for object id ");
            for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                fprintf(stdout, "%02X", object_id.id[i]);
            }
            fprintf(stdout, "\n");
            return send_udp_packet(interface_name,
                                   object_id,
                                   twz_op,
                                   interface->bcast_ip,
                                   TWIZZLER_PORT,
                                   TWIZZLER_PORT,
                                   data);

        case TWZ_WRITE_REQ:
            fprintf(stdout, "Sending WRITE REQ for object id ");
            for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                fprintf(stdout, "%02X", object_id.id[i]);
            }
            fprintf(stdout, "; payload: %s\n", data);
            return send_udp_packet(interface_name,
                                   object_id,
                                   twz_op,
                                   interface->bcast_ip,
                                   TWIZZLER_PORT,
                                   TWIZZLER_PORT,
                                   data);

        default:
            fprintf(stderr, "Error twz_op_send: unrecognized twz op %d\n", twz_op);
            return 3;

    }
}


void twz_op_recv(const char* interface_name,
                 remote_info_t* remote_info,
                 char* payload)
{
    switch (remote_info->twz_op) {
        case TWZ_READ_REQ:
            if (object_exists_locally(interface_name, remote_info->object_id)) {
                //TODO read object
                char read_data[12] = "Read object";

                //reply back
                int ret =  send_udp_packet(interface_name,
                                           remote_info->object_id,
                                           TWZ_READ_REPLY,
                                           remote_info->remote_ip,
                                           TWIZZLER_PORT,
                                           TWIZZLER_PORT,
                                           read_data);
            } else {
                fprintf(stdout, "Error twz_op_recv: object ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stdout, "%02X", remote_info->object_id.id[i]);
                }
                fprintf(stdout, " does not exist on this machine\n");
            }
            break;

        case TWZ_WRITE_REQ:
            if (object_exists_locally(interface_name, remote_info->object_id)) {
                //TODO write payload
                //reply back
                int len = strlen(payload);
                char* msg = (char *)malloc(sizeof(char)*(21+len));
                sprintf(msg, "Wrote object with - %s", payload);
                int ret =  send_udp_packet(interface_name,
                                           remote_info->object_id,
                                           TWZ_WRITE_REPLY,
                                           remote_info->remote_ip,
                                           TWIZZLER_PORT,
                                           TWIZZLER_PORT,
                                           msg);
                free(msg);
            } else {
                fprintf(stdout, "Error twz_op_recv: object ");
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    fprintf(stdout, "%02X", remote_info->object_id.id[i]);
                }
                fprintf(stdout, " does not exist on this machine\n");
            }
            break;

        case TWZ_READ_REPLY:
            fprintf(stdout, "Received READ REPLY from ('%d.%d.%d.%d', %d) "
                    "reply: %s\n", remote_info->remote_ip.ip[0],
                    remote_info->remote_ip.ip[1],
                    remote_info->remote_ip.ip[2],
                    remote_info->remote_ip.ip[3],
                    remote_info->remote_port,
                    payload);
            break;

        case TWZ_WRITE_REPLY:
            fprintf(stdout, "Received WRITE REPLY from ('%d.%d.%d.%d', %d) "
                    "reply: %s\n", remote_info->remote_ip.ip[0],
                    remote_info->remote_ip.ip[1],
                    remote_info->remote_ip.ip[2],
                    remote_info->remote_ip.ip[3],
                    remote_info->remote_port,
                    payload);
            break;
    }
}

