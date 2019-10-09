#include "global.h"
#include "hash_table.h"
#include "stream.h"
#include "buffer.h"

#ifndef RX_H

#define RX_H

typedef struct client {
    /** 
     * Protects the file descriptor against
     * parallel writes.
     */
    pthread_mutex_t *file_mutex;

    /** Output file descriptor */
    int out_fd;

    /** Client address */
    struct addrinfo_in6 *address;

    /** Client address length */
    socklen_t *addr_len;

    /** Current client receive window */
    buf_t *window;
} client_t;

typedef struct receive_thread_config {
    /** Thread reference */
    pthread_t *thread;

    /** Receive to Handle stream */
    stream_t *tx;
    /** Handle to Receive stream */
    stream_t *rx;

    /** Hash table of client */
    ht_t *clients;

    /** Socket file descriptor */
    int sockfd;
} rx_cfg;

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
} hd_cfg;

typedef struct send_thread_config {
    /** Thread reference */
    pthread_t *thread;

    /** Handle to Send stream */
    stream_t *send_rx;
    /** Send to Handle stream */
    stream_t *send_tx;

    /** Socket file descriptor */
    int sockfd;
} tx_cfg;

typedef struct handle_request {
    /** true = the loop should stop */
    bool stop;

    /** the client port */
    uint16_t port;

    /** the client IP */
    uint8_t ip[16];

    /** length of the data read from the network */
    int length;

    /** data read from the network */
    uint8_t buffer[528];
} hd_req;

typedef struct send_request {
    /** true = the loop should stop */
    bool stop;

    /** the IP to send to */
    struct addrinfo_in6 *address;

    /** the packet to send */
    packet_t to_send;
} tx_req;

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
 */
void *receive_thread(void *);

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
 */
void *handle_thread(void *);

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
 */
void *send_thread(void *);

typedef struct receive_config {
    pthread_t *handle_thread;

    bool running;

    buf_t buf;

    int out;

    struct sockaddr *clt_addr;

    int socket_fd;
} rcv_cfg_t;

void *recv_loop(void *);

void *handle_loop(void *);


int deallocate_rcv_config(rcv_cfg_t *cfg);



/**
 * ## Use :
 *
 * 
 * 
 * ## Arguments :
 *
 * - `argument` - a pointer to the address you want to listen to
 *
 * ## Return value:
 *
 * the socketfd if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 *
 */
int make_listen_socket(const struct sockaddr *src_addr, socklen_t addrlen);

//TODO1: gérer errno, 
//TODO2: potentiellement passer le socket en argument pour le réutiliser ou autre
//TODO4: rajouter un paramètre payload pour le passer en argument à unpack
//TODO5: rename function =p
/**
 * ## Use :
 *
 * creates a socket, listens and unpacks a message.
 * 
 * ## Arguments :
 *
 * - `src_addr` - a pointer to the address you want to listen to
 * - `addrlen` - the size of the address contained in `src_addr`
 * - `packet` - pointer to a packet that has been allocated
 * - `sender_addr` - pointer to a buffer in which the source of the packet will be stored
 * - `sender_addr_len` - before the call : points to the size of the buffer sender_addr
 *                     - after the call : points to the size of the address stored in sender_addr
 * - `packet_received` - a buffer capable of containing a packet (this is so that we don`t need to reallocate memory)
 * 
 * ## Return value:
 *
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 */
int recv_and_handle_message(const struct sockaddr *src_addr, socklen_t addrlen, packet_t* packet, struct sockaddr * sender_addr, socklen_t* sender_addr_len, uint8_t* packet_received);

/**
 * ## Use :
 *
 * allocates a list containing `number` 528 byte uint8_t* buffers to store incoming packets
 * 
 * ## Arguments :
 *
 * - `number` - the number of buffer you want to allocate
 * - `buffers` - the pointer to the list where the buffers will be stored
 *
 * ## Return value:
 * 
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error.
 */
 int alloc_reception_buffers(int number, uint8_t** buffers);

/**
 * ## Use :
 *
 * deallocates a list of number buffers of 528 bytes
 * 
 * ## Arguments :
 *
 * - `number` - the number of buffer that were allocated
 * - `buffers` - pointer to the list of buffers to dealocate
 *
 * ## Return value:
 * 
 */
void dealloc_reception_buffers(int number, uint8_t** buffers);

#endif