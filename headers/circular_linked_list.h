#ifndef CLL_H

#define CLL_H

#include "packet.h"

#define MAX_WINDOW_VALUE 31

typedef struct node {
    int seqnum;
    bool used;
    packet_t *packet;
    struct node *next;
} node_t;

typedef struct cll{
    //the number of nodes containing a packet
    int used_nodes;

    //pointer to the allocated buffer (kept for freeing purposes)
    node_t * const head; // the pointer is constant, no what is pointed

    //last nodes written or read
    node_t *last_written;
    node_t *last_read;
}cll_t;

//TODO: add semaphore protection on EVERYTHING

/**
 * ## Use :
 *
 * Create an intialized CLL with MAX_WINDOW_VALUE nodes
 * and allocates it's packets
 * 
 * ## Arguments :
 *
 * - `cll` - a pointer where the CLL should be created
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int create_cll(cll_t *cll);

/**
 * ## Use :
 *
 * Allocates an empty CLL
 * 
 * ## Arguments :
 *
 * - `cll` - a NULL pointer where the CLL should be stored
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int allocate_cll(cll_t *cll);

/**
 * ## Use :
 *
 * Deallocates a CLL and its contents
 * 
 * ## Arguments :
 *
 * - `cll` - a pointer to a CLL
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int dealloc_cll(cll_t *cll);

/**
 * ## Use :
 *
 * Returns the pointer to the next **unused** packet in the CLL.
 * The structure is already allocated and can be used to
 * unpack a network packet.
 * 
 * sets used boolean of the node to true
 * 
 * increments used_nodes in case of success
 * 
 * in case of failure the cll is left untouched
 * 
 * ## Arguments :
 *
 * - `cll` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * a non-used packet in case of success, NULL otherwise
 * If it failed, errno is set to an appropriate error.
 */
packet_t *next(cll_t *cll);

//uses semaphore

/**
 * ## Use :
 *
 * Returns the first **used** packet in the CLL.
 * Also increments the position by one
 * 
 * ## Arguments :
 *
 * - `last_read` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * a used packet, NULL otherwise
 *
 */
node_t *pop(cll_t *cll);

/**
 * 
 * release mutex on a node
 * 
 */
int unlock(node_t*);


//incrémente pas last_read
//renvoie->next
//mutex lock le next
/**
 * ## Use :
 *
 * Returns the first **used** packet in the CLL.
 * Contrary to `pop` it does **not** decrement the counter.
 * 
 * ## Arguments :
 *
 * - `last_read` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * a used packet, NULL otherwise
 *
 */
packet_t *peak(cll_t *cll);

#endif