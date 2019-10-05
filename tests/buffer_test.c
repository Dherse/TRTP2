
void test_fill_and_empty() {
    
}

int add_cli_tests() {
    CU_pSuite pSuite = CU_add_suite("cli_test_suite", 0, 0);

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