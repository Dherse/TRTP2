#include <stdio.h>   
#include <stdlib.h> 
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <zlib.h>
#include <time.h>
#include "../headers/packet.h"
#include "../headers/errors.h"

int allocate_void(void *data, size_t length) {
    if (data != NULL) {
        errno = ALREADY_ALLOCATED;
        return -1;
    }

    void *d = calloc(1, length);
    if (d == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    data = d;

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

/*
 * Refer to headers/packet.h
 */
int alloc_packet(packet_t* packet) {
    packet_t* temp = calloc(1, sizeof(packet_t));
    if (temp == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return -1;
    }

    *packet = *temp;

    free(temp);
    return 0;
}

/*
 * Refer to headers/packet.h
 */
int dealloc_packet(packet_t* packet) {
    return deallocate_void((void *) packet);
}

/*
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

    out->received_time = ((double) clock()) / CLOCKS_PER_SEC;

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

    uint8_t *header = packet++;

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

    *header = (in->type << 6) | (in->window & 0b00011111);

    uint32_t crc1 = htonl(crc32(0, (void *) raw, length));

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

/*
 * Refer to headers/packet.h
 */
int packet_to_string(const packet_t* packet){
    if (packet == NULL) {
        errno = NULL_ARGUMENT;
        return -1;
    }

    char s_type[15];
    char s_trun[6];
    char s_longlength[9];

    switch(packet->type) {
        case 0: strcpy(s_type, "IGNORE (00)"); break;
        case 1: strcpy(s_type, "DATA (01)"); break;
        case 2: strcpy(s_type, "ACK (10)"); break;
        case 3: strcpy(s_type, "NACK (11)"); break;
    }
    
    if(packet->truncated == 1) {
        strcpy(s_trun, "true");
    } else {
        strcpy(s_trun, "false");
    }

    if(packet->long_length == 1) {
        strcpy(s_longlength, "15 bits");
    } else {
        strcpy(s_longlength, "7 bits");
    }
    
    printf("type : %s\n truncated : %s\n window : %u\n length is %s long\n length : %u\n seqnum : %u\n timestamp : %u\n",
     s_type, s_trun, packet->window, s_longlength, packet->length, packet->seqnum, packet->timestamp);

    uint16_t i;
    for(i = 0; i < packet->length ; i= i + 4) {
        printf(
            "%02x %02x %02x %02x | %c %c %c %c\n", 
            packet->payload[i], 
            packet->payload[i + 1], 
            packet->payload[i + 2], 
            packet->payload[i + 3], 
            (char) packet->payload[i], 
            (char) packet->payload[i + 1], 
            (char) packet->payload[i + 2], 
            (char) packet->payload[i + 3]
        );
    }

    return 0;
}

void ip_to_string(uint8_t *ip, char *target) {
    sprintf(
        target, 
        "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
        ip[0],
        ip[1],
        ip[2],
        ip[3],
        ip[4],
        ip[5],
        ip[6],
        ip[7],
        ip[8],
        ip[9],
        ip[10],
        ip[11],
        ip[12],
        ip[13],
        ip[14],
        ip[15]
    );
}