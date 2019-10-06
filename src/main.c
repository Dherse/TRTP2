#include "../headers/main.h"
#include "../headers/hash_table.h"

int main() {
    ht_t table;

    int res = allocate_ht(&table);
    if (res != 0) {
        return -1;
    }

    for (uint16_t i = 0; i < 1<<15; i++) {
        errno = 0;

        rcv_cfg_t *cfg = malloc(sizeof(rcv_cfg_t));
        if (cfg == NULL) {
            return -1;
        }

        if(ht_put(&table, i, cfg) != NULL || errno != 0) {
            return -1;
        }
    }

    for (uint16_t i = 0; i < 1<<15; i++) {
        if (!ht_contains(&table, i)) {
            return -1;
        }
    }

    if (dealloc_ht(&table) != 0) {
        return -1;
    }

    return 0;
}