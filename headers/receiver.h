#include "../headers/global.h"

typedef struct receive_common {
    /** signifies the threads have to stop */
    bool stop;

    buf_t *buffer;

    uint8_t sender_window;

    pthread_mutex_t *window_lock;

    struct sockaddr *clt_addr;
} rcv_common_t;

typedef struct receive_config {
    rcv_common_t *common;
} rcv_cfg_t;

void *socket_receive_loop(void *);

typedef struct writeback_config {
    rcv_common_t *common;
} wb_cfg_t;

void socket_writeback_loop(void *);

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