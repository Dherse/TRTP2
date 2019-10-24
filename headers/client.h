#ifndef CLIENT_H

#include "global.h"
#include "buffer.h"

#define CLIENT_H

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


typedef struct client {
    /**
     * Is the client actively exchanging data?
     */
    bool active;

    /** 
     * Protects the file descriptor against
     * parallel writes.
     */
    pthread_mutex_t *lock;

    /** Output file descriptor */
    FILE *out_file;

    /** Client address */
    struct sockaddr_in6 *address;

    /** Client address length */
    socklen_t addr_len;

    /** Current client receive window */
    buf_t *window;

    /** The client ID (number) */
    uint32_t id;

    /** The client's IP as a string */
    char ip_as_string[INET6_ADDRSTRLEN];

    /** The time the client connected */
    struct timespec connection_time;

    /** The time the client was set as inactive */
    struct timespec end_time;

    /** Total bytes transferred */
    uint64_t transferred;
} client_t;

GETSET(client_t, FILE *, out_file);
GETSET(client_t, struct sockaddr_in6 *, address);
GETSET(client_t, socklen_t, addr_len);
GETSET(client_t, buf_t *, window);
GETSET(client_t, uint32_t, id);

/**
 * ## Use
 *
 * Gets the client's lock
 * 
 * ## Arguments
 *
 * - `client` - a pointer to an already initialized client
 *
 * ## Return value
 *
 * an initialized mutex
 */
pthread_mutex_t *client_get_lock(client_t *self);

/**
 * ## Use
 *
 * Is the client active?
 * 
 * ## Arguments
 *
 * - `client` - a pointer to an already initialized client
 *
 * ## Return value
 *
 * true if active, false otherwise
 */
bool is_active(client_t *self);

/**
 * ## Use
 *
 * Sets the `active` field in the `client`
 * 
 * ## Arguments
 *
 * - `client` - a pointer to an already initialized client
 * - `active` - whether the client is active or not
 */
void set_active(client_t *self, bool active);

/**
 * ## Use
 *
 * Initializes the client with empty/NULL values
 * 
 * ## Arguments
 *
 * - `client` - a pointer to an already allocated client
 * - `id`     - the ID for the file name format
 * - `format` - the file name format
 * - `address` - the client's address
 * - `add_len` - address length
 *
 * ## Return value
 *
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 */
int initialize_client(
    client_t *client, 
    uint32_t id, 
    char *format, 
    struct sockaddr_in6 *address,
    socklen_t *addr_len
);

/**
 * ## Use
 *
 * Frees a client
 * 
 * ## Arguments
 *
 * - `client`       - a pointer to the address you want to listen to
 * - `dealloc_addr` - should dealloc the addr?
 * - `close_file`   - should close the file?
 */
void deallocate_client(client_t *client, bool dealloc_addr, bool close_file);

#endif