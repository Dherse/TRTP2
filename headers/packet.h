#ifndef PACKET_H

#define PACKET_H

/**
 * Represents a packet as it is on the network hence the `packed` attribute.
 * 
 * To get a detailed description of the contents refer to the appropriate
 * document: [../statement/pdf](statement.pdf)
 * 
 */
typedef __attribute__((packed, aligned(4))) struct Packet {

} packet_t;

/**
 * Represents an "unpacked-packet" where all of the content
 * has already been processed.
 */
typedef struct UnpackedPacket {

} unpacked_t;

/**
 * Unpacks `packet` to `out.
 * 
 * If it worked, will return 0,
 * -1 otherwise and will set errno accordingly.
 * 
 * errno table :
 *  - 
 *  - 
 *  - 
 *  - 
 */
int unpack(packet_t* packet, unpacked_t *out);

#endif