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

    uint8_t *payload;

    uint32_t crc2;
} packet_t;


/**
 * ## Use :
 *
 * Allocates a packet, payload as NULL pointer
 * 
 * ## Arguments :
 *
 * - `packet` - a pointer to a non-initialized pointer
 *
 * ## Payload :
 *
 * Payload will be a zero pointer (NULL)
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
 * - `out` - a pointer to a packet
 * - `payload` - NULL or pointer to a 512 bytes long uint8_t buffer
 *
 * ## Payload :
 *
 * If the `payload` argument is non null, the payload pointer of the packet
 * will be replaced with this new pointer. If the packet already had a
 * non-null payload, it will be freed. (careful for double free)
 * 
 * If the `payload` is null, the current packet payload will be reused.
 *
 * If the current payload of the packet is also null, a new 512 bytes
 * long buffer will be allocated.
 *
 * This behaviour although a bit convoluted is designed to make it easy
 * to allocated once and reuse the same buffers and struct in order to
 * make a near allocation free code.
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int unpack(uint8_t *packet, packet_t *out, uint8_t *payload);

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
int packet_to_string(const packet_t *packet);

#endif