#include "tcp_conn.h"

#include "interface.h"
#include "twz.h"
#include "ipv4.h"
#include "port.h"
#include "encapsulate.h"


struct cmp1 {
    bool operator()(const half_conn_t a, const half_conn_t b) const {
        return a.port < b.port;
    }
};

std::map<half_conn_t,std::vector<outstanding_conn_t>,cmp1>outstanding_conns;
std::mutex outstanding_conns_mutex;


bool compare_half_conn(half_conn_t half_conn_1,
                       half_conn_t half_conn_2)
{
    ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

    if (!compare_ip_addr(half_conn_1.ip, default_ip, default_ip)
    && !compare_ip_addr(half_conn_2.ip, default_ip, default_ip)) {
        for (int i = 0; i < IP_ADDR_SIZE; ++i) {
            if (half_conn_1.ip.ip[i] != half_conn_2.ip.ip[i]) {
                return false;
            }
        }
    }

    if (half_conn_1.port != half_conn_2.port) {
        return false;
    }

    return true;
}


void outstanding_conns_enqueue(half_conn_t half_conn,
                               outstanding_conn_t outstanding_conn)
{
    std::map<half_conn_t,std::vector<outstanding_conn_t>>::iterator it;

    outstanding_conns_mutex.lock();

    bool found = false;
    for (it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
        if (compare_half_conn(half_conn, it->first)) {
            found = true;
            break;
        }
    }
    if (!found) {
        std::vector<outstanding_conn_t> outstanding_conn_list;
        outstanding_conn_list.push_back(outstanding_conn);
        outstanding_conns[half_conn] = outstanding_conn_list;
    } else {
        it->second.push_back(outstanding_conn);
    }

    outstanding_conns_mutex.unlock();
}


outstanding_conn_t outstanding_conns_dequeue(half_conn_t half_conn)
{
    std::map<half_conn_t,std::vector<outstanding_conn_t>>::iterator it;

    outstanding_conns_mutex.lock();

    bool found = false;
    for (it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
        if (compare_half_conn(half_conn, it->first)) {
            found = true;
            break;
        }
    }

    outstanding_conn_t conn;
    if (!found) {
        conn.is_valid = false;
    } else {
        conn = it->second.front();
        it->second.erase(it->second.begin());
    }

    outstanding_conns_mutex.unlock();

    return conn;
}


void outstanding_conns_delete(half_conn_t half_conn)
{
    std::map<half_conn_t,std::vector<outstanding_conn_t>>::iterator it;

    outstanding_conns_mutex.lock();

    bool found = false;
    for (it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
        if (compare_half_conn(half_conn, it->first)) {
            found = true;
            break;
        }
    }

    if (found) {
        outstanding_conns.erase(it);
    }

    outstanding_conns_mutex.unlock();
}


void outstanding_conns_view()
{
    std::map<half_conn_t,std::vector<outstanding_conn_t>>::iterator it;

    outstanding_conns_mutex.lock();

    fprintf(stderr, "OUTSTANDING CONN TABLE:\n");
    fprintf(stderr, "------------------------------------------\n");
    for (it = outstanding_conns.begin(); it != outstanding_conns.end(); ++it) {
        fprintf(stderr, "[%u.%u.%u.%u, %u] -> [ ",
                (it->first.ip.ip[0] & 0x000000FF),
                (it->first.ip.ip[1] & 0x000000FF),
                (it->first.ip.ip[2] & 0x000000FF),
                (it->first.ip.ip[3] & 0x000000FF),
                (it->first.port & 0x0000FFFF));
        std::vector<outstanding_conn_t>::iterator it1;
        for (it1 = it->second.begin(); it1 != it->second.end(); ++it1) {
            fprintf(stderr, "[%d, %u.%u.%u.%u, %u, %u] ",
                    it1->is_valid,
                    (it1->remote.ip.ip[0] & 0x000000FF),
                    (it1->remote.ip.ip[1] & 0x000000FF),
                    (it1->remote.ip.ip[2] & 0x000000FF),
                    (it1->remote.ip.ip[3] & 0x000000FF),
                    (it1->remote.port & 0x0000FFFF),
                    it1->seq_num);
        }
        fprintf(stderr, "]\n");
    }
    fprintf(stderr, "------------------------------------------\n");

    outstanding_conns_mutex.unlock();
}


