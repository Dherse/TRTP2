/**
 * /!\ IMPLEMENTATION VALIDATED
 * 
 * The implementation has been fully tested and results
 * in complete memory cleanup and no memory leak!
 */
#include "../headers/stream.h"

GETSET_IMPL(s_node_t, void *, content);
GETSET_IMPL(s_node_t, s_node_t *, next);

/**
 * Refer to headers/stream.h
 */
int initialize_node(s_node_t *node, void *(*allocator)()) {
    if (node == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    node->next = QUEUE_POISON1;
    node->content = allocator();
    if (node->content == NULL) {
        return -1;
    }
    
    return 0;
}

/**
 * Refer to headers/stream.h
 */
void deallocate_node(s_node_t *node) {
    if (node == NULL) {
        return;
    }

    if (node->content != NULL) {
        free(node->content);
    }

    free(node);
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
int allocate_stream(stream_t *stream) {
    stream->in_queue = NULL;
    stream->out_queue = NULL;
    stream->length = 0;
    stream->waiting = 0;

    if (pthread_mutex_init(&stream->lock, NULL)) {
        dealloc_stream(stream);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    if (pthread_cond_init(&stream->read_cond, NULL)) {
        dealloc_stream(stream);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
int dealloc_stream(stream_t *stream) {
    if (stream == NULL) {
        return 0;
    }

    bool non_null = false;
    do {
        s_node_t *node = stream_pop(stream, false);
        non_null = node != NULL;

        if (non_null) {
            deallocate_node(node);
        }
    } while(non_null);

    pthread_mutex_destroy(&stream->lock);
    pthread_cond_destroy(&stream->read_cond);
    
    return 0;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
bool stream_enqueue(stream_t *stream, s_node_t *node, bool wait) {
    if (node == NULL) {
        return false;
    }

    while (true) {
        s_node_t *in_queue = stream->in_queue;
        node->next = in_queue;
        if (_cas(&stream->in_queue, in_queue, node)) {
            break;
        }
    }

    _faa(&stream->length, 1);
    if (stream->waiting > 0) {
        pthread_cond_signal(&stream->read_cond);
    }
    
    return true;
}

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/stream.h !
 */
s_node_t *stream_pop(stream_t *stream, bool wait) {
    pthread_mutex_lock(&stream->lock);
    
    _faa(&stream->waiting, 1);

    while (wait && stream->length == 0) {

        pthread_cond_wait(&stream->read_cond, &stream->lock);

    }
    _fas(&stream->waiting, 1);

    if (!stream->out_queue) {
        while (true) {
            s_node_t *head = stream->in_queue;
            if (!head) {
                break;
            }

            if (_cas(&stream->in_queue, head, NULL)) {
                while (head) {
                    s_node_t *next = head->next;
                    head->next = stream->out_queue;
                    stream->out_queue = head;
                    head = next;
                }
                break;
            }
        }
    }

    s_node_t *head = stream->out_queue;
    if (head) {
        _fas(&stream->length, 1);
        stream->out_queue = head->next;
    }
    
    pthread_mutex_unlock(&stream->lock);

    return head;

}