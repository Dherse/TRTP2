
#include <CUnit/CUnit.h>

#include "../headers/hash_table.h"

#define N 1024

void test_ht_put_and_get() {
    ht_t table;
    int res = allocate_ht(&table);
    CU_ASSERT(res == 0);
    if (res != 0) {
        return;
    }

    rcv_cfg_t cfg;

    for (uint16_t i = 0; i < N; i++) {
        errno = 0;
        CU_ASSERT(ht_put(&table, i, &cfg) == NULL);
        CU_ASSERT(errno == 0);
    }

    for (uint16_t i = 0; i < N; i++) {
        CU_ASSERT(ht_contains(&table, i));
    }
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