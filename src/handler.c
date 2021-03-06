#define _GNU_SOURCE
#include "../headers/receiver.h"
#include "../headers/handler.h"
#include "../headers/global.h"


#define LOG_ERROR(message, id, port, ip_as_str, ...) \
    LOG("HANDLER][ERROR", "Error on client #%d [%s]:%u: " message "\n", id, ip_as_str, ntohs(port), __VA_ARGS__)

#define LOG_ERRORN(message, id, port, ip_as_str) \
    LOG("HANDLER][ERROR", "Error on client #%d [%s]:%u: " message "\n", id, ip_as_str, ntohs(port))


void print_unpack_error(uint32_t id, uint16_t port, char *ip_as_str) {
    switch(errno) {
        case TYPE_IS_WRONG:
            LOG_ERRORN("Type is wrong", id, port, ip_as_str);
            break;
        case NON_DATA_TRUNCATED:
            LOG_ERRORN("Non-PData packet truncated", id, port, ip_as_str);
            break;
        case CRC_VALIDATION_FAILED:
            LOG_ERRORN("Header could not be validated", id, port, ip_as_str);
            break;
        case PAYLOAD_VALIDATION_FAILED:
            LOG_ERRORN("Payload could not be validated", id, port, ip_as_str);
            break;
        case PACKET_TOO_SHORT:
            LOG_ERRORN("Packet has incorrect size (too short)", id, port, ip_as_str);
            break;
        case PACKET_TOO_LONG:
            LOG_ERRORN("Packet has incorrect size (too long)", id, port, ip_as_str);
            break;
        case PAYLOAD_TOO_LONG:
            LOG_ERRORN("Payload has incorrect size (too long)", id, port, ip_as_str);
            break;
        default:
            LOG_ERROR("Unknown packet error (errno = %d)", id, port, ip_as_str, errno);
            break;
    }
}

void bytes_to_unit(uint64_t total, char *size, double *multiplier) {
    char *sizes[5] = {
        "B", "KiB", "MiB", "GiB", "TiB"
    };

    int i;
    *multiplier = 1.0;
    for (i = 0; i < 5; i++) {
        if (total > 1024 && i != 4) {
            total /= 1024;
            *multiplier *= 1024;
        } else {
            strcpy(size, sizes[i]);
            break;
        }
    }
}

/*
 * Refer to headers/buffer.h
 */
