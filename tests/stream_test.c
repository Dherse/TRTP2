#include <CUnit/CUnit.h>

#include "../headers/stream.h"

void test_single() {
    stream_t stream;
    allocate_stream(&stream, 1);

    s_node_t node;

    CU_ASSERT(stream_enqueue(&stream, &node, false) == true);
    CU_ASSERT(stream.length == 1);

    CU_ASSERT(stream_pop(&stream, false) != NULL);
    CU_ASSERT(stream.length == 0);

    dealloc_stream(&stream);
}

void test_many() {
    stream_t stream;
    allocate_stream(&stream, 2048);

    for (int i = 0; i < 1024; i++) {
        int *cnt = malloc(sizeof(int));
        *cnt = i;
        s_node_t *node = malloc(sizeof(s_node_t));
        node->content = cnt;


        CU_ASSERT(stream_enqueue(&stream, node, false) == true);
        CU_ASSERT(stream.length == i + 1);
    }

    printf("\nfilled\n");

    for (int i = 0; i < 1024; i++) {
        s_node_t *node = stream_pop(&stream, false);
        CU_ASSERT(node != NULL);
        if (node != NULL) {
            CU_ASSERT(*((int *) node->content) == i);
        }

        CU_ASSERT(stream.length == 1023 - i);
    }

    dealloc_stream(&stream);
}

int add_stream_tests() {
    CU_pSuite pSuite = CU_add_suite("ht_stream_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_single", test_single)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_many", test_many)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}