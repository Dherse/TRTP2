#include "../headers/packet.h"

typedef struct node {
   int seqnum;
   packet_t *packet;
	
   struct node *next;
} node_t;

/**
 * ## Use :
 *
 * Create an intialized CLL with some length
 * 
 * ## Arguments :
 *
 * - `length` - the length of the CLL
 * - `list` - a pointer where the CLL should be created
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int create_cll(size_t length, node_t *head);

/**
 * ## Use :
 *
 * Allocates an empty CLL
 * 
 * ## Arguments :
 *
 * - `length` - the length of the CLL
 * - `head` - a pointer where the CLL should be created
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int allocate_cll(size_t length, node_t *head);

/**
 * ## Use :
 *
 * Deallocated a CLL and its contents
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int dealloc_cll(node_t *head);

/**
 * ## Use :
 *
 * Resizes the CLL to have `new_length` elements.
 * 
 * ## new_length > old_length
 * 
 * Allocates more space and allocates new packets, no loss of information
 *
 * ## new_length < old_length
 *
 * Will not deallocate actively used packets. The function will return a number
 * between `old_packet` and `new_length`. This difference is the number
 * of packets still waiting to be popped. The operation can be retried after
 * emptying those entries.
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 * - `new_length` - the new length of the CLL
 *
 * ## Return value:
 * 
 * `new_length` if the operation is a success, -1 if it failed
 * and a number between `old_length` and `new_length` if one or more
 * packet_t could not be deleted (already in use)
 */
int resize_cll(node_t *head, size_t new_length);

/**
 * ## Use :
 *
 * Pushes an element onto the CLL.
 *
 * Should NOT be used
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 * - `packet` - the packet to push
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int push_cll(node_t *head, packet_t *packet);

/**
 * ## Use :
 *
 * Points to the next **unused** packet in the CLL.
 * The structure is already allocated and can be used to
 * unpack a network packet.
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 * - `packet` - the packet to push
 *
 * ## Return value:
 * 
 * a non-used packet, NULL otherwise
 *
 */
packet_t *next(node_t *head);

/**
 * ## Use :
 *
 * Returns the first **used** packet in the CLL.
 * Also increments the position by one
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * a used packet, NULL otherwise
 *
 */
packet_t *pop(node_t *head);

/**
 * ## Use :
 *
 * Returns the first **used** packet in the CLL.
 * Contrary to `pop` it does **not** increment the counter.
 * 
 * ## Arguments :
 *
 * - `head` - a pointer to an allocated CLL
 *
 * ## Return value:
 * 
 * a used packet, NULL otherwise
 *
 */
packet_t *peak(node_t *head);