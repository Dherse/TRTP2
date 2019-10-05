#ifndef BUFFER_H

#define BUFFER_H

#define MAX_WINDOW_SIZE 31

#include "packet.h"
#include <pthread.h>
#include <stdlib.h>

typedef struct node{
    packet_t *packet;
    bool used;
    pthread_mutex_t mut;
}node_t;

typedef struct buf{
    uint8_t last_read;
    uint8_t last_written;
    node_t nodes[MAX_WINDOW_SIZE];
}buf_t;


/**
 * ## Use :
 * 
 * ## Arguments :
 *
 * - `buf` - 
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int create_buffer(buf_t *buf);

/**
 * ## Use :
 * 
 * ## Arguments :
 *
 * - `buf` - 
 *
 */
void deallocate_buffer(buf_t *buf);

/**
 * ## Use :
 * 
 * returs the 5 least significant bits of `seqnum`
 * 
 * this value is always between 0 and 31 and never returns two times the same number if given. this allows us to simply
 * store the packet at the index returned by this function without 
 * parsing said buffer
 * 
 * ## Arguments :
 *
 * - `seqnum` - 
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
inline uint8_t hash(uint8_t seqnum) __attribute__((always_inline)); //"hash" we know it's not really one but here we go

#endif