#ifndef HD_H

#include "global.h"
#include "hash_table.h"
#include "stream.h"
#include "buffer.h"
#include "lookup.h"
#include "client.h"

#define HD_H

typedef struct handle_thread_config {
    /** Thread reference */
    pthread_t *thread;

    /** Receive to Handle stream */
    stream_t *rx;
    /** Handle to Receive stream */
    stream_t *tx;

    /** Handle to Send stream */
    stream_t *send_tx;
    /** Send to Handle stream */
    stream_t *send_rx;

    /** Hash table of client */
    ht_t *clients;
} hd_cfg_t;

typedef struct handle_request {
    /** true = the loop should stop */
    bool stop;

    /** the client port */
    client_t *client;

    /** length of the data read from the network */
    ssize_t length;

    /** data read from the network */
    uint8_t buffer[528];
} hd_req_t;

GETSET(hd_req_t, bool, stop);

GETSET(hd_req_t, client_t, client);

GETSET(hd_req_t, ssize_t, length);

uint8_t *get_buffer(hd_req_t* self);

void set_buffer(hd_req_t* self, uint8_t *buffer, uint16_t len);

/**
 * /!\ This is a THREAD definition
 * 
 * # Parameter
 * 
 * Although threads take a `void *` the actual
 * data structure that should be sent to the thread
 * is `hd_cfg`.
 * 
 * ## Use
 * 
 * The handler thread takes a raw packet byte buffer,
 * unpacks it and validates it. If it is a valid packet,
 * and its sequence number fits within the window, it
 * will be added to the current window.
 * 
 * If the packets present in the window are in order,
 * it will loop over every packet, writing their contents
 * to the file and producing a single ACK packet
 * appended onto the `send_tx` stream. The sent packet
 * should be appended on the stream first in order to 
 * shorten packet-processing latency.
 * 
 * ## Failed to validate a packet
 * 
 * In the event a packet fails to validate, the handler
 * does nothing.
 * 
 * ## Truncated packet
 * 
 * In the event of a truncated packet, the packet is ignored
 * and a NACK packet is appended to the `send_tx` stream.
 * 
 * ## Sender window
 * 
 * Using the received packet, the handler will check if the
 * sender has any space in its window. If not, it will leave
 * the window intact and **not** send an (N)ACK packet.
 * 
 * The ACK packets will thus be sent when the next packet is
 * received even though this may be caused by a retransmission
 * timer kicking-in. This is not perfect but there's apparent
 * solution.
 * 
 * ## End of file
 * 
 * Once the EOF has been reached, the client is deleted and freed.
 * The ACK packet will however be sent but with a special flag
 * teeling the sender to deallocate the address.
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
void *handle_thread(void *);

/**
 * ## Use :
 *
 * mallocs a hq_req_t and initializes all it's fields
 * 
 * ## Return value:
 *
 * a valid pointer if the process completed successfully. 
 * NULL otherwise. If it failed, errno is set to an 
 * appropriate error. 
 */
void *allocate_handle_request();

#endif