#include "../headers/packet.h"

GETSET_IMPL(packet_t, ptype_t, type);

GETSET_IMPL(packet_t, bool, truncated);

GETSET_IMPL(packet_t, uint8_t, window);

GETSET_IMPL(packet_t, uint8_t, seqnum);

GETSET_IMPL(packet_t, uint32_t, timestamp);

GETSET_IMPL(packet_t, uint32_t, crc1);

GETSET_IMPL(packet_t, uint32_t, crc2);

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
    if (length > 512) {
        return;
    }

    if (length & 0x11111 != length) {
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
    if (len > 512) {
        return;
    }

    set_length(self, len);

    memcpy(self->payload, payload, len);
    memset(self->payload + len, 0, 512 - len);
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
    memset(packet->payload, 0, 512);
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
    uint8_t *raw = packet;

    if (--length <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    uint8_t *header = packet++;

    uint8_t ttrwin = *header;
    uint8_t type = (ttrwin & 0b11000000) >> 6;
    out->truncated = (ttrwin & 0b00100000) >> 5;
    out->window = ttrwin & 0b00011111;

    out->type = type;

    if (--length <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    uint8_t size = *packet++;
    out->long_length = (size & 0b10000000) >> 7;
    if (out->long_length) {
        if (--length <= 0) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        out->length = (size & 0b01111111) | (*packet++ << 8);
        out->length = ntohs(out->length);
    } else {
        out->length = size & 0b01111111;
    }

    if (--length <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->seqnum = *packet++;

    if ((length -= 4) <= 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->timestamp = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
    out->timestamp = ntohl(out->timestamp);

    if ((length -= 4) < 0) {
        errno = PACKET_TOO_SHORT;
        return -1;
    }

    out->crc1 = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
    out->crc1 = ntohl(out->crc1);

    if (out->type == DATA && !out->truncated && out->length != 0) {
        if ((length -= out->length) <= 0) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        if (&out->payload != memcpy(&out->payload, packet, out->length)) {
            errno = FAILED_TO_COPY;
            return -1;
        }

        packet += out->length;

        if ((length -= 4) < 0) {
            errno = PACKET_TOO_SHORT;
            return -1;
        }

        out->crc2 = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
        out->crc2 = ntohl(out->crc2);
    }

    if (length > 0 && !out->truncated) {
        errno = PACKET_TOO_LONG;
        return -1;
    }

    size_t len = 7;
    if (out->long_length) {
        len += 1;
    }

    *header = *header & 0b11011111;
    uint32_t crc = crc32(0, (void*) raw, len);
    if (out->crc1 != crc) {
        errno = CRC_VALIDATION_FAILED;

        return -1;
    }

    if (type == 0) {
        errno = TYPE_IS_WRONG;

        return -1;
    }

    if (type != DATA && out->truncated) {
        errno = NON_DATA_TRUNCATED;

        return -1;
    }

    if (out->length > 512) {
        errno = PAYLOAD_TOO_LONG;
        return -1;
    } else if (out->payload != NULL && out->length > 0 && !out->truncated) {
        crc = crc32(0, (void*) out->payload, (size_t) out->length);
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

    *header = (in->type << 6) | (in->window & 0b00011111);

    uint8_t length = 7;
    if (in->long_length) {
        length++;

        uint16_t len = htons(in->length);
        (*packet++) = (uint8_t) (0b10000000 | (len & 0b01111111));
        (*packet++) = (uint8_t) (len >> 7);
    } else {
        (*packet++) = (uint8_t) (in->length & 0b01111111);
    }

    (*packet++) = in->seqnum;

    uint32_t timestamp = htonl(in->timestamp);

    (*packet++) = (uint8_t) (timestamp);
    (*packet++) = (uint8_t) (timestamp >> 8);
    (*packet++) = (uint8_t) (timestamp >> 16);
    (*packet++) = (uint8_t) (timestamp >> 24);

    uint32_t crc1 = htonl(crc32(0, (uint8_t *) raw, length));

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
            crc2 = htonl(crc32(0, (void *) &in->payload, in->length));
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
    switch(get_type(packet)) {
        case 0: strcpy(s_type, "IGNORE (00)"); break;
        case 1: strcpy(s_type, "DATA (01)"); break;
        case 2: strcpy(s_type, "ACK (10)"); break;
        case 3: strcpy(s_type, "NACK (11)"); break;
    }

    uint16_t len = get_length(packet);

    fprintf(stderr, " - - - - - - - - PACKET - - - - - - - - \n");
    fprintf(stderr, "TYPE:       %s\n", s_type);
    fprintf(stderr, "TRUNCATED:  %s\n", get_truncated(packet) ? "true" : "false");
    fprintf(stderr, "WINDOW:     %u\n", get_window(packet));
    fprintf(stderr, "SEQNUM:     %u\n", get_seqnum(packet));
    fprintf(stderr, "LENGTH (L): %u (%s)\n", len, is_long(packet) ? "true" : "false");
    fprintf(stderr, "TIMESTAMP:  %u\n", get_timestamp(packet));

    if (print_payload) {
        fprintf(stderr, "\n");

        uint8_t *payload = get_payload(packet);

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
int ip_to_string(uint8_t *ip, char *target) {
    if (target == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    struct in6_addr addr;
    if (addr.__in6_u.__u6_addr8 != memcpy(addr.__in6_u.__u6_addr8, ip, 16)) {
        errno = FAILED_TO_COPY;
        return -1;
    }

    if (target != inet_ntop(AF_INET6, &addr, target, 18)) {
        errno = FAILED_TO_COPY;
        return -1;
    }

    return 0;
}

/**
 * Refer to headers/packet.h
 */
bool ip_equals(uint8_t *ip1, uint8_t *ip2) {
    if (ip1 == NULL) {
        return ip2 == NULL;
    } else if (ip2 == NULL) {
        return false;
    } else {
        return ip1[0] == ip2[0] &&
            ip1[1] == ip2[1] && 
            ip1[2] == ip2[2] && 
            ip1[3] == ip2[3] && 
            ip1[4] == ip2[4] && 
            ip1[5] == ip2[5] && 
            ip1[6] == ip2[6] && 
            ip1[7] == ip2[7] && 
            ip1[8] == ip2[8] && 
            ip1[9] == ip2[9] && 
            ip1[10] == ip2[10] && 
            ip1[11] == ip2[11] && 
            ip1[12] == ip2[12] && 
            ip1[13] == ip2[13] && 
            ip1[14] == ip2[14] && 
            ip1[15] == ip2[15]; 
    }
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