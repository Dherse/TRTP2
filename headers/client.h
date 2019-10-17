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
    socklen_t *addr_len;

    /** Current client receive window */
    buf_t *window;

    uint32_t id;
} client_t;

GETSET(client_t, FILE *, out_file);
GETSET(client_t, struct sockaddr_in6 *, address);
GETSET(client_t, socklen_t *, addr_len);
GETSET(client_t, buf_t *, window);
GETSET(client_t, uint32_t, id);

pthread_mutex_t *client_get_lock(client_t *self);

bool is_active(client_t *self);

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
 *
 * ## Return value
 *
 * 0 if the process completed successfully. -1 otherwise.
 * If it failed, errno is set to an appropriate error. 
 */
int initialize_client(client_t *client, uint32_t id, char *format);

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