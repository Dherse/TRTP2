
#include "./headers/ht_test.h"

void test_ht_put_and_get() {
    ht_t table;
    int res = allocate_ht(&table);
    CU_ASSERT(res == 0);
    if (res != 0) {
        return;
    }

    char *cfg = "Hello, world!";

    uint8_t ip[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    
    uint16_t i;
    for (i = 0; i < N; i++) {
        char *str = malloc(sizeof(char) * (strlen(cfg) + 1));
        strcpy(str, cfg);

        errno = 0;
        CU_ASSERT(ht_put(&table, i, ip, str) == NULL);
        CU_ASSERT(errno == 0);
    }

    for (i = N; i > 0; i--) {
        CU_ASSERT(ht_contains(&table, i-1, ip));
    }

    dealloc_ht(&table);
}

int add_ht_tests() {
    CU_pSuite pSuite = CU_add_suite("ht_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_ht_put_and_get", test_ht_put_and_get)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}