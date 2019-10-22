#include "./headers/stream_test.h"

int global_value = 0;

void *empty_allocator() {
    return &global_value;
}

void test_single() {
    stream_t stream;
    allocate_stream(&stream);

    s_node_t *node = calloc(1, sizeof(s_node_t));
    CU_ASSERT(initialize_node(node, empty_allocator) == 0);

    CU_ASSERT(stream_enqueue(&stream, node, false) == true);
    //CU_ASSERT(stream.length == 1);

    CU_ASSERT(stream_pop(&stream, false) != NULL);
    //CU_ASSERT(stream.length == 0);

    dealloc_stream(&stream);

    free(node);
}

void test_many() {
    stream_t stream;
    allocate_stream(&stream);

    int i;
    for (i = 0; i < 1024; i++) {
        int *cnt = calloc(1, sizeof(int));
        *cnt = i;
        
        s_node_t *node = calloc(1, sizeof(s_node_t));
        CU_ASSERT(initialize_node(node, empty_allocator) == 0);
        node->content = cnt;


        CU_ASSERT(stream_enqueue(&stream, node, false) == true);
        //CU_ASSERT(stream.length == i + 1);
    }

    for (i = 0; i < 1024; i++) {
        s_node_t *node = stream_pop(&stream, false);
        CU_ASSERT(node != NULL);
        if (node != NULL) {
            CU_ASSERT(*((int *) node->content) == i);
            free(node);
        }

        //CU_ASSERT(stream.length == 1023 - i);
    }

    dealloc_stream(&stream);
}

int add_stream_tests() {
    CU_pSuite pSuite = CU_add_suite("stream_test_suite", 0, 0);

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