#ifndef CLI_H

#define CLI_H

#include "errors.h"
#include "global.h"
#include "packet.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define REGEX_FORMAT "^[a-zA-Z0-9._\\/-]*\\%[0-9]*d[a-zA-Z0-9._\\/-]*$"

typedef struct affinity_setting {
    int cpu;
} afs_t;

/**
 * Contains a receiver configuration.
 * 
 */
typedef struct config_receiver {
    /** Is the application running with a single thread? */
    bool sequential;

    /** Number of handler thread, ignored if `sequential` equals true */
    int handle_num;

    /** CPU core affinities for the handler threads */
    afs_t *handle_affinities;

    /** Number of receiver thread, ignored if `sequential` equals true */
    int receive_num;

    /** CPU core affinities for the receiver threads */
    afs_t *receive_affinities;

    /** Maximum window size to advertise, by default 31 */
    int max_window;

    /** How many packet to read in a single syscall, see recvmmsg */
    int receive_window_size;

    /** Output file name format length */
    size_t format_len;
    /** Output file name format */
    char *format;

    /** Maximum number of concurrent connections */
    uint8_t max_connections;

    /** The input IP */
    struct addrinfo *addr_info;
    /** The input port */
    uint16_t port;    
} config_rcv_t;

int parse_receiver(int argc, char *argv[], config_rcv_t *config);

int parse_affinity_file(config_rcv_t *config);

void print_config(config_rcv_t *config);

#endif