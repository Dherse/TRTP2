#ifndef CLI_H

#define CLI_H

#include "errors.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define REGEX_FORMAT "^[a-zA-Z0-9._-]*\\%[0-9]*d[a-zA-Z0-9._-]*$"
/**
 * Contains a receiver configuration.
 * 
 */
typedef struct config_receiver {
    int handle_num;

    size_t format_len;
    char *format;

    uint8_t max_connections;

    struct addrinfo *addr_info;
    uint16_t port;    
} config_rcv_t;

int parse_receiver(int argc, char *argv[], config_rcv_t *config);

#endif