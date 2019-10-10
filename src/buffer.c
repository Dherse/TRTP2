/**
 * /!\ IMPLEMENTATION VALIDATED
 * 
 * The implementation has been fully tested and results
 * in complete memory cleanup and no memory leak!
 */

#include "../headers/buffer.h"
#include "../headers/packet.h"
#include "../headers/errors.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>

/**
 * REALLY IMPORTANT, REFER TO headers/buffer.h
 */
uint8_t hash(uint8_t seqnum) {
    return seqnum & 0b00011111;
}

/*
 * Refer to headers/buffer.h
 */
node_t *next(buf_t *buffer, uint8_t seqnum) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->write_lock);

    /* Increments the seqnum and gets its index in the buffer */
    uint8_t next_written = seqnum;
    uint8_t next_index = hash(seqnum);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        pthread_mutex_unlock(buffer->write_lock);

        errno = UNKNOWN;
        return NULL;
    }

    /* Locks the node */
    pthread_mutex_lock(node->lock);

    /* 
     * If the node is used wait for a signal indicating it's no longer used.
     * 
     * The reason why there is no deadlock is because pthread_cond_wait unlocks
     * the mutex before going to sleep!
     */
    while (node->used) {
        pthread_cond_wait(node->notifier, node->lock);
    }

    node->used = true;

    buffer->length--;

    /* Unlocks the write on the buffer */
    pthread_mutex_unlock(buffer->write_lock);

    /* We notify any peek waiting that it is available*/
    pthread_cond_signal(node->notifier);

    return node;
}

/*
 * Refer to headers/buffer.h
 */
node_t *peek(buf_t *buffer, bool wait, bool inc) {
    return get(buffer, buffer->window_low, wait, inc);
}

/*
 * Refer to headers/buffer.h
 */
node_t *get(buf_t *buffer, uint8_t seqnum, bool wait, bool inc) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->read_lock);

    uint8_t next_read = seqnum;
    uint8_t next_index = hash(next_read);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        pthread_mutex_unlock(buffer->read_lock);

        errno = UNKNOWN;
        return NULL;
    }

    pthread_mutex_lock(node->lock);

    if (wait) {
        while(!node->used) {
            pthread_cond_wait(node->notifier, node->lock);
        }
    } else if (!node->used) {
        pthread_mutex_unlock(buffer->read_lock);
        pthread_mutex_unlock(node->lock);

        return NULL;
    }

    node->used = false;

    if (inc) {
        buffer->window_low = next_read;
    }

    buffer->length++;

    /* Unlocks the write on the buffer */
    pthread_mutex_unlock(buffer->read_lock);

    /* We notify any next waiting that it is available*/
    pthread_cond_signal(node->notifier);

    return node;
}

/*
 * Refer to headers/buffer.h
 */
void unlock(node_t* node) {
    if (node != NULL) {
        pthread_mutex_unlock(node->lock);
    }
}

/*
 * Refer to headers/buffer.h
 */
int allocate_buffer(buf_t *buffer, size_t size) {
    buf_t *temp = (buf_t *) malloc(sizeof(buf_t));
    if(temp == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    *buffer = *temp;

    free(temp);

    buffer->window_low = 0;

    buffer->read_lock = malloc(sizeof(pthread_mutex_t));
    if(buffer->read_lock == NULL || pthread_mutex_init(buffer->read_lock, NULL) != 0) {
        deallocate_buffer(buffer);

        errno = FAILED_TO_INIT_MUTEX;
        return -1;
    }

    pthread_mutex_lock(buffer->read_lock);

    buffer->write_lock = malloc(sizeof(pthread_mutex_t));
    if(buffer->write_lock == NULL || pthread_mutex_init(buffer->write_lock, NULL) != 0) {
        pthread_mutex_unlock(buffer->read_lock);

        deallocate_buffer(buffer);

        errno = FAILED_TO_INIT_MUTEX;
        return -1;
    }

    pthread_mutex_lock(buffer->write_lock);

    for(int i = 0; i < MAX_WINDOW_SIZE; i++) {
        buffer->nodes[i].used = false;

        buffer->nodes[i].lock = malloc(sizeof(pthread_mutex_t));
        if(buffer->nodes[i].lock == NULL || pthread_mutex_init(buffer->nodes[i].lock, NULL) != 0) {
            pthread_mutex_unlock(buffer->read_lock);
            pthread_mutex_unlock(buffer->write_lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_INIT_MUTEX;
            return -1;
        }

        pthread_mutex_lock(buffer->nodes[i].lock);

        buffer->nodes[i].notifier = malloc(sizeof(pthread_cond_t));
        if(buffer->nodes[i].notifier == NULL || pthread_cond_init(buffer->nodes[i].notifier, NULL) != 0) {
            pthread_mutex_unlock(buffer->nodes[i].lock);
            pthread_mutex_unlock(buffer->read_lock);
            pthread_mutex_unlock(buffer->write_lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_INIT_COND;
            return -1;
        }

        buffer->nodes[i].value = malloc(size);
        if(buffer->nodes[i].value == NULL) {
            pthread_mutex_unlock(buffer->nodes[i].lock);
            pthread_mutex_unlock(buffer->read_lock);
            pthread_mutex_unlock(buffer->write_lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_ALLOCATE;
            return -1;
        }

        pthread_mutex_unlock(buffer->nodes[i].lock);
    }

    pthread_mutex_unlock(buffer->read_lock);
    pthread_mutex_unlock(buffer->write_lock);

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

    if (buffer->read_lock != NULL) {
        pthread_mutex_destroy(buffer->read_lock);
        free(buffer->read_lock);
    }

    if (buffer->write_lock != NULL) {
        pthread_mutex_destroy(buffer->write_lock);
        free(buffer->write_lock);
    }

    int i = 0;
    for (; i < MAX_WINDOW_SIZE; i++) {
        if (buffer->nodes[i].value != NULL) {
            free(buffer->nodes[i].value);
        }

        if (buffer->nodes[i].lock != NULL) {
            pthread_mutex_destroy(buffer->nodes[i].lock);
            free(buffer->nodes[i].lock);
        }

        if (buffer->nodes[i].notifier != NULL) {
            pthread_cond_destroy(buffer->nodes[i].notifier);
            free(buffer->nodes[i].notifier);
        }
    }
}