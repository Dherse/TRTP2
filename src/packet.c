#include <stdio.h>   
#include <stdlib.h> 
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
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
    {
        uint8_t ttrwin = *packet++;
        uint8_t _type = (uint8_t) (ttrwin & 0b11000000 >> 6);
        out->truncated = ttrwin & 0b00100000 >> 5;
        out->window = ttrwin & 0b00011111;

        if (_type == 0) {
            errno = TYPE_IS_WRONG;
            return -1;
        }

        out->type = _type;
    }

    {
        uint8_t length = *packet++;
        out->long_length = length & 0b10000000 >> 8;
        if (out->long_length) {
            out->length = (length & 0b01111111) | ((*packet++) << 7);
            out->length = ntohs(out->length);
        } else {
            out->length = length & 0b01111111;
        }
    }

    {
        out->seqnum = *packet++;
    }

    {
        memcpy(&out->timestamp, packet, sizeof(uint32_t));
        packet += sizeof(uint32_t);

        out->timestamp = ntohl(out->timestamp);
    }

    {
        memcpy(&out->crc1, packet, sizeof(uint32_t));
        packet += sizeof(uint32_t);

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

            memcpy(&out->crc2, packet, sizeof(uint32_t));
            packet += sizeof(uint32_t);

            out->crc2 = ntohl(out->crc2);
        }
    }

    return 0;
}