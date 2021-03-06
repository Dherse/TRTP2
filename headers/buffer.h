#ifndef BUFFER_H

#define BUFFER_H

#include "global.h"
#include "packet.h"

typedef struct node {
    /** Pointer to the contained value */
    void *value;

    /** Is the value in use? */
    bool used;
} node_t;

typedef struct buf {
    /** Low index in the buffer */
    uint8_t window_low;

    /** Number of elements in the buffer */
    uint8_t length;

    /** Array of nodes contained in the buffer */
    node_t nodes[MAX_BUFFER_SIZE];
} buf_t;

uint8_t buf_get_length(buf_t *self);

/**
 * ## Use
 * 
 * **PSA**: If you need to understand what this function is used for
 * or what inlining is just read further down.
 * 
 * Returns the 5 least significant bits of `seqnum`
 * 
 * ## Explanation
 * 
 * It is interesting as we know the window has a maximum of 32 elements.
 * This means that this function will never collide for elements within
 * the same window as all 5 LSBs will be within the 0-31 range.
 * 
 * It makes it really easy to use a normal 32-elements array instead
 * of a complex FIFO or other data structure.
 * 
 * ## What's all this inline stuff
 * 
 * In C/C++/Rust (and probably others) you can mark a function
 * as recommended for inlining or always inlined. This means that the
 * compiler will try to replace any call to this method with its actual
 * body. The point of this technique is to gain in performance
 * by avoiding unecessary instruction cache misses amongst other things.
 * 
 * 
 * Keep in mind that it can increase the binary size of the final executable.
 * 
 * ## Proof
 * 
 * The main area where a collide could happen is in the interval 240 -> 16.
 * Here are the LSB of each of those 32 values :
 * 
 *  | Decimal |  LSBs  || Decimal |    LSBs  ||  Conflict? |
 *  |     240 |  10000 ||       0 |    00000 ||  FALSE     |
 *  |     241 |  10001 ||       1 |    00001 ||  FALSE     |
 *  |     242 |  10010 ||       2 |    00010 ||  FALSE     |
 *  |     243 |  10011 ||       3 |    00011 ||  FALSE     |
 *  |     244 |  10100 ||       4 |    00100 ||  FALSE     |
 *  |     245 |  10101 ||       5 |    00101 ||  FALSE     |
 *  |     246 |  10110 ||       6 |    00110 ||  FALSE     |
 *  |     247 |  10111 ||       7 |    00111 ||  FALSE     |
 *  |     248 |  11000 ||       8 |    01000 ||  FALSE     |
 *  |     249 |  11001 ||       9 |    01001 ||  FALSE     |
 *  |     250 |  11010 ||      10 |    01010 ||  FALSE     |
 *  |     251 |  11011 ||      11 |    01011 ||  FALSE     |
 *  |     252 |  11100 ||      12 |    01100 ||  FALSE     |
 *  |     253 |  11101 ||      13 |    01101 ||  FALSE     |
 *  |     254 |  11110 ||      14 |    01110 ||  FALSE     |
 *  |     255 |  11111 ||      15 |    01111 ||  FALSE     |
 * 
 * It's easy to extend this test (in Excel for example) to
 * any group of 32 consecutive binary numbers. Since out-of-order
 * numbers (i.e numbers outside of the window) are always rejected
 * there is not possibility of error with this method.
 * 
 * ## ""But seqnum only goes to 31!?""
 * 
 * While its true that seqnum only goes to 31 it's actually a
 * non-issue with this technique as we can simple return
 * as the window of any packet `max(31 - occupied_spaces, 0)`.
 * 
 * And on receive check that the number does not exceed the maximum
 * seqnum allowed by the window. (i.e buf.last_read + 31).
 * 
 * Then again, this method proves to be foolproof and working
 * in all situations including edge-cases.
 * 
 * ## ""But what about receiving packets widely out of order!?""
 * 
 * While it is possible that some weird and convoluted corruption
 * could cause a new packet to have the same 5 LSBs has an in-order
 * packet this is not an issue as the "simple" technique is to use
 * packet.h/unpack on a separate `packet_t buffer` and then
 * swap the buffers with the node once we have validated that the packet
 * belongs to the current window. No gotchas here either!
 * 
 * (foolproof I tell you! I swear)
 * 
 * ## ""I still don't believe you!""
 * 
 * Well, just look at tests/buffer_test.c where we programmed a test
 * testing all of the possible windows (by increments of 16) and you'll
 * see that it works flawlessly.
 * 
 * ## Arguments
 *
 * - `seqnum` - the input sequence number (from a packet)
 *
 * ## Return value
 * 
 * the hash, a uint8_t from 0 to 31.
 * 
 */
uint8_t hash(uint8_t seqnum);

/**
 * ## Use
 * 
 * Allocated a new buffer.
 * 
 * ## Arguments
 *
 * - `buffer`    - a pointer to a buffer
 * - `allocator` - an allocate to be called for each node
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int initialize_buffer(buf_t *buffer, void *(*allocator)());

/**
 * ## Use
 * 
 * Deallocated an existing buffer.
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer.
 *  The buffer does not have to be fully initialized!
 *
 */
void deallocate_buffer(buf_t *buffer);

/**
 * ## Use
 * 
 * Gets the next writable node in the buffer.
 * You should check whether it is available using `is_used()`
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer
 * - `seqnum` - the sequence number to insert
 *
 * ## Return value
 * 
 * NULL if it failed or empty, an initialized node otherwise
 */
node_t *next(buf_t *buffer, uint8_t seqnum);

/**
 * ## Use
 * 
 * Peeks the nth readable element in the buffer. 
 * Starting from 0.
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer
 * - `seqnum` - the sequence number to get
 * - `inc`    - whether to increment last_read or not
 *
 * ## Return value
 * 
 * NULL if it failed or empty, an initialized node otherwise
 */
node_t *get(buf_t *buffer, uint8_t seqnum, bool inc);

/**
 * ## Use
 * 
 * Checks if the seqnum is used
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer
 * - `seqnum` - the sequence number to check
 *
 * ## Return value
 * 
 * - true if the seqnum is in use
 * - false otherwise
 */
bool is_used(buf_t *buffer, uint8_t seqnum);

#endif
