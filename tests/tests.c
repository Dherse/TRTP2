#include "../headers/global.h"
#include "CUnit/Basic.h"
#include "packet_test.c"

int main() {
    if (CUE_SUCCESS != CU_initialize_registry()) {
      return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);

    add_packet_tests();

    CU_basic_run_tests();

    return CU_get_error();
}