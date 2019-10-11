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
    const size_t ip_size = sizeof(uint8_t) * 16;

    struct sockaddr_in6 sockaddr;
    socklen_t addr_len = sizeof(sockaddr);
    s_node_t *node;
    hd_req_t *req;

    bool already_popped = 0;
    char ip_as_str[40]; //to print an IP if needed

    while(!rcv_cfg->stop) { 
        if(!already_popped) {
            /** get a buffer from the stream */
            node = stream_pop(rcv_cfg->rx, false);
            if(node == NULL) {
                fprintf(stderr,"[RX] stream_pop returned NULL\n");
                node = (s_node_t *) malloc(sizeof(s_node_t));
                if(node == NULL) {
                    fprintf(stderr, "[RX] malloc called failed");
                    break;
                }
                node->content = calloc(1, sizeof(hd_req_t));
                if(node->content == NULL) {
                    fprintf(stderr, "[RX] calloc called failed");
                    free(node);
                    break;
                }
            }
            req = (hd_req_t *) node->content;
            if(req == NULL) {
                fprintf(stderr, "[RX] `content` in a node was NULL");
                req = (hd_req_t *) calloc(1, sizeof(hd_req_t));
                if(req == NULL) {
                    free(node);
                    fprintf(stderr, "[RX] calloc called failed");
                    break;
                }
            }
            req->stop = false; //just to be sure not to shutdown threads unintentionnally
        }
        
        /** receive packet from network */
        req->length = recvfrom(rcv_cfg->sockfd, req->buffer, buf_size, MSG_DONTWAIT, (struct sockaddr *) &sockaddr, &addr_len);
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
                    fprintf(stderr, "[RX] recvfrom failed. \n");
            }
        } else {
            /** set the last handle_request parameters */
            req->port = (uint16_t) sockaddr.sin6_port;
            move_ip(req->ip, sockaddr.sin6_addr.__in6_u.__u6_addr8);

            /** check if sockaddr is already known in the hash-table */
            if(!ht_contains(rcv_cfg->clients, req->port, req->ip)) {
                ip_to_string(req->ip, ip_as_str);

                /** add new client in `clients` */
                client_t *new_client = (client_t *) malloc(sizeof(client_t));
                if(new_client == NULL){
                    fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                    break;
                } 
                if(allocate_client(new_client) == -1) {
                    fprintf(stderr, "[%s] Client allocation failed\n", ip_as_str);
                    free(new_client);
                    break;
                }
                *new_client->address = sockaddr;
                *new_client->addr_len = addr_len;
                ht_put(rcv_cfg->clients, req->port, req->ip, (void *) new_client);

                fprintf(stderr, "[%s][%u] New client\n", ip_as_str, req->port);
            }

            /** send handle_request */
            stream_enqueue(rcv_cfg->tx, node, true);
            already_popped = false;
        }
    }

    if(already_popped) {
        free(req);
        free(node);
    }
    pthread_exit(0);
    return NULL;
}

/*
 * Refer to headers/receiver.h
 */
void move_ip(uint8_t *destination, uint8_t *source) {
    uint8_t *dst = destination;
    uint8_t *src = source;
    
    for(int i = 0; i < 16; i++) {
        (*dst++) = (*src++);
    }
}

/*
 * Refer to headers/receiver.h
 */
int allocate_client(client_t *client) {

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
    if(allocate_buffer(client->window, sizeof(packet_t)) != 0) { 
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
