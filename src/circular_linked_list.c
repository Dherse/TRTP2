
#include <stdlib.h> 
#include "../headers/packet.h"
#include "../headers/circular_linked_list.h"
#include <errno.h>

int create_circular_linked_list(int number, node_t* head){
    if(number < 1){
        return 0;
    }
    if(allocate_circular_linked_list(number, head) == -1){
        //set errno
        return -1;
    }
    node_t* current = head;
    current->seqnum = 0;
    if( alloc_packet(current->packet) == -1){
        dealloc_circular_linked_list(head);
        return -1;
    }

    for(int i = 1; i < number; i++){
        current->next = ++current;
        current->seqnum = i;
        if( alloc_packet(current->packet) == -1){
            dealloc_circular_linked_list(head);
            return -1;
        }
    }
    current->next = head;
    return 0;
}


int allocate_circular_linked_list(int number, node_t* head){
    if(number < 1){
        return 0;
    }

    head = (node_t*) calloc(number, sizeof(node_t));
    if(head == NULL){
        return -1;
    }
}

/**
 * deallocates the list and the packets contained in it
 *
 *
 */
int dealloc_circular_linked_list(node_t* head){
    node_t* current = head;
    while(current != NULL && dealloc_packet(current->packet) != -1){
        current++;
    }
    free(head);
    return 0;
}