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
inline uint16_t ht_hash(ht_t *table, uint8_t *key, uint8_t len) {
    return SuperFastHash((char *) key, (int) len) % table->size;
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

int dealloc_items(size_t len, item_t *items) {
    int i = 0;
    for (; i < len; i++) {
        if (items[i].value != NULL) {
            free(items[i].value);
        }
    }
}

/*
 * Refer to headers/hash_table.h
 */
int dealloc_ht(ht_t *table) {
    if (table == NULL) {
        errno = ALREADY_DEALLOCATED;
        return -1;
    }

    dealloc_items(table->size, table->items);

    free(table->items);

    table->size = 0;
    table->length = 0;

    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
bool ht_contains(ht_t* table, uint8_t *key, uint8_t len) {
    return ht_get(table, key, len) != NULL;
}

/*
 * Refer to headers/hash_table.h
 */
void *ht_get(ht_t *table, uint8_t *key, uint8_t len) {
    uint16_t index = ht_hash(table, key, len);
    while(table->items[index].used) {

        if (table->items[index].len = len) {
            int i = 0;
            bool equals = true;
            for(; i < len && equals; i++) {
                if (table->items[index].key[i] != key[i]) {
                    equals = false;
                }
            }

            if (equals) {
                return table->items[index].value;
            }
        }

        index = (index + 1) % table->size;
    }

    return NULL;
}

/*
 * Refer to headers/hash_table.h
 */
void *ht_put(ht_t *table, uint8_t *key, uint8_t len, void *item) {
    if (table->length > (table->size / 2)) {
        if (ht_resize(table, table->size * 2) != 0) {
            errno = FAILED_TO_RESIZE;
            return NULL;
        }

        return ht_put(table, key, len, item);
    }

    uint16_t index = ht_hash(table, key, len);
    void *old = NULL;
    while(table->items[index].used) {
        if (table->items[index].len == len) {
            int i = 0;
            bool equals = true;
            for(; i < len && equals; i++) {
                if (table->items[index].key[i] != key[i]) {
                    equals = false;
                }
            }

            if (equals) {
                old = table->items[index].value;
                break;
            }
        }

        index = (index + 1) % table->size;
    }

    table->items[index].used = item != NULL;
    table->items[index].key = key;
    table->items[index].len = len;
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
void *ht_remove(ht_t *table, uint8_t *key, uint8_t len) {
    return ht_put(table, key, len, NULL);
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
            ht_put(table, old_values[i].key, old_values[i].len, old_values[i].value);
            old_values[i].value = NULL;
        }
    }

    dealloc_items(old_size, old_values);
    free(old_values);
    
    return 0;
}

/*
 * Refer to headers/hash_table.h
 */
size_t ht_length(ht_t *table) {
    return table->length;
}