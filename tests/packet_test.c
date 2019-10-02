#include <CUnit/CUnit.h>

#include "../headers/global.h"

void test_packet_decoding() {
    uint8_t ack_packet[11] = {
        // Type + TR + Window
        0b10101010,

        // L + Length
        0b00000000,

        // Seqnum
        0b10101010,

        // timestamp
        0b10101010,
        0b00000000,
        0b01010101,
        0b00000000,

        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
    };

    uint32_t crc1 = crc32(0, (void*) ack_packet, 7);
    uint32_t crc = htonl(crc1);
    memcpy(ack_packet + 7, &crc, sizeof(uint32_t));

    packet_t packet;

    CU_ASSERT(unpack((uint8_t *) &ack_packet, &packet, NULL) == 0);

    CU_ASSERT(packet.type == ACK);
    CU_ASSERT(!packet.truncated);
    CU_ASSERT(packet.window == 0b01010);
    CU_ASSERT(!packet.long_length);
    CU_ASSERT(packet.length == 0);
    CU_ASSERT(packet.seqnum == 0b10101010);
    CU_ASSERT(packet.timestamp == 0b10101010000000000101010100000000);
    CU_ASSERT(packet.crc1 == crc1);
}

int add_packet_tests() {
    CU_pSuite pSuite = CU_add_suite("packet_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_packet_decoding", test_packet_decoding)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}