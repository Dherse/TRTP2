#define _GNU_SOURCE
#include "../headers/receiver.h"
#include "../headers/handler.h"

void print_unpack_error(char *ip_as_str) {
    switch(errno) {
        case TYPE_IS_WRONG:
            fprintf(stderr, "[%s] Type is wrong\n", ip_as_str);
            break;
        case NON_DATA_TRUNCATED:
            fprintf(stderr, "[%s] Non-PData packet truncated\n", ip_as_str);
            break;
        case CRC_VALIDATION_FAILED:
            fprintf(stderr, "[%s] Header could not be validated\n", ip_as_str);
            break;
        case PAYLOAD_VALIDATION_FAILED:
            fprintf(stderr, "[%s] Payload could not be validated\n", ip_as_str);
            break;
        case PACKET_TOO_SHORT:
            fprintf(stderr, "[%s] Packet has incorrect size (too short)\n", ip_as_str);
            break;
        case PACKET_TOO_LONG:
            fprintf(stderr, "[%s] Packet has incorrect size (too long)\n", ip_as_str);
            break;
        case PAYLOAD_TOO_LONG:
            fprintf(stderr, "[%s] Payload has incorrect size (too long)\n", ip_as_str);
            break;
        default:
            fprintf(stderr, "[%s] Unknown packet error\n", ip_as_str);
            break;
    }
}

/*
 * Refer to headers/handler.h
 */