struct cmp2 {
    bool operator()(const tcp_conn_t a, const tcp_conn_t b) const {
        return a.local.port < b.local.port;
    }
};


std::map<tcp_conn_t,tcp_conn_state_t*,cmp2> conn_table;
std::map<tcp_conn_t,tcp_conn_state_t*,cmp2>::iterator head;
std::vector<tcp_conn_state_t*> inactive_conn_list;
bool conn_table_bootstrap_done = false;
std::mutex conn_table_mutex;


bool compare_tcp_conn(tcp_conn_t conn_1,
                      tcp_conn_t conn_2)
{
    ip_addr_t default_ip = string_to_ip_addr(DEFAULT_IP);

    if (!compare_ip_addr(conn_1.local.ip, default_ip, default_ip)
    && !compare_ip_addr(conn_2.local.ip, default_ip, default_ip)) {
        for (int i = 0; i < IP_ADDR_SIZE; ++i) {
            if (conn_1.local.ip.ip[i] != conn_2.local.ip.ip[i]) {
                return false;
            }
        }
    }

    assert(!compare_ip_addr(conn_1.remote.ip, default_ip, default_ip)
    && !compare_ip_addr(conn_2.remote.ip, default_ip, default_ip));
    for (int i = 0; i < IP_ADDR_SIZE; ++i) {
        if (conn_1.remote.ip.ip[i] != conn_2.remote.ip.ip[i]) {
            return false;
        }
    }

    if (conn_1.local.port != conn_2.local.port) {
        return false;
    }

    if (conn_1.remote.port != conn_2.remote.port) {
        return false;
    }

    return true;
}


void conn_table_put(tcp_conn_t conn,
                    tcp_conn_state_t* conn_state)
{
    std::map<tcp_conn_t,tcp_conn_state_t*>::iterator it;

    conn_table_mutex.lock();

    bool found = false;
    for (it = conn_table.begin(); it != conn_table.end(); ++it) {
        if (compare_tcp_conn(conn, it->first)) {
            found = true;
            break;
        }
    }

    if (!found) {
        conn_table[conn] = conn_state;
    } else {
        it->second = conn_state;
    }

    if (head == conn_table.begin()) {
        ++head;
    }

    conn_table_mutex.unlock();
}


tcp_conn_state_t* conn_table_get(tcp_conn_t conn)
{
    std::map<tcp_conn_t,tcp_conn_state_t*>::iterator it;

    conn_table_mutex.lock();

    bool found = false;
    for (it = conn_table.begin(); it != conn_table.end(); ++it) {
        if (compare_tcp_conn(conn, it->first)) {
            found = true;
            break;
        }
    }

    conn_table_mutex.unlock();

    if (!found) {
        return NULL;
    } else {
        return it->second;
    }
}


void conn_table_delete(tcp_conn_t conn)
{
    std::map<tcp_conn_t,tcp_conn_state_t*>::iterator it;

    conn_table_mutex.lock();

    bool found = false;
    for (it = conn_table.begin(); it != conn_table.end(); ++it) {
        if (compare_tcp_conn(conn, it->first)) {
            found = true;
            break;
        }
    }

    if (found) {
        tcp_conn_state_t* conn_state = it->second;

        //pthread_spin_destroy(&conn_state->mtx);
        //free_char_ring_buffer(conn_state->tx_buffer);
        //free_char_ring_buffer(conn_state->rx_buffer);
        //free(conn_state);
        inactive_conn_list.push_back(conn_state); //TODO free conn_state

        conn_table.erase(it);
    }

    if (head == it) {
        if (++head == conn_table.end()) {
            head = conn_table.begin();
            if (++head == conn_table.end()) {
                head = conn_table.begin();
            }
        }
    }

    conn_table_mutex.unlock();
}


