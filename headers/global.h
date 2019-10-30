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
#define true -1
#define false 0
typedef int bool;

/** Minimum size of a packet (in bytes) */
#define MIN_PACKET_SIZE 11

/** Maximum size of a packet (in bytes) */
#define MAX_PACKET_SIZE  528

/** Maximum size of the payload (in bytes) */
#define MAX_PAYLOAD_SIZE 512

/** Maximum size of a receive window */
#define MAX_WINDOW_SIZE  31

/** Maximum size of a receive window as a string */
#define MAX_WINDOW_SIZE_STR "31"

/** Maximum size of a buffer */
#define MAX_BUFFER_SIZE     32

/** Default concurrent capacity as string */
#define DEFAULT_MAX_CAPACITY_STR "100"

/** Default number of receiver thread as string */
#define DEFAULT_RECEIVER_NUM_STR "1"

/** Default number of handler thread as string */
#define DEFAULT_HANDLER_NUM_STR  "2"

/** Default output file name format as string */
#define DEFAULT_OUT_FORMAT       "%d"

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