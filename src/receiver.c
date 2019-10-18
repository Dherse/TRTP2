
#include "../headers/receiver.h"
#include "../headers/handler.h"

/*
 * Refer to headers/receiver.h
 */
void *receive_thread(void *receive_config) {
    if(receive_config == NULL) {
        pthread_exit(NULL_ARGUMENT);
    }
    rx_cfg_t *rcv_cfg = (rx_cfg_t *) receive_config;

    const size_t buf_size = sizeof(uint8_t) * 528;

    struct sockaddr_in6 sockaddr;
    socklen_t addr_len = sizeof(sockaddr);
    s_node_t *node;
    hd_req_t *req;

    bool already_popped = false;
    char ip_as_str[46]; //to print an IP if needed

    uint16_t port;
    uint8_t ip[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    while(!rcv_cfg->stop) {
        //TODO make it shorter
        if(!already_popped) {
            node = pop_and_check_req(rcv_cfg->rx, allocate_handle_request);
            if(node == NULL) {
                break;
            }
            req = (hd_req_t *) node->content;
        }

        req->length = recvfrom(rcv_cfg->sockfd, req->buffer, buf_size, 0, (struct sockaddr *) &sockaddr, &rcv_cfg->addr_len);
        if(req->length == -1) {
            switch(errno) {
                case EAGAIN:
                    already_popped = true;
                    break;

                default :
                    already_popped = true;
                    fprintf(stderr, "[RX] recvfrom failed. (errno = %d)\n", errno);
            }
        } else if (req->length >= 11) {
            /** set the last handle_request parameters */
            port = (uint16_t) sockaddr.sin6_port;
            move_ip(ip, sockaddr.sin6_addr.__in6_u.__u6_addr8);

            /** check if sockaddr is already known in the hash-table */
            client_t *contained = ht_get(rcv_cfg->clients, port, ip);
            
            if (contained == NULL && rcv_cfg->clients->length >= rcv_cfg->max_clients) {
                /** the client is new **but** the maximum number of clients is already reached */
                ip_to_string(ip, ip_as_str);

                fprintf(stderr, "[%s] New client but max sized reached, ignored\n", ip_as_str);

                already_popped = true;
            } else {
                if(contained == NULL) {
                    ip_to_string(ip, ip_as_str);

                    /** add new client in `clients` */
                    contained = (client_t *) calloc(1, sizeof(client_t));
                    if(contained == NULL){
                        fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                        break;
                    } 
                    if(initialize_client(contained, rcv_cfg->idx++, rcv_cfg->file_format) == -1) {
                        fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                        free(contained);
                        break;
                    }
                    *contained->address = sockaddr;
                    
                    ht_put(rcv_cfg->clients, port, ip, (void *) contained);

                    fprintf(stderr, "[%s][%5u] New client\n", ip_as_str, port);
                }

                req->client = contained;

                /** send handle_request */
                stream_enqueue(rcv_cfg->tx, node, true);
                already_popped = false;
            }
        }
    }

    if(already_popped) {
        free(req);
        free(node);
    }

    fprintf(stderr, "[RX] Stopped\n");

    pthread_exit(0);
    return NULL;
}

/*
 * Refer to headers/receiver.h
 */
void move_ip(uint8_t *destination, uint8_t *source) {
    destination[1] = source[1];
    destination[2] = source[2];
    destination[3] = source[3];
    destination[4] = source[4];
    destination[5] = source[5];
    destination[6] = source[6];
    destination[7] = source[7];
    destination[8] = source[8];
    destination[9] = source[9];
    destination[10] = source[10];
    destination[11] = source[11];
    destination[12] = source[12];
    destination[13] = source[13];
    destination[14] = source[14];
    destination[15] = source[15];
}

/*
 * Refer to headers/receiver.h
 */
void *allocate_send_request() {
    tx_req_t *req = (tx_req_t *) malloc(sizeof(tx_req_t));
    if(req == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return NULL;
    }

    req->stop = false;
    req->deallocate_address = false;
    req->address = NULL;
    memset(req->to_send, 0, 32);
    return req;
}
