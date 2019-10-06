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
    ht_t *temp = calloc(1, sizeof(ht_t));
    if (temp == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    *table = *temp;

    free(temp);

    table->items = calloc(INITIAL_SIZE, sizeof(item_t));
    if (table->items == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    table->size = INITIAL_SIZE;

    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
int dealloc_ht(ht_t *table) {
    if (table == NULL) {
        errno = ALREADY_DEALLOCATED;
        return -1;
    }

    int i = 0;
    for (; i < table->size; i++) {
        if (table->items[i].used && table->items[i].value != NULL) {
            deallocate_rcv_config(table->items[i].value);
        }
    }

    free(table->items);

    table->size = 0;
    table->length = 0;

    free(table);

    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
bool ht_contains(ht_t* table, uint16_t port) {
    uint16_t index = ht_hash(table, port);
    while(table->items[index].used) {
        if (table->items[index].port == port) {
            return true;
        }

        index = ht_hash(table, index + 1);
    }

    return false;
}

/*
 * Refer to headers/hash_table.h
 */
rcv_cfg_t *ht_get(ht_t *table, uint16_t port) {
    uint16_t index = ht_hash(table, port);
    while(table->items[index].used) {
        if (table->items[index].port == port) {
            return table->items[index].value;
        }

        index = ht_hash(table, index + 1);
    }

    return NULL;
}

/*
 * Refer to headers/hash_table.h
 */
rcv_cfg_t *ht_put(ht_t *table, uint16_t port, rcv_cfg_t *item) {
    if (table->length > (table->size / 2)) {
        if (ht_resize(table, table->size * 2) != 0) {
            errno = FAILED_TO_RESIZE;
            return NULL;
        }

        return ht_put(table, port, item);
    }

    uint16_t index = ht_hash(table, port);
    rcv_cfg_t *old = NULL;
    while(table->items[index].used) {
        if (table->items[index].port == port) {
            old = table->items[index].value;
            break;
        }

        index = ht_hash(table, index + 1);
    }

    table->items[index].used = item != NULL;
    table->items[index].port = port;
    table->items[index].value = item;

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
rcv_cfg_t *ht_remove(ht_t *table, uint16_t port) {
    return ht_put(table, port, NULL);
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

    int i = 0;
    for(; i < old_size; i++) {
        if (old_values[i].used) {
            ht_put(table, old_values[i].port, old_values[i].value);
        }
    }

    free(old_values);
    
    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
size_t ht_length(ht_t *table) {
    return table->length;
}