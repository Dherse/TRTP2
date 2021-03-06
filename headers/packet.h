#ifndef PACKET_H

#define PACKET_H

#include "global.h"
#include "errors.h"
#include "lookup.h"

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
    /** Packet type */
    ptype_t type;

    /** Is it truncated ? */
    bool truncated;
    
    /** What is the sender/receiver window size */
    uint8_t window;

    /** Is the length encoded on two bits or one? */
    bool long_length;

    /** The payload length */
    uint16_t length;

    /** The sequence number */
    uint8_t seqnum;

    /** The timestamp */
    uint32_t timestamp;

    /** The CRC for the header */
    uint32_t crc1;

    /** The CRC for the payload */
    uint32_t crc2;

    /** The payload */
    uint8_t payload[MAX_PAYLOAD_SIZE];
} packet_t;

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
 * - `in` - a pointer to a packet
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
 * ## Performance
 * 
 * According to our testing, this function is extremely slow and
 * should only be called in case of errors.
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int ip_to_string(struct sockaddr_in6 *ip, char *target);

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