void conn_table_view()
{
    std::map<tcp_conn_t,tcp_conn_state_t*>::iterator it;

    conn_table_mutex.lock();

    fprintf(stderr, "TCP CONN TABLE:\n");
    fprintf(stderr, "------------------------------------------\n");
    for (it = conn_table.begin(); it != conn_table.end(); ++it) {
        pthread_spin_lock(&it->second->mtx);
        fprintf(stderr, "[%u.%u.%u.%u, %u.%u.%u.%u, %u, %u] -> "
                "[seq_num: %u, ack_num: %u]\n",
                (it->first.local.ip.ip[0] & 0x000000FF),
                (it->first.local.ip.ip[1] & 0x000000FF),
                (it->first.local.ip.ip[2] & 0x000000FF),
                (it->first.local.ip.ip[3] & 0x000000FF),
                (it->first.remote.ip.ip[0] & 0x000000FF),
                (it->first.remote.ip.ip[1] & 0x000000FF),
                (it->first.remote.ip.ip[2] & 0x000000FF),
                (it->first.remote.ip.ip[3] & 0x000000FF),
                (it->first.local.port & 0x0000FFFF),
                (it->first.remote.port & 0x0000FFFF),
                it->second->seq_num,
                it->second->ack_num);
        pthread_spin_unlock(&it->second->mtx);
    }
    fprintf(stderr, "------------------------------------------\n");

    conn_table_mutex.unlock();
}


tcp_conn_state_t* get_next_conn_state(tcp_conn_t* conn)
{
    tcp_conn_state_t* conn_state = NULL;

    conn_table_mutex.lock();

    if (conn_table_bootstrap_done && head != conn_table.begin()) {
        memcpy(conn->local.ip.ip, head->first.local.ip.ip, IP_ADDR_SIZE);
        memcpy(conn->remote.ip.ip, head->first.remote.ip.ip, IP_ADDR_SIZE);
        conn->local.port = head->first.local.port;
        conn->remote.port = head->first.remote.port;

        ++head;
        if (head == conn_table.end()) {
            head = conn_table.begin();
            ++head;
        }

        conn_state = head->second;
    }

    conn_table_mutex.unlock();

    return conn_state;
}


void bootstrap_conn_table()
{
    tcp_conn_t conn;
    memcpy(conn.local.ip.ip, "0.0.0.0", IP_ADDR_SIZE);
    memcpy(conn.remote.ip.ip, "0.0.0.0", IP_ADDR_SIZE);
    conn.local.port = 0;
    conn.remote.port = 0;

    conn_table_put(conn, NULL);

    conn_table_mutex.lock();
    head = conn_table.begin();
    conn_table_mutex.unlock();

    conn_table_bootstrap_done = true;
}


int establish_tcp_conn_client(half_conn_t* local,
                              half_conn_t remote)
{
    char interface_name[MAX_INTERFACE_NAME_SIZE];
    ip_table_get(remote.ip, interface_name);
    if (interface_name == NULL) {
        fprintf(stderr, "Error establish_tcp_conn_client: "
                "ip_table_get returned no valid interface\n");
        exit(1);
    }
    interface_t* interface = get_interface_by_name(interface_name);

    tcp_conn_t conn;
    memcpy(conn.local.ip.ip, interface->ip.ip, IP_ADDR_SIZE);
    memcpy(conn.remote.ip.ip, remote.ip.ip, IP_ADDR_SIZE);
    conn.local.port = bind_to_random_tcp_port();
    conn.remote.port = remote.port;

    tcp_conn_state_t* conn_state =
        (tcp_conn_state_t *)malloc(sizeof(tcp_conn_state_t));

    pthread_spin_init(&conn_state->mtx, 0);
    conn_state->curr_state = SYN_SENT;
    conn_state->seq_num = 0;
    conn_state->ack_num = 0;
    conn_state->tx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
    conn_state->rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
    uint32_t seq_num = conn_state->seq_num;
    uint32_t ack_num = conn_state->ack_num;

    conn_table_put(conn, conn_state);

    object_id_t object_id;
    /* send SYN packet */
    int ret = encap_tcp_packet(object_id, NOOP, conn.local.ip, conn.remote.ip,
                conn.local.port, conn.remote.port, SYN_PKT, seq_num, ack_num,
                NULL, 0);
    if (ret != 0) {
        free_tcp_port(conn.local.port);
        cleanup_conn_state(conn);
        return ETCPCONNFAILED;
    }

    clock_t start = clock();
    while (conn_state->curr_state != SYN_ACK_RECVD) {
        usleep(10);

        clock_t time_elapsed_msec
            = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
        if (time_elapsed_msec >= TCP_TIMEOUT) {
            free_tcp_port(conn.local.port);
            cleanup_conn_state(conn);
            return ETCPCONNFAILED;
        }
    }

    /* send ACK packet */
    ret = encap_tcp_packet(object_id, NOOP, conn.local.ip, conn.remote.ip,
            conn.local.port, conn.remote.port, ACK_PKT, conn_state->seq_num,
            conn_state->ack_num, NULL, 0);
    if (ret != 0) {
        free_tcp_port(conn.local.port);
        cleanup_conn_state(conn);
        return ETCPCONNFAILED;
    }

    pthread_spin_lock(&conn_state->mtx);
    conn_state->curr_state = CONN_ESTABLISHED;
    assert(conn_state->seq_num == 1
           && conn_state->tx_buffer->head == 1
           && conn_state->tx_buffer->tail == 1
           && conn_state->rx_buffer->head == 1
           && conn_state->rx_buffer->tail == 1);
    pthread_spin_unlock(&conn_state->mtx);

    if (local != NULL) {
        memcpy(local->ip.ip, conn.local.ip.ip, IP_ADDR_SIZE);
        local->port = conn.local.port;
    }

    return 0;
}


