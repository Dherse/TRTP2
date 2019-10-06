#include <sys/types.h>          
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h> 
#include "../headers/errors.h"
#include "../headers/receiver.h"
#include "../headers/packet.h"

int write_to_file(rcv_cfg_t *config, packet_t *packet) {

}

void *decode_loop(void *params) {

}

void *handle_loop(void *params) {
    rcv_cfg_t *config = (rcv_cfg_t *) params;

    socklen_t slen = sizeof(config->clt_addr);

    packet_t packet = {
        ACK,
        false,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };

    uint8_t write_buffer[11];

    while(config->running) {
        node_t *next = peek(&config->buf, true, true);
        if (next == NULL) {
            errno = NULL_ARGUMENT;

            return NULL;
        }

        node_t* last = next;
        ptype_t type = NACK;
        int len = 1;

        uint8_t window = ((packet_t *) last->value)->window;
        double oldest_time = ((packet_t *) last->value)->received_time;
        if (!((packet_t *) next->value)->truncated) {
            type = ACK;

            if (write_to_file(config, ((packet_t *) next->value)) != 0) {
                errno = FAILED_TO_OUTPUT;
                fprintf(stderr, "Failed to write packet to file, closing connection");

                config->running = false;
                return NULL;
            }

            int i = 1;
            for (; i < 31; i++) {
                node_t *current = peek(&config->buf, false, true);
                if (last == NULL) {
                    break;
                } else {
                    len++;

                    unlock(last);

                    /* Just moves the read pointer over */
                    last = peek(&config->buf, true, false);

                    if (((packet_t *) last->value)->received_time > oldest_time) {
                        oldest_time = ((packet_t *) last->value)->received_time;
                        window = ((packet_t *) last->value)->window;
                    }

                    if (write_to_file(config,   ((packet_t *) last->value)) != 0) {
                        errno = FAILED_TO_OUTPUT;
                        fprintf(stderr, "Failed to write packet to file, closing connection\n");

                        config->running = false;
                        return NULL;
                    }
                }
            }
        }

        /* 
         * used to make sure that the window of the sender has
         * some space left and waits for it to have some space.
         */
        do {
            int i = 0;
            for (; i < 31; i++) {
                node_t *node = peek_n(&config->buf, i, false, false);
                if (node->used) {
                    if (((packet_t *) node->value)->received_time > oldest_time) {
                        oldest_time = ((packet_t *) node->value)->received_time;
                        window = ((packet_t *) node->value)->window;
                    }
                }
                unlock(node);
            }
        } while (window == 0);

        packet.type = type;
        packet.window = (uint8_t) (config->buf.last_written - config->buf.last_read);
        packet.seqnum = ((packet_t *) last->value)->seqnum;
        packet.timestamp = ((packet_t *) last->value)->timestamp;

        if (pack(write_buffer, &packet, false) != 0) {
            errno = FAILED_TO_PACK;
            fprintf(stderr, "Failed to pack ACK\n");

            config->running = false;
            return NULL;
        }

        int code = sendto(config->socket_fd, &write_buffer, 11, 0, config->clt_addr, slen);
        if (code == -1) {
            errno = FAILED_TO_SEND;
            fprintf(stderr, "Failed to write ACK\n");

            config->running = false;
            return NULL;
        }

        unlock(last);
    }

    return NULL;
}










int deallocate_rcv_config(rcv_cfg_t *cfg) {
    if (cfg != NULL) {
        free(cfg);
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