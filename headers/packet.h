#ifndef PACKET_H

#define PACKET_H

#ifndef true
    #define true 1
    #define false 0
#endif
typedef int bool;

#include <stdint.h>

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


/**
 * ## Use :
 *
 * Allocates a packet
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to an already allocated packet
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int alloc_packet(packet_t* packet);

/**
 * ## Use :
 *
 * Deallocated a packet and its payload if any.
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to a packet buffer
 *
 * ## Payload :
 *
 * If the payload is not null it will also be freed.
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int dealloc_packet(packet_t* packet);


/**
 * ## Use :
 *
 * Unpacks a packet_t from a uint8_t buffer coming from the network.
 * Uses network byte order.
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to a packet buffer
 * - `length` - the length of the packet buffer
 * - `out`    - a pointer to a packet
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int unpack(uint8_t *packet, int length, packet_t *out);

/**
 * ## Use :
 *
 * Packs a packet_t into a uint8_t buffer coming to send on the network.
 * Uses network byte order.
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to a packet buffer
 * - `in` - a ponter to a packet
 * - `recompute_crc2` - true to recompute the payload CRC, 
 *      false to use the existing one
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int pack(uint8_t *packet, packet_t *in, bool recompute_crc2);

/**
 * ## Use :
 *
 * Pretty-prints the packet contents including the payload.
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to a valid packet buffer
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int packet_to_string(const packet_t *packet, bool print_payload);

void ip_to_string(uint8_t *ip, char *target);

bool ip_equals(uint8_t *ip1, uint8_t *ip2);

#endif