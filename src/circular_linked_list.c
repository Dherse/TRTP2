#include "../headers/global.h"
#include "../headers/circular_linked_list.h"

/*
 * Refer to headers/circular_linked_list.h
 */
int create_cll(cll_t *cll) {

    if(allocate_cll(cll) == -1) {
        return -1;
    }

    node_t* current = cll->head;
    current->seqnum = 0;
    if(alloc_packet(current->packet) == -1) {
        dealloc_cll(cll);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    for(int i = 1; i < length; i++) {
        current->next = ++current;
        current->seqnum = i;
        if(alloc_packet(current->packet) == -1) {
            dealloc_cll(cll);
            errno = FAILED_TO_ALLOCATE;
            return -1;
        }
    }
    current->next = head;
    cll->used_nodes = 0;
    return 0;
}


/*
 * Refer to headers/circular_linked_list.h
 */
int allocate_cll(cll_t *cll){
    cll = (cll_t *) calloc((size_t) 1, sizeof(cll_t));
    if(cll == NULL){
        errno = FAILED_TO_ALLOCATE;
        return -1
    }

    cll->head = (node_t *) calloc(MAX_WINDOW_VALUE, sizeof(node_t));
    if(head == NULL) {
        free(cll);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    cll->last_written = head;
    cll->last_read = head;

    return 0;
}

/*
 * Refer to headers/circular_linked_list.h
 */
int dealloc_cll(cll_t *cll){
    if(cll == NULL){
        errno = ALREADY_DEALLOCATED;
        return 0;
    }

    node_t* current = cll->head;

    while(current != NULL) {
        dealloc_packet(current->packet);
        current++;
    }

    free(cll->head);
    free(cll);
    return 0;
}

/*
 * Refer to headers/circular_linked_list.h
 */
packet_t *next(cll_t *cll){
    if(cll == NULL || cll->last_written == NULL}{
        errno = NULL_ARGUMENT;
        return NULL;
    }

    if(cll->used_nodes == MAX_WINDOW_VALUE){
        errno = CLL_FULL;
        return NULL;
    }

    node_t *current = cll->last_written->next;
    for(int i = 0; i < 30; i++){ // last written ++
        if(current->used == false){
            current->used = true;
            cll->used_nodes += 1;
            cll->last_written = current;
            return current->packet
        }
        current = current->next;
    }

    return NULL;
}

/*
 * Refer to headers/circular_linked_list.h
 */
packet_t *pop(cll_t *cll){
    if(cll == NULL || cll->last_read == NULL}{
        errno = NULL_ARGUMENT;
        return NULL;
    }

    if(cll->used_nodes == 0){
        errno = CLL_EMPTY;
        return NULL;
    }

    node_t *current = cll->last_read->next;
    for(int i = 0; i < 30; i++){ // last_read ++
        if(current->used == true){
            current->used = false;
            cll->used_nodes -= 1;
            cll->last_read = current;
            //TODO : add mutex to current->packet
            return current->packet
        }
        current = current->next;
    }
    return NULL;
}

/*
 * Refer to headers/circular_linked_list.h
 */
int unlock(node_t*){
    return 0;
}

/*
 * Refer to headers/circular_linked_list.h
 */
packet_t *peak(cll_t *cll){
    if(cll == NULL || cll->last_read == NULL}{
        errno = NULL_ARGUMENT;
        return NULL;
    }

    if(cll->used_nodes == 0){
        errno = CLL_FULL;
        return NULL;
    }

    node_t *current = cll->last_read->next;
    for(int i = 0; i < 30; i++){ //
        if(current->used == false){
            current->used = true;
            cll->used_nodes += 1;
            cll->last_written = current;
            return current->packet
        }
        current = current->next;
    }

    return NULL;
}