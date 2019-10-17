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
inline uint8_t hash(uint8_t seqnum) {
    return seqnum & 0b00011111;
}

/*
 * Refer to headers/buffer.h
 */
GETSET_IMPL(node_t, void *, value);

/*
 * Refer to headers/buffer.h
 */
GETSET_IMPL(node_t, bool, used);

/*
 * Refer to headers/buffer.h
 */
pthread_mutex_t *node_get_lock(node_t *self) {
    return self->lock;
}

/*
 * Refer to headers/buffer.h
 */
GETSET_IMPL(node_t, pthread_cond_t *, notifier);

/*
 * Refer to headers/buffer.h
 */
GETSET_IMPL(buf_t, uint8_t, window_low);

/*
 * Refer to headers/buffer.h
 */
uint8_t buf_get_length(buf_t *self) {
    return self->length;
}

/*
 * Refer to headers/buffer.h
 */
inline pthread_mutex_t *buf_get_lock(buf_t *self) {
    return self->lock;
}

/*
 * Refer to headers/buffer.h
 */
bool is_used_nolock(buf_t *buffer, uint8_t seqnum) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return false;
    }
    
    uint8_t next_written = seqnum;
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
bool is_used(buf_t *buffer, uint8_t seqnum) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return false;
    }

    pthread_mutex_lock(buffer->lock);
    
    bool used = is_used_nolock(buffer, seqnum);

    pthread_mutex_unlock(buffer->lock);

    return used;
}

/*
 * Refer to headers/buffer.h
 */
node_t *next_nolock(buf_t *buffer, uint8_t seqnum, bool wait) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    uint8_t next_written = seqnum;
    uint8_t next_index = hash(seqnum);
    
    node_t *node = &buffer->nodes[next_index];
    if (node == NULL) {
        pthread_mutex_unlock(buffer->lock);

        errno = UNKNOWN;
        return NULL;
    }

    /* 
     * If the node is used wait for a signal indicating it's no longer used.
     * 
     * The reason why there is no deadlock is because pthread_cond_wait unlocks
     * the mutex before going to sleep!
     */
    if (wait) {
        pthread_mutex_lock(node->lock);

        while (node->used) {
            pthread_cond_wait(node->notifier, node->lock);
        }
    } else if(node->used) {
        return NULL;
    } else {
        pthread_mutex_lock(node->lock);
    }

    /* Locks the node */

    node->used = true;

    buffer->length++;

    /* We notify any peek waiting that it is available*/
    pthread_cond_broadcast(node->notifier);

    return node;
}

/*
 * Refer to headers/buffer.h
 */
node_t *next(buf_t *buffer, uint8_t seqnum, bool wait) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->lock);

    node_t *node = next_nolock(buffer, seqnum, wait);

    /* Unlocks the write on the buffer */
    pthread_mutex_unlock(buffer->lock);

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
node_t *get_nolock(buf_t *buffer, uint8_t seqnum, bool wait, bool inc) {
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

    if (wait) {
        pthread_mutex_lock(node->lock);
        while(!node->used) {
            pthread_cond_wait(node->notifier, node->lock);
        }
    } else if (!wait && !node->used) {
        return NULL;
    } else {
        pthread_mutex_lock(node->lock);
    }

    node->used = false;

    if (inc) {
        buffer->length--;
        buffer->window_low = next_read + 1;
    }

    /* We notify any next waiting that it is available*/
    pthread_cond_signal(node->notifier);

    return node;
}

/*
 * Refer to headers/buffer.h
 */
node_t *get(buf_t *buffer, uint8_t seqnum, bool wait, bool inc) {
    if (buffer == NULL) {
        errno = NULL_ARGUMENT;
        return NULL;
    }

    pthread_mutex_lock(buffer->lock);

    node_t *node = get_nolock(buffer, seqnum, wait, inc);

    /* Unlocks the write on the buffer */
    pthread_mutex_unlock(buffer->lock);

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
int initialize_buffer(buf_t *buffer, void *(*allocator)()) {
    buffer->length = 0;
    buffer->window_low = 0;

    buffer->lock = malloc(sizeof(pthread_mutex_t));
    if(buffer->lock == NULL || pthread_mutex_init(buffer->lock, NULL) != 0) {
        deallocate_buffer(buffer);

        errno = FAILED_TO_INIT_MUTEX;
        return -1;
    }

    pthread_mutex_lock(buffer->lock);

    int i;
    for(i = 0; i < MAX_WINDOW_SIZE; i++) {
        buffer->nodes[i].used = false;

        buffer->nodes[i].lock = malloc(sizeof(pthread_mutex_t));
        if(buffer->nodes[i].lock == NULL || pthread_mutex_init(buffer->nodes[i].lock, NULL) != 0) {
            pthread_mutex_unlock(buffer->lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_INIT_MUTEX;
            return -1;
        }

        pthread_mutex_lock(buffer->nodes[i].lock);

        buffer->nodes[i].notifier = malloc(sizeof(pthread_cond_t));
        if(buffer->nodes[i].notifier == NULL || pthread_cond_init(buffer->nodes[i].notifier, NULL) != 0) {
            pthread_mutex_unlock(buffer->nodes[i].lock);
            pthread_mutex_unlock(buffer->lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_INIT_COND;
            return -1;
        }

        buffer->nodes[i].value = allocator();
        if(buffer->nodes[i].value == NULL) {
            pthread_mutex_unlock(buffer->nodes[i].lock);
            pthread_mutex_unlock(buffer->lock);

            deallocate_buffer(buffer);

            errno = FAILED_TO_ALLOCATE;
            return -1;
        }

        pthread_mutex_unlock(buffer->nodes[i].lock);
    }

    pthread_mutex_unlock(buffer->lock);

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

    pthread_mutex_lock(buffer->lock);

    int i = 0;
    for (; i < MAX_WINDOW_SIZE; i++) {
        if (buffer->nodes[i].notifier != NULL) {
            pthread_cond_broadcast(buffer->nodes[i].notifier);
            pthread_cond_destroy(buffer->nodes[i].notifier);
            free(buffer->nodes[i].notifier);
        }

        if (buffer->nodes[i].value != NULL) {
            free(buffer->nodes[i].value);
        }

        if (buffer->nodes[i].lock != NULL) {
            pthread_mutex_destroy(buffer->nodes[i].lock);
            free(buffer->nodes[i].lock);
        }
    }

    pthread_mutex_unlock(buffer->lock);
    pthread_mutex_destroy(buffer->lock);
    free(buffer->lock);

    free(buffer);
}