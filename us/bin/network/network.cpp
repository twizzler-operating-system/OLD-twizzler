#include "common.h"
#include "interface.h"
#include "eth.h"
#include "ipv4.h"
#include "twz.h"
#include "send.h"
#include "twz_op.h"


int main(int argc, char* argv[])
{
    ip_addr_t interface_ip;
    ip_addr_t interface_bcast_ip;

    if (argc > 2) {
        /* extract IP address */
        interface_ip = string_to_ip_addr(argv[1]);
        interface_bcast_ip = string_to_ip_addr(argv[2]);
    } else {
        fprintf(stderr, "usage: network host_ip_address host_broadcast_ip "
                "<program_name> <args>\n");
        exit(1);
    }

    /* initialize a new network interface */
    init_interface("/dev/e1000", "/dev/e1000-info",
            interface_ip, interface_bcast_ip);

    /* start receiving packets on the new interface */
    std::thread recv_thread (eth_rx, "/dev/e1000");

    /* free packet memory asynchronously */
    interface_t* interface = get_interface_by_name("/dev/e1000");
    std::thread free_pkt_memory (free_packet_buffer_object,
            &interface->tx_queue_obj);

///////////////////////////////////////////////////////////////////////////////////

    /* Run specified network program */
    if (argc > 3) {
        if (strcmp(argv[3], "udp") == 0) {
            if (argc == 7) {
                object_id_t object_id;
                int count = 0;
                while (count < 50) {
                    ++count;
                    char payload[] = "This is test data.";
                    fprintf(stderr, "Sending out a UDP packet...\n");
                    int ret = send_udp_packet
                        ("/dev/e1000", object_id, NOOP,
                         string_to_ip_addr(argv[4]),
                         atoi(argv[5]),
                         atoi(argv[6]),
                         payload);
                    int64_t counter = 0;
                    while (ret == EARP_WAIT) {
                        usleep(10);
                        ret = send_udp_packet
                            ("/dev/e1000", object_id, NOOP,
                             string_to_ip_addr(argv[4]),
                             atoi(argv[5]),
                             atoi(argv[6]),
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

        } else if (strcmp(argv[3], "twz") == 0) {
            if (argc == 5) {
                //populate local object mappings
                object_id_t object_id;
                for (int i = 0; i < OBJECT_ID_SIZE; ++i) {
                    object_id.id[i] = interface->mac.mac[i % MAC_ADDR_SIZE];
                }
                obj_mapping_insert(object_id.id, interface->ip.ip);

                if (strcmp(argv[4], "0") == 0
                || strcmp(argv[4], "1") == 0) {
                    if (strcmp(argv[1], "10.0.0.1") == 0) { //read/write
                        int count = 0;
                        object_id_t obj_id_1 = (object_id_t) {
                            .id = {0x08, 0x00, 0x00, 0x00, 0x02, 0x22,
                                   0x08, 0x00, 0x00, 0x00, 0x02, 0x22,
                                   0x08, 0x00, 0x00, 0x00}
                        };
                        object_id_t obj_id_2 = (object_id_t) {
                            .id = {0x08, 0x00, 0x00, 0x00, 0x03, 0x33,
                                   0x08, 0x00, 0x00, 0x00, 0x03, 0x33,
                                   0x08, 0x00, 0x00, 0x00}
                        };
                        char payload[5];
                        while (count < 1000) {
                            ++count;
                            sprintf(payload, "%d", count);
                            int ret;
                            if (count % 2 == 0) {
                                ret = twz_op_send("/dev/e1000", obj_id_1,
                                        TWZ_READ_REQ, payload,
                                        atoi(argv[4]));
                            } else {
                                ret = twz_op_send("/dev/e1000", obj_id_2,
                                        TWZ_WRITE_REQ, payload,
                                        atoi(argv[4]));
                            }
                            int64_t counter = 0;
                            while (ret == EARP_WAIT) {
                                usleep(10);
                                if (count % 2 == 0) {
                                    ret = twz_op_send("/dev/e1000", obj_id_1,
                                            TWZ_READ_REQ, payload,
                                            atoi(argv[4]));
                                } else {
                                    ret = twz_op_send("/dev/e1000", obj_id_2,
                                            TWZ_WRITE_REQ, payload,
                                            atoi(argv[4]));
                                }
                                ++counter;
                                if (counter*10 == ARP_TIMEOUT) {
                                    fprintf(stderr, "Error ARP failed; "
                                            "message not sent\n");
                                    break;
                                }
                            }
                            usleep(1000000);
                        }

                    } else if (strcmp(argv[4], "0") == 0){ //advertise
                        usleep(20000000);
                        int ret = twz_op_send("/dev/e1000", object_id,
                                TWZ_ADVERT, NULL, atoi(argv[4]));
                        int64_t counter = 0;
                        while (ret == EARP_WAIT) {
                            usleep(1000000);
                            ret = twz_op_send("/dev/e1000", object_id,
                                    TWZ_ADVERT, NULL, atoi(argv[4]));
                            ++counter;
                            if (counter == 10) {
                            fprintf(stderr, "Error ARP failed; "
                                        "message not sent\n");
                                break;
                            }
                        }
                    }
                }

            } else {
                fprintf(stderr, "Error: twz program requires "
                        "[0/1] as argument\n");
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
