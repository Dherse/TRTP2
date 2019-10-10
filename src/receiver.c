#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h> 
#include "../headers/errors.h"
#include "../headers/receiver.h"
#include "../headers/packet.h"

/** 
 *          struct sockaddr_in6 {
 *              sa_family_t     sin6_family;   // AF_INET6
 *              in_port_t       sin6_port;     // port number
 *              uint32_t        sin6_flowinfo; // IPv6 flow information
 *              struct in6_addr sin6_addr;     // IPv6 address
 *              uint32_t        sin6_scope_id; // Scope ID (new in 2.4)
 *          };
 */

/*
 * Refer to headers/receiver.h
 */
void *receive_thread(void *receive_config){
    if( receive_config == NULL ){
        pthread_exit(NULL_ARGUMENT);
    }
    rx_cfg_t *rcv_cfg = (rx_cfg_t *)receive_config;

    const size_t buf_size = sizeof(uint8_t) * 528;
    const size_t ip_size = sizeof(uint8_t) * 16;

    struct sockaddr_in6 sockaddr;
    socklen_t addr_len = sizeof(sockaddr);

    while(true) { // TODO : condition à changer, peut être mettre flag dans rcv_cfg ?
        /** get a buffer from the stream */
        // TODO : check if node == NULL or req == NULL
        s_node_t *node = stream_pop(rcv_cfg->rx,false);
        hd_req_t *req = (hd_req_t *) node->content;
        req->stop = false; //just to be sure not to shutdown threads
        
        /** receive packet from network */
        req->length = recvfrom(rcv_cfg->sockfd, req->buffer, buf_size, 0, (struct sockaddr *) &sockaddr, &addr_len);
        if(req->length == -1){
            // pas de paquet reçus/echec de la reception
            // TODO : QUE FAIRE DES STRUCTS ?
        }
        /** set handle_request parameters */
        req->port = (uint16_t) sockaddr.sin6_port;
        memcpy(req->ip, &sockaddr.sin6_addr, ip_size); //TODO : try and change it to something better ?

        /** check if sockaddr is already known in the hash-table */
        if(!ht_contains(rcv_cfg->clients, req->port, req->ip)){
            /** add new client in `clients` */
            client_t *new_client; 
            if(allocate_client(new_client) == -1){
                //TODO : client not allocated
            }
            *new_client->address = sockaddr;
            *new_client->addr_len = addr_len;
            ht_put(rcv_cfg->clients, req->port, req->ip, (void *)new_client);
        }

        /** send handle_request */
        if(!stream_enqueue(rcv_cfg->tx, node, true)){
            // TODO : what if not enqueued
        }
    }
    //TODO : shutdown routine

    pthread_exit(0);
}

/*
 * Refer to headers/receiver.h
 */
int allocate_client(client_t *client) {
    client = (client_t *) malloc(sizeof(client_t));
    if(client == NULL){
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

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

    if(allocate_buffer(client->window, sizeof(packet_t)) != 0) { 
        free(client->addr_len);
        free(client->address);
        pthread_mutex_destroy(client->file_mutex);
        free(client->file_mutex);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

int make_listen_socket(const struct sockaddr *src_addr, socklen_t addrlen){
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
