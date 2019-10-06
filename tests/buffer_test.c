#include <CUnit/CUnit.h>

#include "../headers/buffer.h"

void test_fill_and_empty() {
    buf_t buf;
    
    int i = 0;
    int j = 0;
    for (j = 0; j < 256; j += 16) {
        int alloc = allocate_buffer(&buf);
        CU_ASSERT(alloc == 0);
        if (alloc != 0) {
            return;
        }

        node_t *node;
        for (i = j; i < j + 32; i++) {
            node = next(&buf);
            CU_ASSERT(node != NULL);

            node->packet->seqnum = (uint8_t) i; 

            unlock(node);
        }

        for (i = j; i < j + 32; i++) {
            node_t *node = peek(&buf, false, true);
            CU_ASSERT(node != NULL);

            CU_ASSERT(node->packet->seqnum == (uint8_t) i); 

            unlock(node);
        }
    
        deallocate_buffer(&buf);
    }
}

int add_buffer_tests() {
    CU_pSuite pSuite = CU_add_suite("buffer_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(pSuite, "test_fill_and_empty", test_fill_and_empty)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}