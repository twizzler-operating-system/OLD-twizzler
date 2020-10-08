#include "common.h"
#include "interface.h"
#include "eth.h"
#include "ipv4.h"
#include "send.h"
#include "loopback_link.h"


int main(int argc, char* argv[])
{
    ip_addr_t interface_ip;

    if (argc > 1) {
        /* extract IP address */
        interface_ip = convert_ip_addr(argv[1]);
    } else {
        fprintf(stderr, "usage: network ip_address <program_name> <args>\n");
        exit(1);
    }

    /* initialize a new  network interface */
    init_interface("/dev/e1000", "/dev/e1000-info", interface_ip);

    /* start receiving packets on the new interface */
    std::thread recv_thread (eth_rx, "/dev/e1000");

#ifdef LOOPBACK_TESTING
    /* start a simulated network link (for loopback testing) */
    interface_t* interface = get_interface_by_name("/dev/e1000");
    std::thread link (loopback_network_link,
            &interface->tx_queue_obj, &interface->rx_queue_obj);
    std::thread free_pkt_memory (free_packet_buffer_object,
            &interface->rx_queue_obj);
#else
    std::thread free_pkt_memory (free_packet_buffer_object,
            &interface->tx_queue_obj);
#endif

    /* Run specified network program */
    if (argc > 2) {
        if (strcmp(argv[2], "ipv4") == 0) {
            if (argc > 3) {
                int count = 0;
                while (count < 1) {
                    ++count;
                    char payload[] = "This is test data.";
                    fprintf(stdout, "Sending out an ipv4 packet\n");
                    int ret = send_ipv4_packet("/dev/e1000", convert_ip_addr(argv[3]), TCP, payload);
                    if (ret == ARP_TIMEOUT_ERROR) {
                        fprintf(stderr, "ARP TIMEOUT ERROR\n");
                    }
                    usleep(1000000);
                }

            } else {
                fprintf(stderr, "Error: ipv4 program requires dst ip addr as argument\n");
                exit(1);
            }

        } else {
            fprintf(stderr, "Error: unrecognized program name\n");
            exit(1);
        }
    }

    for (;;)
        usleep(100000);
}
