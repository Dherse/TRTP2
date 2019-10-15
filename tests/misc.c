#include <CUnit/CUnit.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

void test_window() {
    /*FILE *fd = fopen("./headers/lookup.h", "wb");

    int i = 0;
    int j = 0;
    int k = 0;
    int valids[31];
    int contained;
    for (i = 0; i < 256; i++) {
        int max = ((i + 31) & 0xFF);

        for (j = 0; j < 31; j++) {
            valids[j] = (i + j) & 0xFF;
        }
        fprintf(fd, "\t{ \n");

        for (j = 0; j < 256; j++) {
            contained = 0;
            for (k = 0; k < 31 && !contained; k++) {
                if (valids[k] == j) {
                    contained = 1;
                }
            }
            if (j % 16 == 0) {
                fprintf(fd, "\n\t\t");
            }
            fprintf(fd, "%d, ", contained);
        }
        fprintf(fd, "\b\b\n \t},\n");
    }
    fprintf(fd, "} \n");*/


}

int add_misc_tests() {
    CU_pSuite pSuite = CU_add_suite("misc_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_window", test_window)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}