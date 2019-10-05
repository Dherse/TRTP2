#include "../headers/packet.h"

typedef struct node {
   int seqnum;
   packet_t *packet;
	
   struct node *next;
} node_t;

/**
 * 
 *
 *
 */
int create_cll(size_t number, node_t *list);

/**
 * 
 *
 *
 */
int allocate_cll(size_t number, node_t *head);

/**
 * deallocates the list and the packets contained in it
 *
 *
 */
int dealloc_cll(node_t *head);

int resize_cll(node_t *head, size_t new_size);

int push_cll(node_t *head, packet_t *packet);

packet_t *pop(node_t *head);

packet_t *peak(node_t *head);