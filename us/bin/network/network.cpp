#include "common.h"
#include "interface.h"
#include "eth.h"
#include "ipv4.h"
#include "twz.h"
#include "send.h"
#include "loopback_link.h"


int main(int argc, char* argv[])
{
    ip_addr_t interface_ip;

    if (argc > 1) {
        /* extract IP address */
        interface_ip = convert_ip_addr(argv[1]);
    } else {
        fprintf(stderr, "usage: network host_ip_address <program_name> <args>\n");
        exit(1);
    }

    /* initialize a new  network interface */
    init_interface("/dev/e1000", "/dev/e1000-info", interface_ip);

    /* start receiving packets on the new interface */
    std::thread recv_thread (eth_rx, "/dev/e1000");

    interface_t* interface = get_interface_by_name("/dev/e1000");
#ifdef LOOPBACK_TESTING
    /* start a simulated network link (for loopback testing) */
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
        if (strcmp(argv[2], "udp") == 0) {
            if (argc == 6) {
                object_id_t object_id;
                int count = 0;
                while (count < 50) {
                    ++count;
                    char payload[] = "This is test data.";
                    fprintf(stdout, "Sending out a UDP packet...\n");
                    int ret = send_udp_packet
                        ("/dev/e1000", object_id, NOOP,
                         convert_ip_addr(argv[3]),
                         atoi(argv[4]),
                         atoi(argv[5]),
                         payload);
                    int64_t counter = 0;
                    while (ret == EARP_WAIT) {
                        usleep(10);
                        ret = send_udp_packet
                            ("/dev/e1000", object_id, NOOP,
                             convert_ip_addr(argv[3]),
                             atoi(argv[4]),
                             atoi(argv[5]),
                             payload);
                        ++counter;
                        if (counter*10 == ARP_TIMEOUT) {
                            fprintf(stderr, "Error ARP failed; message not sent\n");
                            break;
                        }
                    }
                    usleep(1000000);
                }

            } else {
                fprintf(stderr, "Error: udp program requires 3 arguments - "
                        "dst_ip_addr src_port dst_port\n");
                exit(1);
            }

        } else if (strcmp(argv[2], "twz") == 0) {
            if (argc > 3) {
                int count = 0;
                while (count < 50) {
                    ++count;
                    char payload[] = "This is test data.";
                    object_id_t object_id;
                    for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                        object_id.id[i] = interface->mac.mac[i % MAC_ADDR_SIZE];
                    }
                    fprintf(stdout, "Sending out a twizzler packet\n");
                    int ret = send_twz_packet
                        ("/dev/e1000", object_id,
                         convert_ip_addr(argv[3]), TCP, payload);
                    usleep(1000000);
                }

            } else {
                fprintf(stderr, "Error: twz program requires "
                        "dst_ip_addr as argument\n");
                exit(1);
            }

        } else {
            fprintf(stderr, "Error: unrecognized program name\n");
            exit(1);
        }
    }

    for (;;)
        usleep(100000);

    return 0;
}
