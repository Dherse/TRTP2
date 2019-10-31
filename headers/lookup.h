#ifndef SEQ_H

#define SEQ_H

#include "global.h"

#include <stdint.h>

/**
 * A window lookup table used to check if a seqnum
 * is in a particular window. The way to check if
 * it is in the window is by doing :
 * 
 * ```c
 * sequences[window_low][seqnum]
 * ```
 * 
 * Where `window_low` is the lowest seqnum present
 * in the window and `seqnum` is the sequence number
 * to check.
 * 
 * The value is `1` if it is in the window and `0`
 * otherwise.
 * 
 * It may seem a bit barbaric but it makes looking
 * sequence numbers for ordering very easy to do
 * and very easy to understand. In addition, it just
 * consists of 65536 bytes of memory so not that much.
 * 
 * Generated using a bit of C code
 */
extern const uint8_t sequences[256][256];

/**
 * An extended ASCII lookup table used to escape
 * special and control characters when printing the
 * content of the packets.
 * 
 * Generated using Excel
 */
extern const char *ascii[256];

#endif