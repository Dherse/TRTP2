#include "CUnit/Basic.h"

#include "./headers/packet_test.h"
#include "./headers/cli_test.h"
#include "./headers/buffer_test.h"
#include "./headers/ht_test.h"
#include "./headers/stream_test.h"
#include "./headers/Tout_test.h"

/*#include "handler_test.c"
#include "receiver_test.c"*/

int main() {
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);

    add_packet_tests();

    add_cli_tests();

    add_buffer_tests();

    add_ht_tests();

    add_stream_tests();

    add_global_tests();

    /*add_handler_tests();

    add_receiver_tests();*/

    CU_basic_run_tests();

    return CU_get_error();
}