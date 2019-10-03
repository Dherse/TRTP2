#include <CUnit/CUnit.h>

#include "../headers/global.h"

uint8_t ack_packet[11] = {
    // Type + TR + Window
    0b10001010,

    // L + Length
    0b00000000,

    // Seqnum
    0b10101010,

    // timestamp
    0b10101010,
    0b00000000,
    0b01010101,
    0b00000000,

    // CRC1
    0, 0, 0, 0
};

void test_ack_decoding() {

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

void test_ack_encoding() {
    packet_t packet;
    packet.type = ACK;
    packet.truncated = false;
    packet.window = 0b01010;
    packet.long_length = false;
    packet.length = 0;
    packet.seqnum = 0b10101010;
    packet.timestamp = 0b10101010000000000101010100000000;
    packet.payload = NULL;
    packet.crc2 = 0;

    uint8_t *packed = malloc(sizeof(uint8_t) * 1024);
    CU_ASSERT(pack(packed, &packet, false) == 0);

    for(int i = 0; i < sizeof(ack_packet); i++) {
        CU_ASSERT(ack_packet[i] == (*packed++));
    }
}

void test_data_decoding() {
    char *str = "hello, world!";

    int len = strlen(str) + 1;
    uint32_t crc2 = crc32(0, (void*) str, len);
    uint32_t crc2_n = htonl(crc2);
    //memcpy(ack_packet + 7 + strlen(str), &crc2_n, sizeof(uint32_t));

    uint8_t dat_packet[256] = {
        // Type + TR + Window
        0b01001010,

        // L + Length
        len,

        // Seqnum
        0b10101010,

        // timestamp
        0b10101010,
        0b00000000,
        0b01010101,
        0b00000000,

        // CRC1
        0, 0, 0, 0,

        // Payload
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,

        // CRC2
        0, 0, 0, 0
    };

    // Copy CRC1
    uint32_t crc1 = crc32(0, (void*) dat_packet, 7);
    uint32_t crc1_n = htonl(crc1);
    memcpy(dat_packet + 7, &crc1_n, sizeof(uint32_t));

    // Copy the str
    memcpy(dat_packet + 11, (uint8_t *)str, len);

    // Copy CRC2
    memcpy(dat_packet + 11 + len, &crc2_n, sizeof(uint32_t));

    packet_t packet;

    CU_ASSERT(unpack((uint8_t *) &dat_packet, &packet, NULL) == 0);

    CU_ASSERT(packet.type == DATA);
    CU_ASSERT(!packet.truncated);
    CU_ASSERT(packet.window == 0b01010);
    CU_ASSERT(!packet.long_length);
    CU_ASSERT(packet.length == len);
    CU_ASSERT(packet.seqnum == 0b10101010);
    CU_ASSERT(packet.timestamp == 0b10101010000000000101010100000000);
    CU_ASSERT(packet.crc1 == crc1);
    CU_ASSERT(strcmp((char *) packet.payload, str) == 0);
    CU_ASSERT(packet.crc2 == crc2);
}

void test_data_encoding() {
    char *str = "hello, world!";

    int len = strlen(str) + 1;
    uint32_t crc2 = crc32(0, (void*) str, len);
    uint32_t crc2_n = htonl(crc2);

    uint8_t dat_packet[29] = {
        // Type + TR + Window
        0b01001010,

        // L + Length
        len,

        // Seqnum
        0b10101010,

        // timestamp
        0b10101010,
        0b00000000,
        0b01010101,
        0b00000000,

        // CRC1
        0, 0, 0, 0,

        // Payload
        0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0,

        // CRC2
        0, 0, 0, 0
    };

    // Copy CRC1
    uint32_t crc1 = crc32(0, (void*) dat_packet, 7);
    uint32_t crc1_n = htonl(crc1);
    memcpy(dat_packet + 7, &crc1_n, sizeof(uint32_t));

    // Copy the str
    memcpy(dat_packet + 11, (uint8_t *)str, len);

    // Copy CRC2
    memcpy(dat_packet + 11 + len, &crc2_n, sizeof(uint32_t));

    packet_t packet;
    packet.type = DATA;
    packet.truncated = false;
    packet.window = 0b01010;
    packet.long_length = false;
    packet.length = strlen(str) + 1;
    packet.seqnum = 0b10101010;
    packet.timestamp = 0b10101010000000000101010100000000;
    packet.payload = (uint8_t *) str;
    packet.crc2 = crc2;

    toString(&packet);

    uint8_t *packed = malloc(sizeof(uint8_t) * 1024);
    CU_ASSERT(pack(packed, &packet, false) == 0);

    for(int i = 0; i < sizeof(dat_packet); i++) {
        CU_ASSERT(dat_packet[i] == (*packed++));
    }
}

int add_packet_tests() {
    CU_pSuite pSuite = CU_add_suite("packet_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_ack_decoding", test_ack_decoding)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_data_decoding", test_data_decoding)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_ack_encoding", test_ack_encoding)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_data_encoding", test_data_encoding)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}