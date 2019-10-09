#include "global.h"

#ifndef STREAM_H

#define STREAM_H

typedef struct s_node {
    void *content;

    struct s_node *next;
} s_node_t;

typedef struct stream {
    size_t max_length;

    size_t length;

    s_node_t *head;

    s_node_t *tail;

    pthread_mutex_t *lock;

    pthread_cond_t *read_cond;

    pthread_cond_t *write_cond;
} stream_t;

int allocate_stream(stream_t *stream, size_t max_len);

int dealloc_stream(stream_t *stream);

bool stream_enqueue(stream_t *stream, s_node_t *node, bool wait);

s_node_t *stream_pop(stream_t *stream, bool wait);

#endif