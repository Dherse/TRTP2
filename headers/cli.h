#ifndef CLI_H

#define CLI_H

#include "global.h"
#include "packet.h"

/** Regex format for validating an output file name */
#define REGEX_FORMAT "^[a-zA-Z0-9._\\/-]*\\%[0-9]*d[a-zA-Z0-9._\\/-]*$"

/** An affinity on `cpu` */
typedef struct affinity_setting {
    int cpu;
} afs_t;

typedef struct stream_setting {
    int stream;
} sts_t;

/**
 * Contains a receiver configuration.
 */
typedef struct config_receiver {
    /** Is the application running with a single thread? */
    bool sequential;

    /** Number of streams to use */
    int stream_count;

    /** Number of handler thread, ignored if `sequential` equals true */
    int handle_num;

    /** CPU core affinities for the handler threads */
    afs_t *handle_affinities;

    /** Which stream should each handler use */
    sts_t *handle_streams;

    /** Number of receiver thread, ignored if `sequential` equals true */
    int receive_num;

    /** CPU core affinities for the receiver threads */
    afs_t *receive_affinities;

    /** Which stream should each receiver use */
    sts_t *receive_streams;

    /** Maximum window size to advertise, by default 31 */
    int max_window;

    /** How many packet to read in a single syscall, see recvmmsg */
    int receive_window_size;

    /** Output file name format length */
    size_t format_len;
    
    /** Is the format on the heap or the stack? */
    bool deallocate_format;

    /** Output file name format */
    char *format;

    /** Maximum number of concurrent connections */
    uint32_t max_connections;

    /** The input IP */
    struct addrinfo *addr_info;
    
    /** The input port */
    uint16_t port;
} config_rcv_t;

/**
 * ## Use
 * 
 * Parses the receiver configurations from the console line arguments
 * 
 * ## Arguments
 *
 * - `argc`   - the number of CL arguments
 * - `argv`   - the CL arguments split on spaces
 * - `config` - an allocated but not necessarily initialized config
 *
 * ## Return value
 * 
 * 0 if process completed successfully, -1 otherwise.
 * 
 */
int parse_receiver(int argc, char *argv[], config_rcv_t *config);

/**
 * ## Use
 * 
 * Parses the affinity file `affinity.cfg` for CPU affinities.
 * If it fails it will output on stderr the error. The result
 * of this function should be ignored
 * 
 * ## Arguments
 *
 * - `config` - an allocated but not necessarily initialized config
 *
 * ## Return value
 * 
 * 0 if process completed successfully, -1 otherwise.
 * 
 */
int parse_affinity_file(config_rcv_t *config);

/**
 * ## Use
 * 
 * Parses the streams file `streams.cfg` for stream mapping.
 * If it fails it will output on stderr the error. The result
 * of this function should be ignored.
 * 
 * ## Arguments
 *
 * - `config` - an allocated but not necessarily initialized config
 *
 * ## Return value
 * 
 * 0 if process completed successfully, -1 otherwise.
 * 
 */
int parse_streams_file(config_rcv_t *config);

/**
 * ## Use
 * 
 * Pretty prints the configuration on stderr. Useful for startup
 * and debug.
 * 
 * ## Arguments
 *
 * - `config` - an allocated but not necessarily initialized config
 * 
 */
void print_config(config_rcv_t *config);

/**
 * ## Use :
 *
 * Turns a sockaddr into either an IPv4 addr or IPv6 addr.
 * 
 * ## Arguments
 *
 * - `sa` - a socket_addr
 *
 * ## Return value
 * 
 * a pointer to either an IPv4 addr or IPv6
 * 
 * ## Source
 * 
 * We found it last year (2018-2019) and can't find the source
 * so credits to the original author whoever that may be.
 * Sorry :)
 *
 */
void* get_socket_addr(const struct sockaddr *sa);


/**
 * ## Use :
 *
 * Parses an integer from a string
 * 
 * ## Arguments
 *
 * - `out` - a pointer to an output int
 * - `s` - the string to parse
 * - `base` - the base (e.g base 10)
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 * 
 * ## Source
 * 
 * Obtained from (modified) :
 * https://github.com/cirosantilli/cpp-cheat/blob/c6087065b6b499b949360830aa2edeb4ec2ab276/c/string_to_int.c
 *
 */
int str2int(int *out, char *s, int base);

#endif