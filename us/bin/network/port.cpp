#include "port.h"

#include "ipv4.h"


uint8_t udp_ports[65536] = {0};
uint8_t tcp_ports[65536] = {0};
std::mutex port_mtx;

void bind_to_port(uint16_t port_num,
                  uint8_t protocol)
{
    uint8_t* list;

    if (protocol == UDP) {
        list = udp_ports;
    } else if (protocol == TCP) {
        list = tcp_ports;
    } else {
        fprintf(stderr, "Error bind_to_port: unrecognized layer-4 protocol\n");
        exit(1);
    }

    if (port_num == 0) {
        fprintf(stderr, "Error bind_to_port: port number cannot be 0\n");
        exit(1);
    }

    port_mtx.lock();
    if (list[port_num] == 0) {
        list[port_num] = 1;
        port_mtx.unlock();
        return;
    } else {
        fprintf(stderr, "Error bind_to_port: port %d already in use\n", port_num);
        port_mtx.unlock();
        exit(1);
    }
}


uint16_t bind_to_random_port(uint8_t protocol)
{
    uint8_t* list;

    if (protocol == UDP) {
        list = udp_ports;
    } else if (protocol == TCP) {
        list = tcp_ports;
    } else {
        fprintf(stderr, "Error bind_to_port: unrecognized layer-4 protocol\n");
        exit(1);
    }

    port_mtx.lock();
    for (uint32_t i = 1; i < 65536; ++i) {
        if (list[i] == 0) {
            list[i] = 1;
            port_mtx.unlock();
            return i;
        }
    }

    fprintf(stderr, "Error bind_to_random_port: no free ports available\n");
    port_mtx.unlock();
    exit(1);
}


void free_port(uint16_t port_num,
               uint8_t protocol)
{
    uint8_t* list;

    if (protocol == UDP) {
        list = udp_ports;
    } else if (protocol == TCP) {
        list = tcp_ports;
    } else {
        fprintf(stderr, "Error bind_to_port: unrecognized layer-4 protocol\n");
        exit(1);
    }

    port_mtx.lock();
    if (list[port_num] == 1) {
        list[port_num] = 0;
        port_mtx.unlock();
        return;
    }
}

