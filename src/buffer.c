/**
 * /!\ IMPLEMENTATION VALIDATED
 * 
 * The implementation has been fully tested and results
 * in complete memory cleanup and no memory leak!
 */

#include "../headers/buffer.h"
/**
 * REALLY IMPORTANT, REFER TO headers/buffer.h
 */
inline uint8_t hash(uint8_t seqnum) {
    return seqnum & 0x1F;
}

/*
 * Refer to headers/buffer.h
 */
uint8_t buf_get_length(buf_t *self) {
    return self->length;
}

/*
 * Refer to headers/buffer.h
 */
bool is_used(buf_t *buffer, uint8_t seqnum) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return false;
    }
    
    uint8_t next_index = hash(seqnum);

    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        errno = UNKNOWN;
        return false;
    }

    return node->used;
}

/*
 * Refer to headers/buffer.h
 */
node_t *next(buf_t *buffer, uint8_t seqnum) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    uint8_t next_index = hash(seqnum);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        errno = UNKNOWN;
        return NULL;
    }
    /* Locks the node */

    node->used = true;

    buffer->length++;

    return node;
}

/*
 * Refer to headers/buffer.h
 */
node_t *get(buf_t *buffer, uint8_t seqnum, bool inc) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }


    uint8_t next_read = seqnum;
    uint8_t next_index = hash(next_read);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        errno = UNKNOWN;

        return NULL;
    }

    if (!node->used) {
        return NULL;
    }

    node->used = false;

    if (inc) {
        buffer->length--;
        buffer->window_low = next_read + 1;
    }

    return node;
}

/*
 * Refer to headers/buffer.h
 */
int initialize_buffer(buf_t *buffer, void *(*allocator)()) {
    buffer->length = 0;
    buffer->window_low = 0;

    int i;
    for(i = 0; i < MAX_BUFFER_SIZE; i++) {
        buffer->nodes[i].used = false;

        buffer->nodes[i].value = allocator();
        if(buffer->nodes[i].value == NULL) {
            deallocate_buffer(buffer);

            errno = FAILED_TO_ALLOCATE;
            return -1;
        }
    }

    return 0;
}

/*
 * Refer to headers/buffer.h
 */
void deallocate_buffer(buf_t *buffer) {
    if(buffer == NULL) {
        errno = NULL_ARGUMENT;
        return;
    }

    int i = 0;
    for (; i < MAX_BUFFER_SIZE; i++) {

        if (buffer->nodes[i].value != NULL) {
            free(buffer->nodes[i].value);
        }
    }

    free(buffer);
}
