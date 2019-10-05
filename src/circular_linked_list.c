#include "../headers/global.h"
#include "../headers/circular_linked_list.h"

int create_circular_linked_list(size_t length, node_t* head) {
    if(length < 1) {
        return 0;
    }

    if(allocate_circular_linked_list(length, head) == -1) {
        //set errno
        return -1;
    }

    node_t* current = head;
    current->seqnum = 0;
    if(alloc_packet(current->packet) == -1) {
        dealloc_circular_linked_list(head);
        return -1;
    }

    for(int i = 1; i < length; i++) {
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


int allocate_circular_linked_list(size_t length, node_t* head){
    if(length < 1) {
        errno = INVALID_LENGTH;
        return 0;
    }

    head = (node_t*) calloc(length, sizeof(node_t));
    if(head == NULL) {
        errno = FAILED_TO_ALLOCATE;
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

    while(current != NULL && dealloc_packet(current->packet) != -1) {
        current++;
    }

    free(head);

    return 0;
}