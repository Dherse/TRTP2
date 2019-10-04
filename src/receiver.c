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
int recv_and_handle_message(const struct sockaddr *src_addr, socklen_t addrlen, packet_t* packet, struct sockaddr * sender_addr, socklen_t* sender_addr_len) {
    // TODO: Create a IPv6 socket supporting datagrams
    int sock = socket(AF_INET6, SOCK_DGRAM,0);
    if(sock < 0){
        return -1;
    }

    // TODO: Bind it to the source
    int err = bind(sock, src_addr, addrlen);
    if(err == -1){
        return -1;
    }

    // TODO: Receive a message through the socket
    uint8_t* packet_received = (uint8_t*) calloc(528, sizeof(uint8_t));//je sais mais c'est temporaire jusqu'à ce que le reste du code soit là =p

    ssize_t n_received = recvfrom(sock, packet_received, sizeof(packet_t), 0, sender_addr, sender_addr_len);
    if(n_received == -1){
        return -1;
    }
    
    if(alloc_packet(packet) == -1 && errno != ALREADY_ALLOCATED){ //checking if packet has been allocated
        return -1;
    }

    if(unpack(packet_received, packet, NULL) != 0){
        return -1;
    }

    return 0;
}