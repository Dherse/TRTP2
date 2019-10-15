#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h> 
#include "../headers/errors.h"
#include "../headers/receiver.h"
#include "../headers/packet.h"

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
    char ip_as_str[40]; //to print an IP if needed

    uint16_t port;
    uint8_t ip[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    while(!rcv_cfg->stop) {
        if(!already_popped) {
            /** get a buffer from the stream */
            node = stream_pop(rcv_cfg->rx, false);
            if(node == NULL) {
                node = (s_node_t *) calloc(1, sizeof(s_node_t));
                if(node == NULL) {
                    fprintf(stderr, "[RX] malloc called failed\n");
                    break;
                }
                
                node->content = calloc(1, sizeof(hd_req_t));
                if(node->content == NULL) {
                    fprintf(stderr, "[RX] calloc called failed");
                    free(node);
                    break;
                }
            }
            node->next = NULL;
            req = (hd_req_t *) node->content;
            if(req == NULL) {
                fprintf(stderr, "[RX] `content` in a node was NULL\n");
                node->content = calloc(1, sizeof(hd_req_t));
                if(node->content == NULL) {
                    free(node);
                    fprintf(stderr, "[RX] calloc called failed\n");
                    continue;
                }

                req = (hd_req_t *) node->content;
            }
            req->stop = false; //just to be sure not to shutdown threads unintentionally
        }
        
        /** receive packet from network */
        req->length = recvfrom(rcv_cfg->sockfd, req->buffer, buf_size, 0, (struct sockaddr *) &sockaddr, rcv_cfg->addr_len);
        if(req->length == -1) {
            // pas de paquet reçus/echec de la reception
            switch(errno) {
                /**
                 * case 1 :
                 * recvfrom would have blocked and waited for a message
                 * but didn't because of the flag
                 * -> loop again
                 */
                case EWOULDBLOCK : 
                    already_popped = true;
                    break;
                /**
                 * case 2 :
                 * a message was 'received', but it failed
                 * print error
                 */
                default :
                    already_popped = true;
                    fprintf(stderr, "[RX] recvfrom failed. (errno = %d)\n", errno);
            }
        } else {
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
                    if(allocate_client(contained, rcv_cfg->idx++, rcv_cfg->file_format) == -1) {
                        fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                        free(contained);
                        break;
                    }
                    *contained->address = sockaddr;
                    *contained->addr_len = addr_len;
                    
                    ht_put(rcv_cfg->clients, port, ip, (void *) contained);

                    fprintf(stderr, "[%s][%u] New client\n", ip_as_str, port);
                } else {
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
int allocate_client(client_t *client, uint32_t id, char *format) {
    client->id = id;

    client->file_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if(client->file_mutex == NULL) {
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    int err = pthread_mutex_init(client->file_mutex,NULL);
    if(err != 0) {
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_INIT_MUTEX;
        return -1;
    }

    char name[256];
    sprintf(name, format, id);
    client->out_file = fopen(name, "wb");
    if (client->out_file == NULL) {
        fprintf(stderr, "[RX] Failed to create file: %s", name);
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    client->address = (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));
    if(client->address == NULL) {
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    client->addr_len = (socklen_t *) malloc(sizeof(socklen_t));
    if(client->addr_len == NULL) {
        free(client->address);
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    client->window = (buf_t *) malloc(sizeof(buf_t));
    if(client->window == NULL){
        free(client->addr_len);
        free(client->address);
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    if(allocate_buffer(client->window, &allocate_packet) != 0) { 
        free(client->addr_len);
        free(client->address);
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        free(client->window);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

int make_listen_socket(const struct sockaddr *src_addr, socklen_t addrlen) {
    int sock = socket(AF_INET6, SOCK_DGRAM,0);
    if(sock < 0){ //errno handeled by socket
        return -1;
    }

    int err = bind(sock, src_addr, addrlen);
    if(err == -1){
        return -1; // errno handeled by bind
    }
    return sock;
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
    memset(req->to_send, 0, 11);
}

/*
 * Refer to headers/receiver.h
 */
void *allocate_handle_request() {
    hd_req_t *req = (hd_req_t *) malloc(sizeof(hd_req_t));
    if(req == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return NULL;
    }
    
    req->stop = false;
    req->client = NULL;
    req->length = 0;
    memset(req->buffer, 0, 528);
}
