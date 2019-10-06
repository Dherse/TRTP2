#include "../headers/buffer.h"
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
node_t *next(buf_t *buffer) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->write_lock);

    /* Increments the seqnum and gets its index in the buffer */
    uint8_t next_written = buffer->last_written + 1;
    uint8_t next_index = hash(next_written);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
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

    buffer->last_written = next_written;

    /* Unlocks the write on the buffer */
    pthread_mutex_unlock(buffer->write_lock);

    /* We notify any peak waiting that it is available*/
    pthread_cond_signal(node->notifier);

    return node;
}

/*
 * Refer to headers/buffer.h
 */
node_t *peak(buf_t *buffer, bool wait, bool inc) {
    return peak_n(buffer, 0, wait, inc);
}

/*
 * Refer to headers/buffer.h
 */
node_t *peak_n(buf_t *buffer, uint8_t increment, bool wait, bool inc) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->read_lock);

    uint8_t next_read = buffer->last_read + 1 + increment;
    uint8_t next_index = hash(next_read);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        errno = UNKNOWN;
        return NULL;
    }

    pthread_mutex_lock(node->lock);

    if (wait) {
        while(!node->used) {
            pthread_cond_wait(node->notifier, node->lock);
        }
    } else if (!node->used) {
        return NULL;
    }

    node->used = false;

    if (inc) {
        buffer->last_read = next_read;
    }

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
int allocate_buffer(buf_t *buffer) {
    buf_t *temp = (buf_t *) malloc(sizeof(buf_t));
    if(temp == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    *buffer = *temp;

    buffer->last_read = 0;
    buffer->last_written = 0;

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

        buffer->nodes[i].packet = malloc(sizeof(packet_t));
        if(buffer->nodes[i].packet == NULL || alloc_packet(buffer->nodes[i].packet) != 0) {
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
    }

    if (buffer->write_lock != NULL) {
        pthread_mutex_destroy(buffer->write_lock);
    }

    int i = 0;
    for (; i < MAX_WINDOW_SIZE; i++) {
        if (buffer->nodes[i].packet != NULL) {
            dealloc_packet(buffer->nodes[i].packet);
        }

        if (buffer->nodes[i].lock != NULL) {
            pthread_mutex_destroy(buffer->nodes[i].lock);
        }

        if (buffer->nodes[i].notifier != NULL) {
            pthread_cond_destroy(buffer->nodes[i].notifier);
        }
    }
}