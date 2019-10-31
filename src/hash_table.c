/**
 * /!\ IMPLEMENTATION VALIDATED
 * 
 * The implementation has been fully tested and results
 * in complete memory cleanup and no memory leak!
 */

#include "../headers/hash_table.h"

/**
 * /!\ REALLY IMPORTANT, REFER TO headers/hash_table.h !
 */
inline uint16_t ht_hash(ht_t *table, uint16_t port) {
    return port % table->size;
}

/*
 * Refer to headers/hash_table.h
 */
int allocate_ht(ht_t *table) {
    table->size = INITIAL_SIZE;
    table->length = 0;

    table->items = calloc(INITIAL_SIZE, sizeof(item_t));
    if (table->items == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    table->lock = calloc(1, sizeof(pthread_mutex_t));
    if (table->lock == NULL) {
        free(table->items);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    if (pthread_mutex_init(table->lock, NULL)) {
        free(table->items);
        free(table->lock);

        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

int dealloc_items(size_t len, item_t *items) {
    int i = 0;
    for (; i < len; i++) {
        if (items[i].value != NULL) {
            deallocate_client(items[i].value, true, true);
        }
    }

    free(items);
}

/*
 * Refer to headers/hash_table.h
 */
int dealloc_ht(ht_t *table) {
    if (table == NULL) {
        errno = ALREADY_DEALLOCATED;
        return -1;
    }
    pthread_mutex_lock(table->lock);

    dealloc_items(table->size, table->items);

    pthread_mutex_unlock(table->lock);
    pthread_mutex_destroy(table->lock);
    free(table->lock);

    table->size = 0;
    table->length = 0;

    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
bool ht_contains(ht_t* table, uint16_t port, uint8_t *ip) {
    return ht_get(table, port, ip) != NULL;
}

/*
 * Refer to headers/hash_table.h
 */
client_t *ht_get(ht_t *table, uint16_t port, uint8_t *ip) {
    pthread_mutex_lock(table->lock);
    uint16_t index = ht_hash(table, port);
    while(table->items[index].used) {
        if (table->items[index].port == port) {
            if (ip_equals(table->items[index].ip, ip)) {
                pthread_mutex_unlock(table->lock);
                return table->items[index].value;
            }
        }

        index = (index + 1) % table->size;
    }

    pthread_mutex_unlock(table->lock);
    return NULL;
}

client_t *ht_put_nolock(ht_t *table, uint16_t port, uint8_t *ip, client_t *item, bool resize) {
    if (table->length > (table->size / 2) && resize) {
        if (ht_resize(table, table->size * 2) != 0) {
            errno = FAILED_TO_RESIZE;
            return NULL;
        }
        return ht_put(table, port, ip, item);
    }

    uint16_t index = ht_hash(table, port);
    client_t *old = NULL;
    while(table->items[index].used) {
        if (table->items[index].port == port) {
            if (ip_equals(table->items[index].ip, ip)) {
                old = table->items[index].value;
                break;
            }
        }

        index = (index + 1) % table->size;
    }

    table->items[index].used = item != NULL;
    table->items[index].port = port;
    table->items[index].value = item;

    memcpy(table->items[index].ip, ip, 16);

    if (old == NULL && table->items[index].used) {
        table->length++;
    } else if (old != NULL && !table->items[index].used) {
        table->length--;
    }

    return old;
}

/*
 * Refer to headers/hash_table.h
 */
client_t *ht_put(ht_t *table, uint16_t port, uint8_t *ip, client_t *item) {
    pthread_mutex_lock(table->lock);

    if (table->length > (table->size / 2)) {
        if (ht_resize(table, table->size * 2) != 0) {
            errno = FAILED_TO_RESIZE;
            return NULL;
        }
    }
    
    client_t *old = ht_put_nolock(table, port, ip, item, false);

    pthread_mutex_unlock(table->lock);

    return old;
}

/*
 * Refer to headers/hash_table.h
 */
client_t *ht_remove(ht_t *table, uint16_t port, uint8_t *ip) {
    client_t *del = ht_put(table, port, ip, NULL);
    if (del == NULL) {
        errno = 0;
        return del;
    }

    if (ht_resize(table, table->size)) {
        return del;
    }

    return del;
}

/*
 * Refer to headers/hash_table.h
 */
int ht_resize(ht_t *table, size_t new_size) {
    item_t *old_values = table->items;
    size_t old_size = table->size;

    table->size = new_size;
    table->length = 0;
    table->items = calloc(new_size, sizeof(item_t));
    if (table->items == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    int i;
    for(i = 0; i < old_size; i++) {
        if (old_values[i].used) {
            ht_put_nolock(table, old_values[i].port, old_values[i].ip, old_values[i].value, false);
            old_values[i].value = NULL;
        }
    }

    dealloc_items(old_size, old_values);
    
    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
size_t ht_length(ht_t *table) {
    return table->length;
}