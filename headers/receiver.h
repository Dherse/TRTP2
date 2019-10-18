#ifndef RX_H

#include "global.h"
#include "hash_table.h"
#include "stream.h"
#include "buffer.h"
#include "lookup.h"
#include "client.h"

#define RX_H

#ifndef GETSET

#define GETSET(owner, type, var) \
    // Gets ##var from type \
    void set_##var(owner *self, type val);\
    // Sets ##var from type \
    type get_##var(owner *self);

#define GETSET_IMPL(owner, type, var) \
    inline void set_##var(owner *self, type val) {\
        self->var = val;\
    }\
    inline type get_##var(owner *self) {\
        return self->var;\
    }

#endif

typedef struct receive_thread_config {
    int i;

    /** Thread reference */
    pthread_t *thread;

    /** the number of clients that have connected */
    uint32_t idx;

    /** the file name format */
    char *file_format;

    /** true = the loop should stop */
    bool stop;

    /** Receive to Handle stream */
    stream_t *tx;

    /** Handle to Receive stream */
    stream_t *rx;

    /** Hash table of client */
    ht_t *clients;

    /** Socket file descriptor */
    int sockfd;

    socklen_t *addr_len;
    
    /** Maximum number of clients */
    int max_clients;
} rx_cfg_t;

typedef struct send_thread_config {
    /** Thread reference */
    pthread_t *thread;

    /** Handle to Send stream */
    stream_t *send_rx;
    /** Send to Handle stream */
    stream_t *send_tx;

    /** Socket file descriptor */
    int sockfd;
} tx_cfg_t;

typedef struct send_request {
    /** true = the loop should stop */
    bool stop;

    /** Ignores the window if set to true */
    bool deallocate_address;

    /** the IP to send to */
    struct sockaddr_in6 *address;

    /** the packet to send */
    uint8_t to_send[32];
} tx_req_t;

GETSET(tx_req_t, bool, stop);

GETSET(tx_req_t, bool, deallocate_address);

GETSET(tx_req_t, struct sockaddr_in6 *, address);

/**
 * /!\ This is a THREAD definition
 * 
 * ## Parameter
 * 
 * Although threads take a `void *` the actual
 * data structure that should be sent to the thread
 * is `rx_cfg`.
 * 
 * ## Use
 * 
 * The purpose of this thread is to receive messages
 * from the socket in a single threaded manner and
 * pass them onto the handle thread pool.
 * 
 * It does **not** manipulate the packet in any way,
 * it just writes it to a buffer and appends it
 * to the stream of pending packets.
 * 
 * In shorter terms, it acts as a multiplexer.
 * 
 * ## New connections
 * 
 * In the event of a new client connecting, the receive
 * thread will create a new client data structure
 * and append it to the client hash-table. This action
 * is done here to avoid needing extra synchronization
 * on the hashtable in the event of a packet burst from
 * a new client.
 * 
 * However, if the maximum number of client has been reached,
 * the new client will be ignored. Hopefully after the
 * retransmission timer has been reached a slot will be
 * available.
 * 
 * ## Appending to a stream
 * 
 * For each stream, two sub-streams are used. One is used to
 * send the information onto the next stage. The other is
 * used to return the previously used data structures
 * in order to avoid needing on-the-go allocation.
 * 
 * In the event that no data structure can be received without
 * waiting, a new one is allocated. This should guarantee that
 * all allocation happen close to the startup of the application.
 * 
 * Once a node has been popped from a stream it has to be
 * enqueued onto the return stream. If it cannot be it should be
 * deallocated to avoid memory leaks.
 * 
 */
void *receive_thread(void *);

/**
 * /!\ This is a THREAD definition
 * 
 * ## Parameter
 * 
 * Although threads take a `void *` the actual
 * data structure that should be sent to the thread
 * is `tx_cfg`.
 * 
 * ## Use
 * 
 * Reads from a stream of packets, packs them
 * and writes them to the socket. Finally it appends the
 * spent packet structure to the return stream.
 * 
 * ## Appending to a stream
 * 
 * For each stream, two sub-streams are used. One is used to
 * send the information onto the next stage. The other is
 * used to return the previously used data structures
 * in order to avoid needing on-the-go allocation.
 * 
 * In the event that no data structure can be received without
 * waiting, a new one is allocated. This should guarantee that
 * all allocation happen close to the startup of the application.
 * 
 * Once a node has been popped from a stream it has to be
 * enqueued onto the return stream. If it cannot be it should be
 * deallocated to avoid memory leaks.
 * 
 */
void *send_thread(void *);

/**
 * ## Use :
 * 
 * copies the ip address stored in src (4 * 32 bits)
 * to dst (a 16 * 8 bits buffer)
 * 
 * ## Arguments :
 *
 * - `dst` - a pointer to the address you want to write to
 * - `src` - a pointer to the address you want to copy from
 *
 * ## Return value:
 *
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 */
void move_ip(uint8_t *dst, uint8_t *src);

/**
 * ## Use :
 *
 * mallocs a tx_req_t and initializes all it's fields
 * 
 * ## Return value:
 *
 * a valid pointer if the process completed successfully. 
 * NULL otherwise. If it failed, errno is set to an 
 * appropriate error. 
 */
void *allocate_send_request();

#endif