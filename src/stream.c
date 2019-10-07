#include "../headers/stream.h"

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

int dealloc_stream(stream_t *stream) {
    snode_t *node = stream->head;
    while (node != NULL) {
        snode_t *old = node;
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

bool stream_enqueue(stream_t *stream, snode_t *snode, bool wait) {
    pthread_mutex_lock(stream->lock);

    while(stream->length >= stream->max_length) {
        if (!wait) {
            return false;
        }

        pthread_cond_wait(stream->write_cond, stream->lock);
    }

    if (stream->head == NULL) {
        stream->head = snode;
    }

    snode->next = stream->tail;
    stream->tail = snode;
    stream->length++;

    pthread_cond_signal(stream->read_cond);

    pthread_mutex_unlock(stream->lock);

    return true;
}

snode_t *stream_pop(stream_t *stream, bool wait) {
    pthread_mutex_lock(stream->lock);

    while (stream->length == 0 || stream->head == NULL) {
        if (!wait) {
            return NULL;
        }

        pthread_cond_wait(stream->read_cond, stream->lock);
    }

    snode_t *head = stream->head;
    snode_t *next = head->next;
    
    stream->head = next;

    if (next == NULL) {
        stream->tail = NULL;
    }

    pthread_mutex_unlock(stream->lock);

    return head;
}