void hd_run_once(
    bool wait,
    hd_cfg_t *cfg,
    packet_t **decoded,
    bool *exit,
    uint8_t file_buffer[MAX_PACKET_SIZE * MAX_WINDOW_SIZE],
    uint8_t packets_to_send[][12],
    struct mmsghdr *msg
) {
    packet_t to_send;
    int len_to_send = 0;
    s_node_t *node_rx = stream_pop(cfg->rx, wait);
    if (node_rx == NULL) {
        sched_yield();
        return;
    }

    hd_req_t *req = (hd_req_t *) node_rx->content;
    if (req != NULL) {
        if (req->stop == true) {
            LOG("HD", "Received STOP (%d)\n", cfg->id);
            free(*decoded);
            deallocate_node(node_rx);
            
            *exit = true;
            return;
        }

        client_t *client = req->client;
        buf_t *window = client->window;
        pthread_mutex_lock(client_get_lock(client));
        uint32_t last_timestamp = client->last_timestamp;

        size_t i = 0;
        for (i = 0; i < req->num; i++) {
            uint8_t *buffer = req->buffer[i];
            int length = req->lengths[i];

            if (unpack(buffer, length, *decoded)) {
                to_send.type = ACK;
                to_send.truncated = false;
                to_send.seqnum = window->window_low;
                to_send.long_length = false;
                to_send.length = 0;
                to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->length);
                to_send.timestamp = client->last_timestamp;

                msg[len_to_send].msg_hdr.msg_name = client->address;
                if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                    LOG_ERROR("Failed to pack ACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                    len_to_send--;
                }

                print_unpack_error(client->id, client->address->sin6_port, client->ip_as_string);
                
                continue;
            }

            last_timestamp = (*decoded)->timestamp;

            if (!client->active) {
                to_send.type = ACK;
                to_send.truncated = false;
                to_send.seqnum = window->window_low;
                to_send.long_length = false;
                to_send.length = 0;
                to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->length);
                to_send.timestamp = (*decoded)->timestamp;

                msg[len_to_send].msg_hdr.msg_name = client->address;
                if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                    LOG_ERROR("Failed to pack ACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                    len_to_send--;
                }
            } else if ((*decoded)->type == DATA) {
                if ((*decoded)->truncated) {
                    to_send.type = NACK;
                    to_send.truncated = false;
                    to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->window_low);
                    to_send.long_length = false;
                    to_send.length = 0;
                    to_send.seqnum = (*decoded)->seqnum;
                    to_send.timestamp = (*decoded)->timestamp;

                    msg[len_to_send].msg_hdr.msg_name = client->address;
                    if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                        LOG_ERROR("Failed to pack NACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                        len_to_send--;
                    }

                    TRACE(
                        "Received truncated: %d (low: %d) for client #%d [%s]:%u\n", 
                        (*decoded)->seqnum, window->window_low,
                        client->id, client->ip_as_string, ntohs(client->address->sin6_port)
                    );
                } else if (!sequences[window->window_low][(*decoded)->seqnum]) {
                    to_send.type = ACK;
                    to_send.truncated = false;
                    to_send.seqnum = window->window_low;
                    to_send.long_length = false;
                    to_send.length = 0;
                    to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->length);
                    to_send.timestamp = (*decoded)->timestamp;

                    msg[len_to_send].msg_hdr.msg_name = client->address;
                    if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                        LOG_ERROR("Failed to pack ACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                        len_to_send--;
                    }

                    TRACE(
                        "Received out of order: %d (low: %d) for client #%d [%s]:%u\n", 
                        (*decoded)->seqnum, window->window_low,
                        client->id, client->ip_as_string, ntohs(client->address->sin6_port)
                    );
                } else if(is_used(window, (*decoded)->seqnum)) {
                    to_send.type = ACK;
                    to_send.truncated = false;
                    to_send.seqnum = window->window_low;
                    to_send.long_length = false;
                    to_send.length = 0;
                    to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->length);
                    to_send.timestamp = (*decoded)->timestamp;

                    msg[len_to_send].msg_hdr.msg_name = client->address;
                    if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                        LOG_ERROR("Failed to pack ACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                        len_to_send--;
                    }

                    TRACE(
                        "Received duplicate: %d (low: %d) for client #%d [%s]:%u\n", 
                        (*decoded)->seqnum, window->window_low,
                        client->id, client->ip_as_string, ntohs(client->address->sin6_port)
                    );
                } else {
                    node_t *spot = next(window, (*decoded)->seqnum);
                    if (spot == NULL) {
                        LOGN("HD", "Internal error");
                    }

                    packet_t *temp = (packet_t *) spot->value;
                    spot->value = *decoded;
                    *decoded = temp;
                }
            }
        }

        int offset = 0;

        node_t *node;
        packet_t *pak;
        int cnt = 0;
        i = window->window_low;
        bool remove;
        do {
            node = get(window, i & 0xFF, false);
            if (node != NULL) {
                pak = (packet_t *) node->value;

                if (pak->length > 0) {
                    memcpy(file_buffer + offset, pak->payload, pak->length);
                    offset += pak->length;

                    remove = false;
                } else if (pak->length == 0 && !remove) {
                    remove = true;
                }

                last_timestamp = pak->timestamp;

                cnt++;
                i++;
            }
        } while(sequences[client->window->window_low][i & 0xFF] && node != NULL && cnt < MAX_WINDOW_SIZE && !remove);

        if (cnt > 0) {
            int result = fwrite(
                file_buffer,
                sizeof(uint8_t),
                offset,
                client->out_file
            );

            if (result != offset) {
                fseek(client->out_file, -result, SEEK_SET);

                LOGN("HD", "Failed to write to file, won't be writing ACK to get retransmission timer\n");
                enqueue_or_free(cfg->tx, node_rx);

                pthread_mutex_unlock(client_get_lock(client));


                return;
            }

            client->transferred += offset;

            window->length -= cnt;
            window->window_low += cnt;
            client->last_timestamp = last_timestamp;

            if (remove && client->active) {
                client->active = false;
                fclose(client->out_file);

                time_t end;
                char size[4], speed[4];
                double sizem = 0.0, speedm = 0.0;

                time(&end);

                bytes_to_unit(client->transferred, size, &sizem);

                client->end_time = malloc(sizeof(struct timespec));
                if (!client->end_time) {
                    LOGN("MAIN][ERROR", "Failed to allocate timespec\n");
                }

                clock_gettime(CLOCK_MONOTONIC, client->end_time);

                double time = ((double)client->end_time->tv_sec + 1.0e-9*client->end_time->tv_nsec) - 
                    ((double)client->connection_time.tv_sec + 1.0e-9*client->connection_time.tv_nsec);
                    
                bytes_to_unit(client->transferred / time, speed, &speedm);

                LOG(
                    "HD",
                    "Done, total transferred: %.1f %s, in %.2fs., avg. speed of %.1f %s/s for client #%d [%s]:%d\n",
                    client->transferred / sizem, size,
                    time,
                    (client->transferred / time) / speedm, speed,
                    client->id, client->ip_as_string, ntohs(client->address->sin6_port)
                );
            }
            
            to_send.type = ACK;
            to_send.truncated = false;
            to_send.long_length = false;
            to_send.length = 0;
            to_send.seqnum = window->window_low;
            to_send.timestamp = last_timestamp;
            to_send.window = min(cfg->max_window_size, MAX_WINDOW_SIZE - window->length);

            msg[len_to_send].msg_hdr.msg_name = client->address;
            if (pack(packets_to_send[len_to_send++], &to_send, false)) {
                LOG_ERROR("Failed to pack ACK (errno = %d)", client->id, client->address->sin6_port, client->ip_as_string, errno);
                len_to_send--;
            }
        }
        pthread_mutex_unlock(client_get_lock(client));
    
        int retval = sendmmsg(cfg->sockfd, msg, len_to_send, 0);
        if (retval == -1) {
            LOG("TX", "sendmmsg failed (fd: %d, len_to_send: %d)\n ", cfg->sockfd, len_to_send);
            perror("sendmmsg()");
        }

        enqueue_or_free(cfg->tx, node_rx);
    } else {
        free(node_rx);
    }
}

