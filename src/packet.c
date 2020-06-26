#include "../headers/packet.h"

#define CRC32H(old, value, length) crc32_16bytes(value, length, old)
#define CRC32P(old, value, length) crc32_16bytes_prefetch(value, length, old, MAX_PAYLOAD_SIZE)
#define U32_FROM_BUFFER(buffer) __extension__ ({ \
    uint8_t a = *(buffer++); \
    uint8_t b = *(buffer++); \
    uint8_t c = *(buffer++); \
    uint8_t d = *(buffer++); \
    a | (b << 8) | (c << 16) | (d << 24); \
})

/**
 * Refer to headers/packet.h
 */
uint16_t get_length(packet_t *self) {
    return self->length;
}

/**
 * Refer to headers/packet.h
 */
bool is_long(packet_t *self) {
    return self->long_length;
}

/**
 * Refer to headers/packet.h
 */
void set_length(packet_t *self, uint16_t length) {
    if (length > MAX_PAYLOAD_SIZE) {
        return;
    }

    if ((length & 0x11111) != length) {
        self->long_length = true;
    }

    self->length = length;
}

/**
 * Refer to headers/packet.h
 */
uint8_t *get_payload(packet_t *self) {
    return self->payload;
}

/**
 * Refer to headers/packet.h
 */
void set_payload(packet_t *self, uint8_t *payload, uint16_t len) {
    if (len > MAX_PAYLOAD_SIZE) {
        return;
    }

    set_length(self, len);

    memcpy(self->payload, payload, len);
    memset(self->payload + len, 0, MAX_PAYLOAD_SIZE - len);
}

/**
 * Refer to headers/packet.h
 */
