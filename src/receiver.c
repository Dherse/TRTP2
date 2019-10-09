#include <sys/types.h>          
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h> 
#include "../headers/errors.h"
#include "../headers/receiver.h"
#include "../headers/packet.h"

void *receive_thread(void *rx_cfg){
    return NULL;
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

/*
 * Refer to headers/receiver.h
 */
int alloc_reception_buffers(int number, uint8_t** buffers){
    if(number < 1){
        return 0;
    }

    for(int i = 0; i < number; i++){
        *(buffers+i*528) = calloc(528,sizeof(uint8_t));
        if(*(buffers+i*528) == NULL){
            dealloc_reception_buffers(i-1, buffers);
            errno = FAILED_TO_ALLOCATE;
            return -1;
        }
    }

    return 0;
}

/*
 * Refer to headers/receiver.h
 */
void dealloc_reception_buffers(int number, uint8_t** buffers){
    if(number > 1){
        for(int i = 0; i < number; i++){
            free(*(buffers+i*528));
        }
    }
}

/*
 * Refer to headers/receiver.h
 */
int recv_and_handle_message(const struct sockaddr *src_addr, socklen_t addrlen, packet_t* packet, struct sockaddr * sender_addr, socklen_t* sender_addr_len, uint8_t* packet_received) {

     int sock = make_listen_socket(src_addr, addrlen);

    // TODO: Receive a message through the socket
    ssize_t n_received = recvfrom(sock, packet_received, sizeof(packet_t), 0, sender_addr, sender_addr_len);
    if(n_received == -1){
        return -1;
    }
    
    if(alloc_packet(packet) == -1 && errno != ALREADY_ALLOCATED){ //checking if packet has been allocated
        return -1;  // j'ai pas fait de errno parce que la fonction le fait déjà
    }

    if(unpack(packet_received, packet) != 0){
        return -1; // j'ai pas fait de errno parce que la fonction le fait déjà
    }

    return 0;
}