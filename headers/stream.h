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
 * the simple producer-consummer architectures is difficult
 * to implement. And in our implementation we need two-way
 * communication between two different kinds of threads.
 * 
 * This means that a single module containing logic capable 
 * of communicating safely accross threads is a must have.
 * 
 * ## Solution
 * 
 * For this reason we decided to use a stream. A stream is
 * in theory an unbounded array, meaning an array with no end
 * where you can always write new values. In practice this is
 * impractical so we're using a double-ended linked list.
 * It acts as a FIFO meaning the first element inserted is also 
 * the first to be popped from the stream.
 * 
 * We also use mutexes and conditions to ensure thread safety
 * and proper waiting instead of busy-waiting.
 * 
 * ## Performance
 * 
 * This implementation is semi-locking meaning that it only uses 
 * a mutex for popping but not for enqueuing. Meaning that enqueue
 * is extremely fast with a O(1) complexity while O(n) for dequeue/pop.
 * Since the queues are almost never full this is not actually an issue.
 * 
 * ## Atomic operations
 * 
 * In order to create a semi-locking queues, some integer operations are
 * still needed and must be executed in a thread safe manner. For this
 * reason this implementation uses atomic operations. To learn more
 * there's a link in the sources below.
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
 * - [Atomic operations](https://en.wikipedia.org/wiki/Linearizability)
 * 
 */
typedef struct stream {
    /** In queue (head) */
    s_node_t *in_queue;

    /** Out queue (tail) */
    s_node_t *out_queue;

    /** Read lock */
    pthread_mutex_t lock;

    /** Number of elements */
    int length;

    /** Read condition */
    pthread_cond_t read_cond;
    
    /** Number of threads waiting for data */
    int waiting;
} stream_t;

#define QUEUE_POISON1 ((void*)0xCAFEBAB5)

/**
 * Atomic operation: 
 * COMPARE -> SWAP
 */
#ifndef _cas
#define _cas(ptr, oldval, newval) \
    __sync_bool_compare_and_swap(ptr, oldval, newval)
#endif

/**
 * Atomic operation: 
 * FETCH -> ADD
 */
#ifndef _faa
#define _faa(ptr, inc) \
    __sync_fetch_and_add(ptr, inc)
#endif

/**
 * Atomic operation: 
 * FETCH -> SUB
 */
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
 *
 * ## Return value
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
int initialize_stream(stream_t *stream);

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
 * Enqueue a node in the stream
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

#endif