int init_packet(packet_t* packet) {
    if (packet == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    packet->crc1 = 0;
    packet->crc2 = 0;
    packet->length = 0;
    packet->long_length = false;
    memset(packet->payload, 0, MAX_PAYLOAD_SIZE);
    packet->seqnum = 0;
    packet->timestamp = 0;
    packet->truncated = false;
    packet->type = 0;
    packet->window = 0;

    return 0;
}

/**
 * Refer to headers/packet.h
 */
int dealloc_packet(packet_t* packet) {
    free(packet);
    return 0;
}

/**
 * Refer to headers/packet.h
 */
int unpack(uint8_t *packet, int length, packet_t *out) {
    uint8_t *buffer = packet;
    int length_rest = length;

    if ((length_rest -= 1) <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    uint8_t *header_pointer = buffer;
    uint8_t header = *buffer++;
    out->type      = (header & 0xC0) >> 6;
    out->truncated = (header & 0x20) >> 5;
    out->window    = (header & 0x1F);

    if ((length_rest -= 1) <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    uint8_t size    = *buffer++;
    uint8_t is_long = (size & 0x80) >> 7;
    if (is_long) {
        if ((length_rest -= 1) <= 0) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        out->long_length = is_long;
        out->length = ntohs((size & 0x7F) | (*buffer++ << 8));
    } else {
        out->length = (size & 0x7F);
        out->long_length = false;
    }

    if ((length_rest -= 1) <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->seqnum = *buffer++;

    if ((length_rest -= 4) <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->timestamp = ntohl(U32_FROM_BUFFER(buffer));

    if ((length_rest -= 4) < 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->crc1 = ntohl(U32_FROM_BUFFER(buffer));

    size_t len = 7 + is_long;
    *header_pointer &= 0xDF;

    uint32_t crc = CRC32H(0, (void*) packet, len);
    if (out->crc1 != crc) {
        errno = CRC_VALIDATION_FAILED;

        return -1;
    }

    if (length_rest == 0 && out->type == DATA && !out->truncated && out->length != 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    if (out->length > MAX_PAYLOAD_SIZE) {
        errno = PACKET_INCORRECT_LENGTH;
        return -1;
    }

    if (out->type == DATA && !out->truncated && out->length != 0) {
        if ((length_rest -= out->length) < 4) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        if (&out->payload != memcpy(&out->payload, buffer, out->length)) {
            errno = FAILED_TO_COPY;
            return -1;
        }

        buffer += out->length;

        if ((length_rest -= 4) < 0) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        out->crc2 = ntohl(U32_FROM_BUFFER(buffer));
    }

    if (length_rest > 0) {
        LOG("PKT", "%d\n", length_rest);
        errno = PACKET_TOO_LONG;
        return -1;
    }

    if (out->type == IGNORE) {
        errno = TYPE_IS_WRONG;

        return -1;
    }

    if (out->type != DATA && out->truncated) {
        errno = NON_DATA_TRUNCATED;

        return -1;
    }

    if (out->length > MAX_PAYLOAD_SIZE) {
        errno = PAYLOAD_TOO_LONG;
        return -1;
    } else if (out->length > 0 && !out->truncated) {
        crc = CRC32P(0, (void*) out->payload, (size_t) out->length);
        if (out->crc2 != crc) {
            errno = PAYLOAD_VALIDATION_FAILED;

            return -1;
        }
    }

    return 0;
}

/**
 * Refer to headers/packet.h
 */
int pack(uint8_t *packet, packet_t *in, bool recompute_crc2) {
    if (in == NULL || packet == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    uint8_t *raw = packet;

    uint8_t *header = packet++;

    *header = (in->type << 6) | (in->window & 0x1F);

    uint8_t length = 7;
    if (in->long_length) {
        length++;

        uint16_t len = htons(in->length);
        (*packet++) = (uint8_t) (0x80 | (len & 0x7F));
        (*packet++) = (uint8_t) (len >> 7);
    } else {
        (*packet++) = (uint8_t) (in->length & 0x7F);
    }

    (*packet++) = in->seqnum;

    uint32_t timestamp = htonl(in->timestamp);

    (*packet++) = (uint8_t) (timestamp);
    (*packet++) = (uint8_t) (timestamp >> 8);
    (*packet++) = (uint8_t) (timestamp >> 16);
    (*packet++) = (uint8_t) (timestamp >> 24);

    uint32_t crc1 = htonl(CRC32H(0, (uint8_t *) raw, length));

    *header = *header | (in->truncated << 5);

    (*packet++) = (uint8_t) (crc1);
    (*packet++) = (uint8_t) (crc1 >> 8);
    (*packet++) = (uint8_t) (crc1 >> 16);
    (*packet++) = (uint8_t) (crc1 >> 24);

    if (!in->truncated && in->type == DATA && in->length > 0) {
        if (packet != memcpy(packet, &in->payload, in->length)) {
            errno = FAILED_TO_COPY;
            return -1;
        }
        packet += in->length;

        uint32_t crc2 = ntohl(in->crc2);
        if (recompute_crc2) {
            crc2 = htonl(CRC32P(0, (void *) &in->payload, in->length));
        }

        (*packet++) = (uint8_t) (crc2);
        (*packet++) = (uint8_t) (crc2 >> 8);
        (*packet++) = (uint8_t) (crc2 >> 16);
        (*packet++) = (uint8_t) (crc2 >> 24);
    }

    return 0;
}

/**
 * Refer to headers/packet.h
 */
int packet_to_string(packet_t* packet, bool print_payload) {
    if (packet == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    char s_type[15];
    switch(packet->type) {
        case 0: strcpy(s_type, "IGNORE (00)"); break;
        case 1: strcpy(s_type, "DATA (01)"); break;
        case 2: strcpy(s_type, "ACK (10)"); break;
        case 3: strcpy(s_type, "NACK (11)"); break;
    }

    uint16_t len = packet->length;

    fprintf(stderr, " - - - - - - - - PACKET - - - - - - - - \n");
    fprintf(stderr, "TYPE:       %s\n", s_type);
    fprintf(stderr, "TRUNCATED:  %s\n", packet->truncated ? "true" : "false");
    fprintf(stderr, "WINDOW:     %u\n", packet->window);
    fprintf(stderr, "SEQNUM:     %u\n", packet->seqnum);
    fprintf(stderr, "LENGTH (L): %u (%s)\n", len, packet->long_length ? "true" : "false");
    fprintf(stderr, "TIMESTAMP:  %u\n", packet->timestamp);

    if (print_payload) {
        fprintf(stderr, "\n");

        uint8_t *payload = packet->payload;

        uint16_t i;
        for(i = 0; i < len; i= i + 8) {
            fprintf(
                stderr, 
                "%02x %02x %02x %02x %02x %02x %02x %02x | %3s %3s %3s %3s %3s %3s %3s %3s\n", 
                payload[i], 
                payload[i + 1], 
                payload[i + 2], 
                payload[i + 3], 
                payload[i + 4], 
                payload[i + 5], 
                payload[i + 6], 
                payload[i + 7], 
                ascii[payload[i]], 
                ascii[payload[i + 1]], 
                ascii[payload[i + 2]], 
                ascii[payload[i + 3]], 
                ascii[payload[i + 4]], 
                ascii[payload[i + 5]], 
                ascii[payload[i + 6]], 
                ascii[payload[i + 7]]
            );
        }
    }

    fprintf(stderr, " - - - - - - - - - - - - - - - - - - - -\n");

    return 0;
}

/**
 * Refer to headers/packet.h
 */
int ip_to_string(struct sockaddr_in6 *ip, char *target) {
    if (target == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    if (target != inet_ntop(AF_INET6, &ip->sin6_addr, target, INET6_ADDRSTRLEN)) {
        perror("inet_ntop");
        errno = FAILED_TO_COPY;
        return -1;
    }

    return 0;
}

/**
 * Refer to headers/packet.h
 */
inline __attribute__((always_inline)) bool ip_equals(uint8_t *ip1, uint8_t *ip2) {
    return memcmp(ip1, ip2, 16) == 0;
}

/**
 * Refer to headers/packet.h
 */
void *allocate_packet() {
    packet_t *pack = malloc(sizeof(packet_t));
    if (pack == NULL || init_packet(pack)) {
        return NULL;
    }

    return pack;
}
