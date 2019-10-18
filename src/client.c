#include "../headers/client.h"

GETSET_IMPL(client_t, FILE *, out_file);

GETSET_IMPL(client_t, struct sockaddr_in6 *, address);

GETSET_IMPL(client_t, socklen_t *, addr_len);

GETSET_IMPL(client_t, uint32_t, id);


pthread_mutex_t *client_get_lock(client_t *self) {
    return self->lock;
}

buf_t *get_client_window(client_t *self) {
    return self->window;
}

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
int initialize_client(client_t *client, uint32_t id, char *format) {
    client->id = id;

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
        fprintf(stderr, "[RX] Failed to create file: %s", name);
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

    client->addr_len = (socklen_t *) malloc(sizeof(socklen_t));
    if(client->addr_len == NULL) {
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    client->window = (buf_t *) malloc(sizeof(buf_t));
    if(client->window == NULL){
        free(client->addr_len);
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }
    
    if(initialize_buffer(client->window, &allocate_packet) != 0) { 
        free(client->addr_len);
        free(client->address);
        pthread_mutex_destroy(client->lock);
        free(client->lock);
        free(client);
        free(client->window);
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

/*
 * Refer to headers/client.h
 */
void deallocate_client(client_t *client, bool dealloc_addr, bool close_file) {
    if (client == NULL) {
        return;
    }

    pthread_mutex_lock(client->lock);

    client->active = false;

    if (dealloc_addr) {
        free(client->address);
        free(client->addr_len);
    }

    if (close_file) {
        fclose(client->out_file);
    }
                                        
    deallocate_buffer(client->window);

    pthread_mutex_unlock(client->lock);
    pthread_mutex_destroy(client->lock);
    free(client->lock);

    free(client);
}