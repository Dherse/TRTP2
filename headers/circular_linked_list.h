#include "../headers/packet.h"

typedef struct node {

   int seqnum;
   packet_t* packet;
	
   struct node *next;
} node_t;

/**
 * 
 *
 *
 */
int create_circular_linked_list(int number, node_t* list);

/**
 * 
 *
 *
 */
int allocate_circular_linked_list(int number, node_t* head);

/**
 * deallocates the list and the packets contained in it
 *
 *
 */
int dealloc_circular_linked_list(node_t* head);