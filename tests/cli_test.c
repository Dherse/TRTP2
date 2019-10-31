#include "./headers/cli_test.h"

void free_config_contents(config_rcv_t *config) {
    if (config->format && config->deallocate_format) {
        free(config->format);
    }

    if (config->addr_info != NULL) {
        freeaddrinfo(config->addr_info);
    }

    if (config->receive_affinities != NULL) {
        free(config->receive_affinities);
    }

    if (config->handle_affinities != NULL) {
        free(config->handle_affinities);
    }

    if (config->handle_streams != NULL) {
        free(config->handle_streams);
    }

    if (config->receive_streams != NULL) {
        free(config->receive_streams);
    }
}

void test_cli_noopt() {
    config_rcv_t config;
    memset(&config, 0, sizeof(config_rcv_t));

    char *p_1 = "trtp_receiver";
    char *p0 = "::1";
    char *p1 = "1234";
    char *params[] = { p_1, p0, p1 };

    CU_ASSERT(parse_receiver(3, params, &config) == 0);
    CU_ASSERT(config.format_len == 2);
    CU_ASSERT(strcmp(config.format, "%d") == 0);
    CU_ASSERT(config.max_connections == 100);
    CU_ASSERT(config.port == 1234);

    free_config_contents(&config);
}

void test_cli_all_opt() {
    config_rcv_t config;
    memset(&config, 0, sizeof(config_rcv_t));

    char *p_1 = "trtp_receiver";
    char *p0 = "-m";
    char *p1 = "128";
    char *p2 = "-o";
    char *p3 = "\"my_file_with_space_%d\"";
    char *p4 = "::1";
    char *p5 = "1234";
    char *params[] = { p_1, p0, p1, p2, p3, p4, p5};

    CU_ASSERT(parse_receiver(7, params, &config) == 0);
    CU_ASSERT(strcmp(config.format, "my_file_with_space_%d") == 0);
    CU_ASSERT(config.max_connections == 128);
    CU_ASSERT(config.port == 1234);

    free_config_contents(&config);
}

int add_cli_tests() {
    CU_pSuite pSuite = CU_add_suite("cli_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_cli_noopt", test_cli_noopt)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_cli_all_opt", test_cli_all_opt)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}