#include "../headers/global.h"
#include "../headers/circular_linked_list.h"

/*
 * Refer to headers/circular_linked_list.h
 */
int create_cll(size_t length, node_t *head) {
    if(length < 1) {
        errno = INVALID_LENGTH;
        return 0;
    }

    if(allocate_cll(length, head) == -1) {
        // no need to set errno has it's already set by allocate_cll
        return -1;
    }

    node_t* current = head;
    current->seqnum = 0;
    if(alloc_packet(current->packet) == -1) {
        dealloc_cll(head);
        return -1;
    }

    for(int i = 1; i < length; i++) {
        current->next = ++current;
        current->seqnum = i;
        if(alloc_packet(current->packet) == -1){
            dealloc_cll(head);
            return -1;
        }
    }
    current->next = head;
    return 0;
}


/*
 * Refer to headers/circular_linked_list.h
 */
int allocate_cll(size_t length, node_t *head){
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

/*
 * Refer to headers/circular_linked_list.h
 */
int dealloc_cll(node_t *head){
    node_t* current = head;

    while(current != NULL && dealloc_packet(current->packet) != -1) {
        current++;
    }

    free(head);

    return 0;
}

int resize_cll(node_t *head, size_t new_size){
    return 0;
}

int push_cll(node_t *head, packet_t *packet){
    return 0;
}

packet_t *pop(node_t *head){
    return NULL;
}

packet_t *peak(node_t *head){
    return NULL;
}