int establish_tcp_conn_server(half_conn_t local,
                              half_conn_t* remote)
{
    while (true) {
        outstanding_conn_t remote_conn;
        do {
            remote_conn = outstanding_conns_dequeue(local);
            if (!remote_conn.is_valid) usleep(10);
        } while (!remote_conn.is_valid);

        tcp_conn_t conn;
        memcpy(conn.local.ip.ip, local.ip.ip, IP_ADDR_SIZE);
        memcpy(conn.remote.ip.ip, remote_conn.remote.ip.ip, IP_ADDR_SIZE);
        conn.local.port = local.port;
        conn.remote.port = remote_conn.remote.port;

        if (conn_table_get(conn) != NULL) { /* connection already exists! */
            continue;
        }

        tcp_conn_state_t* conn_state
            = (tcp_conn_state_t *)malloc(sizeof(tcp_conn_state_t));

        pthread_spin_init(&conn_state->mtx, 0);
        conn_state->curr_state = SYN_ACK_SENT;
        conn_state->seq_num = 0;
        conn_state->ack_num = remote_conn.seq_num + 1;
        conn_state->tx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
        conn_state->rx_buffer = create_char_ring_buffer(CHAR_RING_BUFFER_SIZE);
        uint32_t seq_num = conn_state->seq_num;
        uint32_t ack_num = conn_state->ack_num;

        conn_table_put(conn, conn_state);

        object_id_t object_id;
        /* send SYN-ACK packet */
        int ret = encap_tcp_packet(object_id, NOOP, conn.local.ip, conn.remote.ip,
                    conn.local.port, conn.remote.port, SYN_ACK_PKT, seq_num,
                    ack_num, NULL, 0);
        if (ret != 0) {
            cleanup_conn_state(conn);
            /* re-enqueue to try again */
            outstanding_conns_enqueue(local, remote_conn);
            continue;
        }

        clock_t start = clock();
        bool recvd_ack = true;
        while (conn_state->curr_state != ACK_RECVD) {
            usleep(10);

            clock_t time_elapsed_msec
                = ((clock() - start) * 1000) / CLOCKS_PER_SEC;
            if (time_elapsed_msec >= TCP_TIMEOUT) {
                cleanup_conn_state(conn);
                recvd_ack = false;
                break;
            }
        }
        if (!recvd_ack) { /* issue with the client; don't re-enqueue;
                           * instead try some other outstanding conn */
            continue;
        }

        pthread_spin_lock(&conn_state->mtx);
        conn_state->curr_state = CONN_ESTABLISHED;
        assert(conn_state->seq_num == 1
               && conn_state->tx_buffer->head == 1
               && conn_state->tx_buffer->tail == 1
               && conn_state->rx_buffer->head == 1
               && conn_state->rx_buffer->tail == 1);
        pthread_spin_unlock(&conn_state->mtx);

        if (remote != NULL) {
            memcpy(remote->ip.ip, conn.remote.ip.ip, IP_ADDR_SIZE);
            remote->port = conn.remote.port;
        }

        return 0;
    }
}


