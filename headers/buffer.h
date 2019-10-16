#ifndef BUFFER_H

#define BUFFER_H

#include "../headers/packet.h"
#include "../headers/errors.h"

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_WINDOW_SIZE 32

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

typedef struct node {
    void *value;

    bool used;

    pthread_mutex_t *lock;

    pthread_cond_t *notifier;
} node_t;

GETSET(node_t, void *, value);

GETSET(node_t, bool, used);

pthread_mutex_t *node_get_lock(node_t *self);

GETSET(node_t, pthread_cond_t *, notifier);

typedef struct buf {
    uint8_t window_low;

    uint8_t length;

    pthread_mutex_t *lock;

    node_t nodes[MAX_WINDOW_SIZE];
} buf_t;

GETSET(buf_t, uint8_t, window_low);

uint8_t buf_get_length(buf_t *self);

pthread_mutex_t *buf_get_lock(buf_t *self);

/**
 * ## Use
 * 
 * **PSA**: If you need to understand what this function is used for
 * or what inlining is just read further down.
 * 
 * Returns the 5 least significant bits of `seqnum`
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
 * by avoiding unecessary instruction cache misses.
 * 
 * Mind you, it can increase the binary size of the final executable.
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
 * as the window of any packet `min(31 - occupied_spaces, 0)`.
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
 * Well, just look at test/buffer_test.c where we programmed a test
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
 * - `buffer`  - a pointer to a buffer
 * - `element` - the size of each element given by sizeof
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
 * Waits for one to become available.
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer
 * - `wait` - whether to wait for a readable writable to be available
 * - `seqnum` - the sequence number to insert
 *
 * ## Return value
 * 
 * - wait == true : NULL if it failed, an initialized node otherwise
 * - wait == false : NULL if it failed or empty, an initialized node otherwise
 */
node_t *next(buf_t *buffer, uint8_t seqnum, bool wait);

/**
 * Refer to buffer.h/next(buf_t *, uint8_t, bool)
 * 
 * Does the same thing but without locking, considers the
 * lock has already been grabbed before
 * 
 * ## No lock
 * 
 * This function considers that the lock has already been acquired!
 */
node_t *next_nolock(buf_t *buffer, uint8_t seqnum, bool wait);

/**
 * ## Use
 * 
 * Peeks the next readable element in the buffer
 * 
 * ## Arguments
 *
 * - `buffer` - a pointer to an already-allocated buffer
 * - `wait` - whether to wait for a readable value to be available
 * - `inc` - whether to increment last_read or not
 *
 * ## Return value
 * 
 * - wait == true : NULL if it failed, an initialized node otherwise
 * - wait == false : NULL if it failed or empty, an initialized node otherwise
 */
node_t *peek(buf_t *buffer, bool wait, bool inc);

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
 * - `wait`   - whether to wait for a readable value to be available
 * - `inc`    - whether to increment last_read or not
 *
 * ## Return value
 * 
 * - wait == true : NULL if it failed, an initialized node otherwise
 * - wait == false : NULL if it failed or empty, an initialized node otherwise
 */
node_t *get(buf_t *buffer, uint8_t seqnum, bool wait, bool inc);

/**
 * Refer to buffer.h/get(buf_t *, uint8_t, bool, bool)
 * 
 * Does the same thing but without locking, considers the
 * lock has already been grabbed before
 * 
 * ## No lock
 * 
 * This function considers that the lock has already been acquired!
 */
node_t *get_nolock(buf_t *buffer, uint8_t seqnum, bool wait, bool inc);

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

/**
 * Refer to buffer.h/is_used(buf_t *, uint8_t)
 * 
 * Does the same thing but without locking, considers the
 * lock has already been grabbed before
 * 
 * ## No lock
 * 
 * This function considers that the lock has already been acquired!
 */
bool is_used_nolock(buf_t *buffer, uint8_t seqnum);

/**
 * ## Use
 * 
 * Unlocks the node for future read/write
 * 
 * ## Arguments
 *
 * - `node` - a node owned by a buffer
 */
void unlock(node_t* node);

#endif