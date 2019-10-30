#include "../headers/client.h"

GETSET_IMPL(client_t, FILE *, out_file);

GETSET_IMPL(client_t, struct sockaddr_in6 *, address);

GETSET_IMPL(client_t, socklen_t, addr_len);

GETSET_IMPL(client_t, uint32_t, id);

/*
 * Refer to headers/client.h
 */
pthread_mutex_t *client_get_lock(client_t *self) {
    return self->lock;
}

/*
 * Refer to headers/client.h
 */
buf_t *get_client_window(client_t *self) {
    return self->window;
}

/*
 * Refer to headers/client.h
 */
void set_client_window(client_t *self, buf_t *window) {
    self->window = window;
}

/*
 * Refer to headers/client.h
 */
bool is_active(client_t *self) {
    return self->active;
}

/*
 * Refer to headers/client.h
 */
void set_active(client_t *self, bool active) {
    self->active = active;
}

/*
 * Refer to headers/client.h
 */
int initialize_client(
    client_t *client, 
    uint32_t id, 
    char *format, 
    struct sockaddr_in6 *address,
    socklen_t *addr_len
) {
    client->last_timestamp = 0;
    client->id = id;
    client->active = true;
    
    client->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    if(client->lock == NULL) {
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    int err = pthread_mutex_init(client->lock, NULL);
    if(err != 0) {
        free(client->lock);
        free(client);
        errno = FAILED_TO_INIT_MUTEX;
        return -1;
    }

    char name[256];
    sprintf(name, format, id);
    client->out_file = fopen(name, "wb");
    if (client->out_file == NULL) {
        LOG("RX", "Failed to create file: %s", name);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    client->address = (struct sockaddr_in6 *) malloc(sizeof(struct sockaddr_in6));
    if(client->address == NULL) {
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    *client->address = *address;

    client->addr_len = *addr_len;

    client->window = (buf_t *) malloc(sizeof(buf_t));
    if(client->window == NULL){
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    if(initialize_buffer(client->window, &allocate_packet) != 0) { 
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client->window);
        free(client);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    if(ip_to_string(address, client->ip_as_string)) { 
        deallocate_buffer(client->window);
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &client->connection_time);
    client->transferred = 0;

    return 0;
}

/*
 * Refer to headers/client.h
 */
void deallocate_client(client_t *client, bool dealloc_addr, bool close_file) {
    if (client == NULL) {
        return;
    }

    if (client->lock != NULL) {
        pthread_mutex_destroy(client->lock);
        free(client->lock);
    }

    if (dealloc_addr && client->address != NULL) {
        free(client->address);
    }

    if (close_file && client->active && client->out_file != NULL) {
        fclose(client->out_file);
    }

    if (client->window != NULL) {
        deallocate_buffer(client->window);
    }

    free(client);
}