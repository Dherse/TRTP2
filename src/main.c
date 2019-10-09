#include "../headers/main.h"
#include "../headers/hash_table.h"

int main() {
    ht_t table;
    int res = allocate_ht(&table);
    if (!(res == 0)) { return -1; } 
    if (res != 0) {
        return -1;
    }

    char *cfg = "Hello, world!";

    for (uint16_t i = 0; i < 1024; i++) {
        char *str = malloc(sizeof(char) * (strlen(cfg) + 1));
        strcpy(str, cfg);

        uint8_t *key = malloc(sizeof(uint8_t));
        key[0] = i & 0xFF;
        key[1] = i >> 8;

        errno = 0;
        if (!(ht_put(&table, key, 2, str) == NULL)) { return -1; } 
        if (!(errno == 0)) { return -1; } 
    }

    for (uint16_t i = 0; i < 1024; i++) {
        uint8_t *key = malloc(sizeof(uint8_t));
        key[0] = i & 0xFF;
        key[1] = i >> 8;

        if (!(ht_contains(&table, key, 2))) { return -1; } 
    }

    dealloc_ht(&table);

    return 0;
}