#ifndef STREAM_H

#include "global.h"

#define STREAM_H

typedef struct s_node {
    void *content;

    struct s_node *next;
} s_node_t;

GETSET(s_node_t, void *, content);
GETSET(s_node_t, s_node_t *, next);

/**
 * /!\ READ THIS CAREFULLY IF YOU DON'T UNDERSTAND STREAMS/FIFOs
 * 
 * ## Problem
 * 
 * When communicating between threads, it becomes apparent that
 * the simple producer-consummer architectures are difficult
 * to implement. And in our implementation we need two-way
 * communication between three different kinds of threads.
 * 
 * This means that a single module containing logic capable 
 * of communicating safely accross threads is a must have.
 * 
 * ## Solution
 * 
 * For this reason we decided to use a stream. A stream is
 * in theory an unbounded array, meaning an array with no end
 * where you can always write new values. In practice this is
 * impractical so we're using a double-ended linked list
 * with a maximum capacity. It acts as a FIFO meaning the 
 * first element inserted is also to first to be popped from 
 * the stream.
 * 
 * We also use mutexes and conditions to ensure thread safety
 * and proper waiting instead of busy-waiting.
 * 
 * ## Performance
 * 
 * Unfortunately, this implementation uses quite a few mutex
 * meaning that performance are not going to be great. But
 * knowing the amounts of threads reading and writing from
 * these streams it's important to guarentee strong locking.
 * 
 * ## Implementation details
 * 
 * This implementation is a smi-locking implementation based on the
 * work of user `majek` on GitHub. Link in the sources below
 * 
 * ## For anybody still reading
 * 
 * This is taught in OZ 2 and is a classic piece of
 * multithreaded software. (in Rust they're called
 * channels and they're awesome. They allow fast
 * non-locking inter-thread communication!)
 * 
 * ## Sources
 * 
 * - [Thread safety](https://en.wikipedia.org/wiki/Thread_safety)
 * - [FIFO](https://en.wikipedia.org/wiki/FIFO_(computing_and_electronics))
 * - [Mutex](https://en.wikipedia.org/wiki/Lock_(computer_science))
 * - [Conditions](https://en.wikipedia.org/wiki/Monitor_(synchronization)#Condition_variables)
 * - [Stream](https://en.wikipedia.org/wiki/Stream_(computer_science))
 * - [SemiBlocking Queue](https://github.com/majek/dump/blob/master/msqueue/queue_semiblocking.c)
 * 
 */
typedef struct stream {
    s_node_t *in_queue;
    s_node_t *out_queue;

    pthread_mutex_t lock;

    int length;

    pthread_cond_t read_cond;

    int waiting;


    /*size_t max_length;

    size_t length;

    s_node_t *head;

    s_node_t *tail;

    pthread_mutex_t *head_lock;

    pthread_mutex_t *tail_lock;*/

    /*pthread_cond_t *read_cond;

    pthread_cond_t *write_cond;*/

    /*int value;
    int waiting;
    int released;*/
} stream_t;

#define QUEUE_POISON1 ((void*)0xCAFEBAB5)

#ifndef _cas
#define _cas(ptr, oldval, newval) \
    __sync_bool_compare_and_swap(ptr, oldval, newval)
#endif

#ifndef _faa
#define _faa(ptr, inc) \
    __sync_fetch_and_add(ptr, inc)
#endif

#ifndef _fas
#define _fas(ptr, dec) \
    __sync_fetch_and_sub(ptr, dec)
#endif

/**
 * ## Use
 *
 * Allocates the internal state of a node
 * 
 * ## Arguments
 *
 * - `node`      - a pointer to an already allocated node
 * - `allocator` - an allocator for the content
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int initialize_node(s_node_t *node, void *(*allocator)());

/**
 * ## Use
 *
 * Deallocate a node
 * 
 * ## Arguments
 *
 * - `node`      - a pointer to an already allocated node
 */
void deallocate_node(s_node_t *node);

/**
 * ## Use
 *
 * Allocates the internal state of a stream
 * 
 * ## Arguments
 *
 * - `stream`  - a pointer to an already allocated stream
 * - `max_len` - maximum length of the stream
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int allocate_stream(stream_t *stream, size_t max_len);

/**
 * ## Use
 *
 * Deallocates the internal state of a stream
 * 
 * ## Arguments
 *
 * - `stream` - a pointer to an already allocated stream
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int dealloc_stream(stream_t *stream);

/**
 * ## Use 
 *
 * Enqueues a node in the stream
 * 
 * ## Arguments
 *
 * - `stream` - a pointer to an already allocated stream
 * - `node`   - the node to enqueue
 * - `wait`   - will wait for room to be available if set to true
 *
 * ## Return value
 * 
 * - if wait == true: always returns true
 * - if wait == false: returns true if there was room in the stream
 * and false otherwise
 */
bool stream_enqueue(stream_t *stream, s_node_t *node, bool wait);

/**
 * ## Use
 *
 * pops a node from stream
 * 
 * ## Arguments
 *
 * - `stream` - a pointer to an already allocated stream
 * - `wait`   - will wait for an element to be available if set to true
 *
 * ## Return value
 * 
 * - if wait == true: always returns a value
 * - if wait == false: returns NULL if there was no value in the stream
 * and a pointer otherwise
 */
s_node_t *stream_pop(stream_t *stream, bool wait);

/**
 * ## Use
 *
 * Removes all of the nodes from a stream and deallocate them.
 * Uses a single lock.
 * 
 * ## Arguments
 *
 * - `stream` - a pointer to an already allocated stream
 */
void drain(stream_t *stream);

#endif