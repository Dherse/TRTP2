#ifndef GLOBAL_H

#define GLOBAL_H

#define LOG(component, fmt, arg...) \
    do { fprintf(stderr, "[" component "] " fmt, ##arg); } while(0)

#ifdef DEBUG
    #define TRACE(fmt, arg...) \
        LOG("DEBUG][%s:%d:%s()", fmt, __FILE__, __LINE__, __func__, ##arg)
#else
    #define TRACE(...)
#endif

/** close and sleep */
#include <unistd.h>

/** isspace */
#include <ctype.h>

/** Standard library */
#include <stdlib.h>

/** For the numeric types such as uint8_t, etc. */
#include <stdint.h>

/** Required for IO operations */
#include <stdio.h>

/** Integer limits (for cli.h/str2int()) */
#include <limits.h>

/** String operations */
#include <string.h>

/** CRC32 */
#include "../lib/Crc32.h"

/** Required for recvmmsg & sendmmsg */
#include <sys/socket.h>

/** Required for IP to String conversion */
#include <arpa/inet.h>

/** Required for struct addrinfo */
#include <netdb.h>

/** Required for output file validation */
#include <regex.h>

/** Required for measuring time (average speed and autoremove) */
#include <time.h>

/** Signal for SIGINT in main.c */
#include <signal.h>

/** Required for multithreading */
#include <pthread.h>

#include <sys/cdefs.h>

#include <sys/param.h>

#include <getopt.h>

/** Custom error number definitions */
#include "errors.h"

/** Useful generated lookup tables (LUT) */
#include "lookup.h"

/**
 * Defines boolean semantic for ease of use
 */
#define true 1
#define false 0
typedef int bool;

/** Converts from an DEFINE to a string, used for CLI */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/** Minimum size of a packet (in bytes) */
#define MIN_PACKET_SIZE  11

/** Maximum size of a packet (in bytes) */
#define MAX_PACKET_SIZE  528

/** Maximum size of the payload (in bytes) */
#define MAX_PAYLOAD_SIZE 512

/** Maximum size of a receive window */
#define MAX_WINDOW_SIZE  31

/** Maximum size of a buffer */
#define MAX_BUFFER_SIZE  32

#define DEFAULT_MAX_CAPACITY 100

/** Default output file name format as string */
#define DEFAULT_OUT_FORMAT "%d"

/** Default number of handlers */
#define DEFAULT_HANDLER_COUNT  2

/** Default number of receivers */
#define DEFAULT_RECEIVER_COUNT 1

#ifndef GETSET

#define GETSET(owner, type, var) \
    // Gets ##var from type \
    void set_##var(owner *self, type val);\
    // Sets ##var from type \
    type get_##var(owner *self);

#define GETSET_IMPL(owner, type, var) \
    inline void set_##var(owner *self, type val) {\
        self->var = val;\
    }\
    inline type get_##var(owner *self) {\
        return self->var;\
    }

#endif

#endif