/*
 * Refer to headers/handler.h
 */
void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    if (cfg->affinity != NULL) {
        pthread_t thread = pthread_self();

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cfg->affinity->cpu, &cpuset);

        int aff = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
        if (aff == -1) {
            LOGN("HD", "Failed to set affinity\n");
        } else {
            LOG("HD", "Handler #%hhu running on CPU #%zu\n", cfg->id, cfg->affinity->cpu);
        }
    }

    uint8_t packets_to_send[MAX_WINDOW_SIZE + 1][12];
    struct mmsghdr msg[MAX_WINDOW_SIZE + 1];
    struct iovec iovecs[MAX_WINDOW_SIZE + 1];

    memset(iovecs, 0, sizeof(iovecs));
    int i = 0;
    for(; i < MAX_WINDOW_SIZE + 1; i++) {
        memset(&iovecs[i], 0, sizeof(struct iovec));
        iovecs[i].iov_base = packets_to_send[i];
        iovecs[i].iov_len  = 11;

        memset(&msg[i], 0, sizeof(struct mmsghdr));
        msg[i].msg_hdr.msg_iov = &iovecs[i];
        msg[i].msg_hdr.msg_iovlen = 1;
        msg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in6);
    }

    uint8_t file_buffer[MAX_PAYLOAD_SIZE * MAX_WINDOW_SIZE];

    packet_t *decoded = (packet_t *) allocate_packet();
    init_packet(decoded);
    if (decoded == NULL) {
        LOGN("HD", "Failed to start handle thread: alloc failed\n");
        return NULL;
    }

    bool exit = false;
    while(!exit) {
        hd_run_once(
            true,
            cfg,
            &decoded,
            &exit,
            file_buffer,
            packets_to_send,
            msg
        );
    }
    
    LOGN("HD", "Stopped\n");
    
    pthread_exit(0);
}


/**
 * Refer to headers/receiver.h
 */ 
s_node_t *pop_and_check_req(stream_t *stream, void *(*allocator)()) {
    s_node_t *node = stream_pop(stream, false);

    if (node == NULL) {
        node = malloc(sizeof(s_node_t));
        if(initialize_node(node, allocator)) {
            free(node);
            return NULL;
        }
    }
    return node;
}

/*
 * Refer to headers/handler.h
 */
inline void enqueue_or_free(stream_t *stream, s_node_t *node) {
    if (stream_enqueue(stream, node) == false) {
        TRACEN("Failed to enqueue, freeing\n");
        deallocate_node(node);
    }
}

/*
 * Refer to headers/receiver.h
 */
void *allocate_handle_request() {
    hd_req_t *req = (hd_req_t *) malloc(sizeof(hd_req_t));
    if(req == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return NULL;
    }

    int i = 0;
    for (; i < MAX_WINDOW_SIZE; i++) {
        memset(req->buffer[i], 0, MAX_PACKET_SIZE);
        req->lengths[i] = 0;
    }
    
    req->stop = false;
    req->client = NULL;
    req->num = 0;

    return req;
}
