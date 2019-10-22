#include "./headers/buffer_test.h"

void test_transmit() {
    // TODO :for ALL POSSIBLE PARAMETERS
        // TODO : call receiver 
        // create socket, send preset messages
        // TODO : check ack answers and transmitted file.
}

int add_buffer_tests() {
    CU_pSuite pSuite = CU_add_suite("receiver_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_transmit", test_transmit)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}