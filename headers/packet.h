#ifndef PACKET_H

#define PACKET_H

#ifndef true
    #define true 1
    #define false 0
#endif
typedef int bool;

#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>   
#include <stdlib.h> 
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <zlib.h>
#include <time.h>
#include "errors.h"
#include "lookup.h"

#ifndef GETSET

#define GETSET(owner, type, var) \
    // Gets ##var from type \
    void set_##var(owner *self, type val);\
    // Sets ##var from type \
    type get_##var(owner *self);

#define GETSET_IMPL(owner, type, var) \
    void set_##var(owner *self, type val) {\
        self->var = val;\
    }\
    type get_##var(owner *self) {\
        return self->var;\
    }

#endif

typedef enum PType {
    IGNORE = 0,
    DATA = 1,
    ACK = 2,
    NACK = 3
} ptype_t;

/**
 * Represents an "unpacked-packet" where all of the content
 * has already been processed.
 */
typedef struct Packet {
    ptype_t type;

    bool truncated;
    
    uint8_t window;

    bool long_length;

    uint16_t length;

    uint8_t seqnum;

    uint32_t timestamp;

    uint32_t crc1;

    uint32_t crc2;

    double received_time;

    uint8_t payload[512];
} packet_t;

GETSET(packet_t, ptype_t, type);

GETSET(packet_t, bool, truncated);

GETSET(packet_t, uint8_t, window);

GETSET(packet_t, uint8_t, seqnum);

GETSET(packet_t, uint32_t, timestamp);

GETSET(packet_t, uint32_t, crc1);

GETSET(packet_t, uint32_t, crc2);

GETSET(packet_t, double, received_time);

/**
 * ## Use
 *
 * Gets the length from a packet
 * 
 * ## Arguments
 *
 * - `self` - a pointer to a packet
 *
 * ## Return value
 * 
 * the length of the payload in the packet
 */
uint16_t get_length(packet_t *self);

/**
 * ## Use
 *
 * Checks if the packet has a long length
 * 
 * ## Arguments
 *
 * - `self` - a pointer to a packet
 *
 * ## Return value
 * 
 * - true if the packet has a long length (L = 1)
 * - false otherwise
 */
bool is_long(packet_t *self);

/**
 * ## Use
 *
 * Sets the length
 * 
 * ## Arguments
 *
 * - `self`   - a pointer to a packet
 * - `length` - the new length
 */
void set_length(packet_t *self, uint16_t length);

/**
 * ## Use
 *
 * Gets the payload
 * 
 * ## Arguments
 *
 * - `self` - a pointer to a packet
 *
 * ## Return value
 * 
 * the payload in the packet
 */
uint8_t *get_payload(packet_t* self);

/**
 * ## Use
 *
 * Sets the payload
 * 
 * ## Arguments
 *
 * - `self`    - a pointer to a packet
 * - `payload` - the new payload
 * - `length`  - the new length
 */
void set_payload(packet_t* self, uint8_t *payload, uint16_t len);

/**
 * ## Use
 *
 * Initializes a packet
 * 
 * ## Arguments
 *
 * - `packet` - a pointer to an already allocated packet
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int init_packet(packet_t* packet);

/**
 * ## Use
 *
 * Deallocated a packet and its payload if any.
 * 
 * ## Arguments
 *
 * - `packet` - a pointer to a packet buffer
 *
 * ## Payload
 *
 * If the payload is not null it will also be freed.
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int dealloc_packet(packet_t* packet);

/**
 * ## Use
 *
 * Unpacks a packet_t from a uint8_t buffer coming from the network.
 * Uses network byte order.
 * 
 * ## Arguments
 *
 * - `packet` - a pointer to a packet buffer
 * - `length` - the length of the packet buffer
 * - `out`    - a pointer to a packet
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int unpack(uint8_t *packet, int length, packet_t *out);

/**
 * ## Use
 *
 * Packs a packet_t into a uint8_t buffer coming to send on the network.
 * Uses network byte order.
 * 
 * ## Arguments
 *
 * - `packet` - a pointer to a packet buffer
 * - `in` - a ponter to a packet
 * - `recompute_crc2` - true to recompute the payload CRC, 
 *      false to use the existing one
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int pack(uint8_t *packet, packet_t *in, bool recompute_crc2);

/**
 * ## Use
 *
 * Pretty-prints the packet contents including the payload.
 * 
 * ## Arguments
 *
 * - `packet` - a pointer to a valid packet buffer
 * - `print_payload` - should print the payload
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int packet_to_string(packet_t *packet, bool print_payload);

/**
 * ## Use
 *
 * Pretty-prints the client IP
 * 
 * ## Arguments
 *
 * - `ip` - the IP to print
 * - `target` - the target string, should be 46 characters long
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int ip_to_string(uint8_t *ip, char *target);

/**
 * ## Use
 *
 * Checks if two IPs are equal
 * 
 * ## Arguments
 *
 * - `ip1` - an IPv6 IP
 * - `ip2` - an IPv6 IP
 *
 * ## Return value
 * 
 * - true if they're equal
 * - false otherwise
 */
bool ip_equals(uint8_t *ip1, uint8_t *ip2);

/**
 * ## Use
 *
 * An allocator that allocates an empty packet with
 * every field set to zero
 * 
 * ## Return value
 * 
 * A pointer to a `packet_t` and NULL if failed.
 */
void *allocate_packet();

#endif