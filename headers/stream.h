#include "global.h"

typedef struct snode {
    void *content;

    struct snode *next;
} snode_t;

typedef struct stream {
    size_t max_length;

    size_t length;

    snode_t *head;

    snode_t *tail;

    pthread_mutex_t *lock;

    pthread_cond_t *read_cond;

    pthread_cond_t *write_cond;
} stream_t;

int allocate_stream(stream_t *stream, size_t max_len);

int dealloc_stream(stream_t *stream);

bool stream_enqueue(stream_t *stream, snode_t *snode, bool wait);

snode_t *stream_pop(stream_t *stream, bool wait);