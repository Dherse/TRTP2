#define _GNU_SOURCE
#include "../headers/receiver.h"
#include "../headers/handler.h"

bool init = false;
pthread_mutex_t receiver_mutex;

/*
 * Refer to headers/receiver.h
 */
inline __attribute__((always_inline)) void rx_run_once(
    rx_cfg_t *rcv_cfg, 
    uint8_t buffers[][MAX_PACKET_SIZE],
    socklen_t addr_len,
    struct sockaddr_in6 *addrs, 
    struct mmsghdr *msgs, 
    struct iovec *iovecs
) {
    int i;
    int window_size = rcv_cfg->window_size;
    s_node_t *node;
    hd_req_t *req;

    struct timespec tmo;
    tmo.tv_sec = 0;
    tmo.tv_nsec = 1000*1000;

    int retval = recvmmsg(rcv_cfg->sockfd, msgs, window_size, MSG_WAITFORONE, &tmo);
    if (retval == -1) {
        switch(errno) {
            case EAGAIN:
                break;
            default :
                fprintf(stderr, "[RX] recvmmsg failed. (errno = %d)\n", errno);
                perror("rcvmmsg");
                break;
        }
    } else if (retval >= 1) {
        client_t *contained = NULL;
        node = NULL;
        for(i = 0; i < retval; i++) {
            if (contained != NULL && ip_equals(
                addrs[i].sin6_addr.__in6_u.__u6_addr8, 
                contained->address->sin6_addr.__in6_u.__u6_addr8) && 
                contained->address->sin6_port == addrs[i].sin6_port
            ) {
            } else {
                if (node != NULL) {
                    stream_enqueue(rcv_cfg->tx, node, true);
                }

                contained = ht_get(rcv_cfg->clients, addrs[i].sin6_port, addrs[i].sin6_addr.__in6_u.__u6_addr8);
                if (!contained) {
                    /** add new client in `clients` */
                    contained = (client_t *) calloc(1, sizeof(client_t));
                    if(contained == NULL) {
                        char ip_as_str[46];
                        ip_to_string(&addrs[i], ip_as_str);
                        fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                        break;
                    }

                    if(initialize_client(
                        contained, 
                        __sync_fetch_and_add(rcv_cfg->idx, 1), 
                        rcv_cfg->file_format, 
                        &addrs[i], 
                        &addr_len
                    )) {
                        char ip_as_str[46];
                        ip_to_string(&addrs[i], ip_as_str);
                        fprintf(stderr, "[%s] Client initialization failed\n", ip_as_str);
                        continue;
                    }
                    
                    ht_put(rcv_cfg->clients, addrs[i].sin6_port, addrs[i].sin6_addr.__in6_u.__u6_addr8, (void *) contained);

                    //pl_add(&poll_list, contained);

                    fprintf(stderr, "[%s][%5u] New client #%d\n", contained->ip_as_string, ntohs(contained->address->sin6_port), contained->id);
                }

                node = stream_pop(rcv_cfg->rx, false);
                if(node == NULL) {
                    node = malloc(sizeof(s_node_t));
                    if (initialize_node(node, allocate_handle_request)) {
                        fprintf(stderr, "[RX] Failed to allocate node(errno: %d)\n", errno);
                        break;
                    }
                }

                req = (hd_req_t *) node->content;
                if(req == NULL) {
                    fprintf(stderr, "[RX] `content` in a node was NULL\n");
                    node->content = (hd_req_t *) allocate_handle_request();
                    req = (hd_req_t *) node->content;
                }

                req->client = contained;
                req->num = 0;
            }

            int idx = req->num++;
            req->lengths[idx] = msgs[i].msg_len;
            memcpy(req->buffer[idx], buffers[i], req->lengths[idx]);
        }

        stream_enqueue(rcv_cfg->tx, node, true);
    }
}

/*
 * Refer to headers/receiver.h
 */
void *receive_thread(void *receive_config) {
    if(receive_config == NULL) {
        pthread_exit(NULL_ARGUMENT);
    }

    if (!init) {
        pthread_mutex_init(&receiver_mutex, NULL);
        init = true;
    }

    rx_cfg_t *rcv_cfg = (rx_cfg_t *) receive_config;
    int window_size = rcv_cfg->window_size;

    if (rcv_cfg->affinity != NULL) {
        pthread_t thread = pthread_self();

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(rcv_cfg->affinity->cpu, &cpuset);

        int aff = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (aff == -1) {
            fprintf(stderr, "[HD] Failed to set affinity\n");
        } else {
            fprintf(stderr, "[HD] Receiver #%d running on CPU #%d\n", rcv_cfg->id, rcv_cfg->affinity->cpu);
        }
    }

    socklen_t addr_len = sizeof(struct sockaddr_in6);

    uint8_t buffers[window_size][MAX_PACKET_SIZE];
    struct sockaddr_in6 addrs[window_size];
    struct mmsghdr msgs[window_size];
    struct iovec iovecs[window_size];

    int i;
    for(i = 0; i < window_size; i++) {
        memset(&iovecs[i], 0, sizeof(struct iovec));
        memset(&msgs[i], 0, sizeof(struct mmsghdr));
        
        iovecs[i].iov_base = buffers[i];
        iovecs[i].iov_len = MAX_PACKET_SIZE;

        msgs[i].msg_hdr.msg_name = &addrs[i];
        msgs[i].msg_hdr.msg_namelen = addr_len;
        msgs[i].msg_hdr.msg_iov = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_flags = 0;
        msgs[i].msg_hdr.msg_control = NULL;
    }
    
    int retval;
    while(!rcv_cfg->stop) {
        rx_run_once(
            rcv_cfg,
            buffers,
            addr_len,
            addrs,
            msgs,
            iovecs
        );
    }
    fprintf(stderr, "[RX] Stopped\n");

    pthread_exit(0);
    return NULL;
}

/*
 * Refer to headers/receiver.h
 */
inline void move_ip(uint8_t *destination, uint8_t *source) {
    memcpy(destination, source, 16);
}
