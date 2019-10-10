#include <CUnit/CUnit.h>

#include "../headers/buffer.h"

void test_fill_and_empty() {
    buf_t buf;
    
    int i = 0;
    int j = 0;
    for (j = 0; j < 256; j += 16) {
        int alloc = allocate_buffer(&buf, sizeof(packet_t));
        CU_ASSERT(alloc == 0);
        if (alloc != 0) {
            return;
        }

        int k = 0;
        for (k = 0; k < 32; k++) {
            CU_ASSERT(alloc_packet((packet_t *) buf.nodes[k].value) == 0);
        }

        node_t *node;
        for (i = j; i < j + 32; i++) {
            node = next(&buf, i);
            CU_ASSERT(node != NULL);
            if (node != NULL) {
                ((packet_t *) node->value)->seqnum = (uint8_t) i; 
            }

            unlock(node);
        }

        for (i = j; i < j + 32; i++) {
            node = get(&buf, i, false, false);
            CU_ASSERT(node != NULL);

            if (node != NULL) {
                CU_ASSERT(((packet_t *) node->value)->seqnum == (uint8_t) i); 
            }

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