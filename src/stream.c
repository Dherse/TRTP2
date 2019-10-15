/**
 * /!\ IMPLEMENTATION VALIDATED
 * 
 * The implementation has been fully tested and results
 * in complete memory cleanup and no memory leak!
 */
#include "../headers/stream.h"

/**
 * Refer to headers/stream.h
 */
int allocate_node(s_node_t *node, void *(*allocator)()) {
    if (node == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    node->next = NULL;
    node->content = allocator();
    if (node->content == NULL) {
        return -1;
    }

    return 0;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
int allocate_stream(stream_t *stream, size_t max_len) {
    stream->max_length = max_len;
    stream->length = 0;
    stream->head = NULL;
    stream->tail = NULL;

    stream->lock = malloc(sizeof(pthread_mutex_t));
    if (stream->lock == NULL || pthread_mutex_init(stream->lock, NULL) != 0) {
        dealloc_stream(stream);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    pthread_mutex_lock(stream->lock);

    stream->read_cond = malloc(sizeof(pthread_cond_t));
    if (stream->read_cond == NULL || pthread_cond_init(stream->read_cond, NULL) != 0) {
        pthread_mutex_unlock(stream->lock);
        dealloc_stream(stream);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    stream->write_cond = malloc(sizeof(pthread_cond_t));
    if (stream->write_cond == NULL || pthread_cond_init(stream->write_cond, NULL) != 0) {
        pthread_mutex_unlock(stream->lock);
        dealloc_stream(stream);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    pthread_mutex_unlock(stream->lock);

    return 0;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
int dealloc_stream(stream_t *stream) {
    if (stream == NULL) {
        return 0;
    }

    s_node_t *node = stream->head;
    while (node != NULL) {
        s_node_t *old = node;
        node = node->next;
        if (old->content != NULL) {
            free(old->content);
        }

        free(old);
    }

    if (stream->lock != NULL) {
        pthread_mutex_destroy(stream->lock);
        free(stream->lock);
    }

    if (stream->read_cond != NULL) {
        pthread_cond_destroy(stream->read_cond);
        free(stream->read_cond);
    }

    if (stream->write_cond != NULL) {
        pthread_cond_destroy(stream->write_cond);
        free(stream->write_cond);
    }

    return 0;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
bool stream_enqueue(stream_t *stream, s_node_t *node, bool wait) {
    if (node == NULL) {
        return false;
    }

    pthread_mutex_lock(stream->lock);

    while(stream->length >= stream->max_length) {
        if (!wait) {
            pthread_mutex_unlock(stream->lock);
            return false;
        }

        pthread_cond_wait(stream->write_cond, stream->lock);
    }

    if (stream->head == NULL) {
        stream->head = node;
    }

    if (stream->tail != NULL) {
        stream->tail->next = node;
    }

    stream->tail = node;
    stream->length++;

    pthread_cond_signal(stream->read_cond);

    pthread_mutex_unlock(stream->lock);

    return true;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
s_node_t *stream_pop(stream_t *stream, bool wait) {
    pthread_mutex_lock(stream->lock);

    while (stream->length == 0 || stream->head == NULL) {
        if (!wait) {
            pthread_mutex_unlock(stream->lock);
            return NULL;
        }

        pthread_cond_wait(stream->read_cond, stream->lock);
    }

    s_node_t *head = stream->head;
    s_node_t *next = head->next;
    
    stream->head = next;
    stream->length--;

    if (head == stream->tail) {
        stream->tail = NULL;
    }

    pthread_cond_signal(stream->write_cond);

    pthread_mutex_unlock(stream->lock);

    return head;
}

void drain(stream_t *stream) {
    pthread_mutex_lock(stream->lock);

    while (stream->head != NULL) {
        s_node_t *head = stream->head;
        s_node_t *next = head->next;
        
        stream->head = next;
        stream->length--;

        if (head == stream->tail) {
            stream->tail = NULL;
        }

        free(head->content);
        free(head);
    }


    pthread_cond_broadcast(stream->write_cond);
    pthread_cond_broadcast(stream->read_cond);

    pthread_mutex_unlock(stream->lock);
}