void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    pthread_t thread = pthread_self();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cfg->id + 2, &cpuset);

    int aff = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (aff == -1) {
        fprintf(stderr, "[HD] Failed to set affinity\n");
    }

    packet_t to_send;

    uint8_t len_to_send;
    uint8_t packets_to_send[31][12];
    struct mmsghdr msg[31];
    struct iovec iovecs[31];

    memset(iovecs, 0, sizeof(iovecs));
    int i = 0;
    for(; i < 31; i++) {
        memset(&iovecs[i], 0, sizeof(struct iovec));
        iovecs[i].iov_base = packets_to_send[i];
        iovecs[i].iov_len  = 11;

        memset(&msg[i], 0, sizeof(struct mmsghdr));
        msg[i].msg_hdr.msg_iov = &iovecs[i];
        msg[i].msg_hdr.msg_iovlen = 1;
        msg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in6);
    }


    uint16_t offset;
    uint8_t file_buffer[512*31];

    packet_t *decoded = (packet_t *) allocate_packet();
    char ip_as_str[46];
    if (decoded == NULL) {
        fprintf(stderr, "[HD] Failed to start handle thread: alloc failed\n");
        return NULL;
    }

    bool exit = false;
    while(!exit) {
        len_to_send = 0;
        s_node_t *node_rx = stream_pop(cfg->rx, true);
        if (node_rx == NULL) {
            sched_yield();
            continue;
        }

        if (node_rx != NULL) {
            hd_req_t *req = (hd_req_t *) node_rx->content;
            if (req != NULL) {
                if (req->stop) {
                    deallocate_node(node_rx);
                    
                    exit = true;
                    break;
                }

                client_t *client = req->client;
                buf_t *window = client->window;
                pthread_mutex_lock(buf_get_lock(window));
                pthread_mutex_lock(client_get_lock(client));

                int i = 0;
                for (i = 0; i < req->num; i++) {
                    uint8_t *buffer = req->buffer[i];
                    int length = req->lengths[i];

                    if (unpack(buffer, length, decoded)) {
                        print_unpack_error(client->ip_as_string);

                        continue;
                    }

                    if (decoded->type == DATA) {
                        if (decoded->truncated) {
                            to_send.type = NACK;
                            to_send.truncated = false;
                            to_send.window = 31 - client->window->length;
                            to_send.long_length = false;
                            to_send.length = 0;
                            to_send.seqnum = min(cfg->max_window_size, 31 - window->window_low);
                            to_send.timestamp = decoded->timestamp;

                            msg[len_to_send].msg_hdr.msg_name = client->address;
                            if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                                fprintf(stderr, "[%s][%5u] Failed to pack NACK\n", ip_as_str, ntohs(client->address->sin6_port));
                            }
                        } else if (!sequences[window->window_low][decoded->seqnum]) {
                            fprintf(
                                stderr, 
                                "[%s][%5u] Received out of order: %d (low: %d)\n", 
                                ip_as_str, ntohs(client->address->sin6_port),
                                decoded->seqnum, window->window_low
                            );
                    
                            to_send.type = ACK;
                            to_send.truncated = false;
                            to_send.long_length = false;
                            to_send.length = 0;
                            to_send.seqnum = window->window_low;
                            to_send.timestamp = decoded->timestamp;
                            to_send.window = min(cfg->max_window_size, 31 - window->length);

                            msg[len_to_send].msg_hdr.msg_name = client->address;
                            if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                                fprintf(stderr, "[%s][%5u] Failed to pack ACK\n", ip_as_str, ntohs(client->address->sin6_port));
                            }
                        } else if(is_used_nolock(window, decoded->seqnum)) {
                            fprintf(
                                stderr, 
                                "[%s][%5u] Received duplicate: %d (low: %d)\n", 
                                ip_as_str, ntohs(client->address->sin6_port),
                                decoded->seqnum, window->window_low
                            );
                        } else {
                            node_t *spot = next_nolock(window, decoded->seqnum, true);

                            packet_t *temp = (packet_t *) spot->value;
                            spot->value = decoded;
                            decoded = temp;

                            unlock(spot);
                        }
                    }
                }

                offset = 0;

                node_t *node;
                packet_t *pak;
                int cnt = 0;
                uint32_t last_timestamp = 0;
                i = window->window_low;
                bool remove;
                do {
                    node = get_nolock(window, i & 0xFF, false, false);
                    if (node != NULL) {

                        pak = (packet_t *) node->value;

                        if (pak->length > 0) {
                            memcpy(file_buffer + offset, pak->payload, pak->length);
                            offset += pak->length;

                            remove = false;
                        } else {
                            remove = true;
                        }

                        last_timestamp = pak->timestamp;

                        cnt++;
                        i++;
                    }
                    unlock(node);
                } while(sequences[client->window->window_low][i & 0xFF] && node != NULL && cnt < 31);

                pthread_mutex_unlock(buf_get_lock(window));

                if (cnt > 0) {
                    int result = fwrite(
                        file_buffer,
                        sizeof(uint8_t),
                        offset,
                        client->out_file
                    );

                    if (result != offset) {
                        fprintf(stderr, "[HD] Failed to write to file, won't be writing ACK to get retransmission timer\n");
                        pthread_mutex_lock(client_get_lock(client));
                        continue;
                    }

                    window->length = (window->length - cnt);
                    window->window_low += cnt;
                    
                    to_send.type = ACK;
                    to_send.truncated = false;
                    to_send.long_length = false;
                    to_send.length = 0;
                    to_send.seqnum = window->window_low;
                    to_send.timestamp = last_timestamp;
                    to_send.window = min(cfg->max_window_size, 31 - window->length);

                    msg[len_to_send].msg_hdr.msg_name = client->address;
                    if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                        fprintf(stderr, "[%s][%5u] Failed to pack NACK\n", ip_as_str, client->address->sin6_port);
                    }
                }

                int retval = sendmmsg(cfg->sockfd, msg, len_to_send, 0);
                if (retval == -1) {
                    fprintf(stderr, "[TX] sendmmsg failed\n ");
                    perror("sendmmsg()");
                }

                if (!stream_enqueue(cfg->tx, node_rx, false)) {
                    deallocate_node(node_rx);
                }

                if (!remove) {
                    pthread_mutex_unlock(client_get_lock(client));
                }
            }
        }
    }
    
    free(decoded);
    fprintf(stderr, "[HD] Stopped\n");
    
    pthread_exit(0);

    return NULL;
}

