#include "common.h"


static uint64_t id_counter = 0;

static std::mutex mtx;


endianess_t check_machine_endianess()
{
    int32_t n = 1;
    if (*(char *)&n == 1) {
        return LITTLEENDIAN;
    } else {
        return BIGENDIAN;
    }
}


uint32_t ntohl(uint32_t n)
{
    if (check_machine_endianess() == BIGENDIAN) {
        return n;
    }

    uint8_t data[4] = {};
    memcpy(&data, &n, sizeof(data));
    return ((uint32_t) data[3] << 0)
            | ((uint32_t) data[2] << 8)
            | ((uint32_t) data[1] << 16)
            | ((uint32_t) data[0] << 24);
}


uint16_t ntohs(uint16_t n)
{
    if (check_machine_endianess() == BIGENDIAN) {
        return n;
    }

    uint8_t data[2] = {};
    memcpy(&data, &n, sizeof(data));
    return ((uint16_t) data[1] << 0)
            | ((uint16_t) data[0] << 8);
}


uint32_t htonl(uint32_t n)
{
    if (check_machine_endianess() == BIGENDIAN) {
        return n;
    }

    uint8_t data[4] = {};
    memcpy(&data, &n, sizeof(data));
    return ((uint32_t) data[3] << 0)
            | ((uint32_t) data[2] << 8)
            | ((uint32_t) data[1] << 16)
            | ((uint32_t) data[0] << 24);
}


uint16_t htons(uint16_t n)
{
    if (check_machine_endianess() == BIGENDIAN) {
        return n;
    }

    uint8_t data[2] = {};
    memcpy(&data, &n, sizeof(data));
    return ((uint16_t) data[1] << 0)
            | ((uint16_t) data[0] << 8);
}


bool compare_mac_addr(mac_addr_t their_mac,
                      mac_addr_t my_mac)
{
    int count = 0;
    for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
        if (their_mac.mac[i] != 0xFF) {
            break;
        } else {
            count += 1;
        }
    }

    if (count == MAC_ADDR_SIZE) { /* broadcast mac addr */
        return true;

    } else {
        for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
            if (their_mac.mac[i] != my_mac.mac[i]) {
                return false;
            }
        }
        return true;
    }
}


bool compare_ip_addr(ip_addr_t their_ip,
                     ip_addr_t my_ip,
                     ip_addr_t bcast_ip)
{
    int count = 0;
    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        if (their_ip.ip[i] != bcast_ip.ip[i]) {
            break;
        } else {
            count += 1;
        }
    }

    if (count == IP_ADDR_SIZE) { /* broadcast ip addr */
        return true;

    } else {
        for (int i = 0; i < IP_ADDR_SIZE; ++i) {
            if (their_ip.ip[i] != my_ip.ip[i]) {
                return false;
            }
        }
        return true;
    }
}


mac_addr_t string_to_mac_addr(char* mac_addr)
{
    uint8_t my_mac[MAC_ADDR_SIZE] = {0};

    for (int i = 0; i < MAC_ADDR_SIZE; ++i) {
        char p[3];
        strncpy(p, (mac_addr+(3*i)), 2);
        my_mac[i] = strtol(p, NULL, 16);
    }

    mac_addr_t mac;
    memcpy(mac.mac, my_mac, MAC_ADDR_SIZE);

    return mac;
}


ip_addr_t string_to_ip_addr(char* ip_addr)
{
    uint8_t my_ip[IP_ADDR_SIZE] = {0};
    int index = 0;

    while (*ip_addr) {
        if (isdigit((unsigned char)*ip_addr)) {
            my_ip[index] *= 10;
            my_ip[index] += *ip_addr - '0';
        } else {
            index++;
        }
        ip_addr++;
    }

    ip_addr_t ip;
    memcpy(ip.ip, my_ip, IP_ADDR_SIZE);

    return ip;
}


uint16_t checksum(unsigned char* data,
                  uint16_t len)
{
    uint32_t sum = 0;
    int i = 0;

    while (len > 1) {
        sum += *((uint16_t *)data + i);
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        i += 1;
        len -= 2;
    }

    if (len) {
        sum += (uint16_t) *(unsigned char *)data;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)~sum;
}


uint64_t get_id()
{
    return ++id_counter;
}


void* allocate_packet_buffer_object(uint16_t pkt_size)
{
    twzobj pkt_buffer_obj;
    if (twz_object_new(&pkt_buffer_obj, NULL, NULL,
    TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE) < 0) {
        fprintf(stderr, "Error create_packet_buffer_object: "
                "cannot create packet buffer object\n");
        exit(1);
    }
    mtx.lock();
    twz_object_build_alloc(&pkt_buffer_obj, 0);
    mtx.unlock();
    void *p = twz_object_alloc(&pkt_buffer_obj, pkt_size);
    void *pkt_ptr = twz_object_lea(&pkt_buffer_obj, p);

    return pkt_ptr;
}


void free_packet_buffer_object(twzobj* queue_obj)
{
    while (true) {
        struct packet_queue_entry pqe;

        /* dequeue the completion entry from completion queue of queue_obj */
        queue_get_finished(queue_obj, (struct queue_entry *)&pqe, 0);

        //TODO free pqe
    }
}


