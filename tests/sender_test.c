#include <CUnit/CUnit.h>

#include "../headers/receiver.h"

void test_send(){

}

int add_sender_tests() {
    CU_pSuite pSuite = CU_add_suite("sender_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(pSuite, "test_send", test_send)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    

    return 0;
}