#include <stdio.h>   
#include <stdlib.h> 
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <zlib.h>
#include "../headers/packet.h"
#include "../headers/errors.h"

int allocate_void(void *data, size_t length) {
    if (data != NULL) {
        errno = ALREADY_ALLOCATED;
        return -1;
    }

    data = calloc(1, length);
    if (data == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    return 0;
}

int deallocate_void(void *data) {
    if (data == NULL) {
        errno = ALREADY_DEALLOCATED;
        return -1;
    }

    free(data);
    return 0;
}

int alloc_packet(packet_t* packet) {
    return allocate_void((void *) packet, sizeof(packet_t));
}

int dealloc_packet(packet_t* packet) {
    if (packet->payload == NULL) {
        if (deallocate_void((void *) packet->payload) == -1) {
            return -1;
        }
    }

    return deallocate_void((void *) packet);
}


/*
 * Refer to headers/packet.h
 */
int unpack(uint8_t *packet, packet_t *out, uint8_t *payload) {
    uint8_t *raw = packet;
    {
        uint8_t ttrwin = *packet++;
        uint8_t type = (ttrwin & 0b11000000) >> 6;
        out->truncated = (ttrwin & 0b00100000) >> 5;
        out->window = ttrwin & 0b00011111;

        if (type == 0) {
            errno = TYPE_IS_WRONG;
            return -1;
        }

        out->type = type;
    }

    {
        uint8_t length = *packet++;
        out->long_length = (length & 0b10000000) >> 8;
        if (out->long_length) {
            out->length = (length & 0b01111111) | (*packet++ << 7);
            out->length = ntohs(out->length);
        } else {
            out->length = length & 0b01111111;
        }
    }

    {
        out->seqnum = *packet++;
    }

    {
        out->timestamp = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
        out->timestamp = ntohl(out->timestamp);
    }

    {
        out->crc1 = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
        out->crc1 = ntohl(out->crc1);
    }

    {
        if (payload != NULL) {
            if (out->payload != NULL) {
                free(out->payload);
            }
            out->payload = payload;
        }

        if (out->type == DATA && !out->truncated) {
            if(out->payload == NULL) {
                out->payload = calloc(sizeof(uint8_t), 512);
                if (out->payload == NULL) {
                    errno = FAILED_TO_ALLOCATE;
                    return -1;
                }
            }

            if (out->payload != memcpy(out->payload, packet, out->length)) {
                errno = FAILED_TO_COPY;
                return -1;
            }

            packet += out->length;

            out->crc2 = *packet++ | (*packet++ << 8) | (*packet++ << 16) | (*packet++ << 24);
            out->crc2 = ntohl(out->crc2);
        }
    }

    size_t len = 7;
    if (out->long_length) {
        len += 1;
    }

    uint32_t crc = crc32(0, (void*) raw, len);
    if (out->crc1 != crc) {
        errno = CRC_VALIDATION_FAILED;

        return -1;
    }

    if (out->payload != NULL && out->length > 0 && !out->truncated) {
        crc = crc32(0, (void*) out->payload, (size_t) out->length);
        if (out->crc2 != crc) {
            errno = PAYLOAD_VALIDATION_FAILED;

            return -1;
        }
    }

    return 0;
}

/*
 * Refer to headers/packet.h
 */
int pack(uint8_t *packet, packet_t *in, bool recompute_crc2) {
    uint8_t *raw = packet;

    if (in == NULL || packet == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    (*packet++) = (in->type << 6) | (in->truncated << 5) | (in->window & 0b00011111);

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

    uint32_t crc1 = htonl(crc32(0, (void *) raw, length));

    (*packet++) = (uint8_t) (crc1);
    (*packet++) = (uint8_t) (crc1 >> 8);
    (*packet++) = (uint8_t) (crc1 >> 16);
    (*packet++) = (uint8_t) (crc1 >> 24);

    if (in->payload != NULL && !in->truncated && in->type == DATA) {
        if (packet != memcpy(packet, in->payload, in->length)) {
            errno = FAILED_TO_COPY;
            return -1;
        }
        packet += in->length;

        uint32_t crc2 = ntohl(in->crc2);
        if (recompute_crc2) {
            crc2 = htonl(crc32(0, (void *) in->payload, in->length));
        }

        (*packet++) = (uint8_t) (crc2);
        (*packet++) = (uint8_t) (crc2 >> 8);
        (*packet++) = (uint8_t) (crc2 >> 16);
        (*packet++) = (uint8_t) (crc2 >> 24);
    }

    return 0;
}