void send_tcp_data()
{
    fprintf(stderr, "Started TCP send thread\n");

    srand(time(NULL));

    while (true) {
        usleep(100000);

        tcp_conn_t conn;
        tcp_conn_state_t* conn_state = get_next_conn_state(&conn);

        if (conn_state == NULL) {
            continue;
        }

        if (conn_state->curr_state == CONN_ESTABLISHED) {
            char_ring_buffer_t* tx_buffer = conn_state->tx_buffer;
            uint32_t seq_num = conn_state->seq_num;
            uint32_t ack_num = conn_state->ack_num;

            uint32_t avail_bytes = occupied_space(tx_buffer, &seq_num);

            uint32_t bytes_to_send = (avail_bytes < MSS) ? avail_bytes : MSS;

            if (bytes_to_send > 0) {
                char* payload = (char *)malloc(sizeof(char)*bytes_to_send);

                uint32_t bytes = char_ring_buffer_get
                    (tx_buffer, payload, &seq_num, bytes_to_send);
                assert(bytes == bytes_to_send);

                /* send TCP packet */
                fprintf(stderr, "Sending TCP packet with payload = %s", payload);
                int p = 10000 * 0.7; /* 70% drop probability */
                int r = rand() % 10000;
                if (r < p) {
                    fprintf(stderr, " -- Packet Dropped!!\n");
                } else {
                    object_id_t object_id;
                    int ret = encap_tcp_packet(object_id, NOOP, conn.local.ip,
                                conn.remote.ip, conn.local.port, conn.remote.port,
                                DATA_PKT, seq_num, ack_num, payload, bytes_to_send);
                    if (ret == 0) {
                        fprintf(stderr, "\n");
                    } else {
                        fprintf(stderr, " -- Packet Dropped!!\n");
                    }
                }

                free(payload);
            }

            /* update seq num */
            if (seq_num == tx_buffer->head) {
                pthread_spin_lock(&conn_state->mtx);
                tx_buffer->time_of_head_change = clock();
                pthread_spin_unlock(&conn_state->mtx);
            }
            clock_t time_elapsed_msec
                = ((clock() - tx_buffer->time_of_head_change) * 1000)
                    / CLOCKS_PER_SEC;
            if (time_elapsed_msec >= TCP_TIMEOUT) {
                conn_state->seq_num = tx_buffer->head;
            } else {
                conn_state->seq_num += bytes_to_send;
            }
        }
    }
}


