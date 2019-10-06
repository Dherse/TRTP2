#ifndef CLI_H

#define CLI_H

#include <sys/socket.h>
#include "packet.h"

/**
 * 
 */
int make_sender_socket(const struct sockaddr *dest_addr, socklen_t addrlen);

/**
 * 
 */
int send_packet(int sock, packet_t *msg, size_t msg_len, uint8_t *buf);

/**
 * 
 */
int send_msg(int sock, void *msg, size_t msg_len);

/**
 * 
 */
int receive_ack(int sock, packet_t *packet, uint8_t *buf);
#endif