void recv_tcp_data(const char* interface_name,
                   remote_info_t* remote_info,
                   uint16_t local_port,
                   char* payload,
                   uint16_t payload_size,
                   uint32_t seq_num,
                   uint32_t ack_num,
                   uint8_t ack,
                   uint8_t rst,
                   uint8_t syn,
                   uint8_t fin)
{
    interface_t* interface = get_interface_by_name(interface_name);
    ip_addr_t local_ip = interface->ip;
    ip_addr_t remote_ip = remote_info->remote_ip;
    uint16_t remote_port = remote_info->remote_port;

    tcp_conn_t conn;
    memcpy(conn.local.ip.ip, local_ip.ip, IP_ADDR_SIZE);
    memcpy(conn.remote.ip.ip, remote_ip.ip, IP_ADDR_SIZE);
    conn.local.port = local_port;
    conn.remote.port = remote_port;

    /* if recv a RST packet, close the connection and cleanup state */
    if (rst == 1) {
        fprintf(stderr, "Error: connection [%u.%u.%u.%u, %u.%u.%u.%u, %u, %u] "
                "reset by peer\n", (conn.local.ip.ip[0] & 0x000000FF),
                (conn.local.ip.ip[1] & 0x000000FF),
                (conn.local.ip.ip[2] & 0x000000FF),
                (conn.local.ip.ip[3] & 0x000000FF),
                (conn.remote.ip.ip[0] & 0x000000FF),
                (conn.remote.ip.ip[1] & 0x000000FF),
                (conn.remote.ip.ip[2] & 0x000000FF),
                (conn.remote.ip.ip[3] & 0x000000FF),
                (conn.local.port & 0x0000FFFF),
                (conn.remote.port & 0x0000FFFF));
        cleanup_conn_state(conn);
        return;
    }

    /* if recv a non-SYN pkt for an unknown connection, send back RST packet */
    if (syn != 1 && conn_table_get(conn) == NULL) {
        fprintf(stderr, "Error: received packet for an unknown connection "
                "[%u.%u.%u.%u, %u.%u.%u.%u, %u, %u]; sending RST\n",
                (conn.local.ip.ip[0] & 0x000000FF),
                (conn.local.ip.ip[1] & 0x000000FF),
                (conn.local.ip.ip[2] & 0x000000FF),
                (conn.local.ip.ip[3] & 0x000000FF),
                (conn.remote.ip.ip[0] & 0x000000FF),
                (conn.remote.ip.ip[1] & 0x000000FF),
                (conn.remote.ip.ip[2] & 0x000000FF),
                (conn.remote.ip.ip[3] & 0x000000FF),
                (conn.local.port & 0x0000FFFF),
                (conn.remote.port & 0x0000FFFF));
        object_id_t object_id;
        int ret = encap_tcp_packet(object_id, NOOP, conn.local.ip, conn.remote.ip,
                    conn.local.port, conn.remote.port, RST_PKT, 0, 0, NULL, 0);
        return;
    }

    if (syn == 1 && ack == 0) { /* received SYN packet */
        half_conn_t local;
        memcpy(local.ip.ip, local_ip.ip, IP_ADDR_SIZE);
        local.port = local_port;

        outstanding_conn_t conn;
        conn.is_valid = true;
        memcpy(conn.remote.ip.ip, remote_ip.ip, IP_ADDR_SIZE);
        conn.remote.port = remote_port;
        conn.seq_num = seq_num;

        outstanding_conns_enqueue(local, conn); //TODO can result in SYN attack

    } else if (syn == 1 && ack == 1) { /* received SYN-ACK packet */
        tcp_conn_state_t* conn_state = conn_table_get(conn);
        assert(conn_state != NULL);

        if (conn_state->curr_state == SYN_SENT) {
            pthread_spin_lock(&conn_state->mtx);
            conn_state->curr_state = SYN_ACK_RECVD;
            conn_state->seq_num = ack_num;
            pthread_spin_unlock(&conn_state->mtx);
            conn_state->ack_num = seq_num + 1;
        }

    } else if (ack == 1) { /* received ACK or Data packet */
        tcp_conn_state_t* conn_state = conn_table_get(conn);
        assert(conn_state != NULL);

        if (conn_state->curr_state == SYN_ACK_SENT) {
            pthread_spin_lock(&conn_state->mtx);
            conn_state->curr_state = ACK_RECVD;
            conn_state->seq_num = ack_num;
            pthread_spin_unlock(&conn_state->mtx);

        } else if (conn_state->curr_state == CONN_ESTABLISHED) {
            if (ack_num > conn_state->tx_buffer->head) {
                uint32_t bytes = ack_num - conn_state->tx_buffer->head;
                uint32_t removed_bytes = char_ring_buffer_remove
                    (conn_state->tx_buffer, NULL, bytes);
                assert(removed_bytes == bytes);
                if (removed_bytes > 0) {
                    pthread_spin_lock(&conn_state->mtx);
                    conn_state->tx_buffer->time_of_head_change = clock();
                    pthread_spin_unlock(&conn_state->mtx);
                }
            }

            if (conn_state->ack_num == seq_num) {
                uint32_t added_bytes = char_ring_buffer_add
                    (conn_state->rx_buffer, payload, payload_size);
                conn_state->ack_num += added_bytes;

                /* send ACK packet */
                object_id_t object_id;
                if (added_bytes > 0) {
                    int ret = encap_tcp_packet(object_id, NOOP, conn.local.ip,
                                conn.remote.ip, conn.local.port, conn.remote.port,
                                ACK_PKT, conn_state->seq_num, conn_state->ack_num,
                                NULL, 0);
                }
            }
        }
    }
}


void cleanup_conn_state(tcp_conn_t conn)
{
    /* cleanup entry from outstanding_conns table */
    outstanding_conns_delete(conn.local);

    /* cleanup entry from conn table */
    conn_table_delete(conn);
}
