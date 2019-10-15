#include "../headers/receiver.h"
#include "../headers/global.h"

/*
 * Refer to headers/receiver.h
 */
void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t to_send;
    packet_t *decoded = (packet_t *) allocate_packet();
    if (decoded == NULL) {
        fprintf(stderr, "[HD] Failed to start handle thread: alloc failed\n");
        return NULL;
    }

    bool exit = false;

    while(!exit) {
        s_node_t *node_rx = stream_pop(cfg->rx, true);
        if (node_rx == NULL) {
            sched_yield();
            continue;
        }

        if (node_rx != NULL) {
            hd_req_t *req = (hd_req_t *) node_rx->content;
            if (req != NULL) {
                if (req->stop) {
                    free(req);
                    free(node_rx);
                    
                    exit = true;
                    break;
                }

                ssize_t length = req->length;

                char ip_as_str[40];
                ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);

                client_t *client = req->client;
                if (client == NULL) {
                    fprintf(stderr, "[%s][%5u] Unknown client\n", ip_as_str, client->address->sin6_port);
                } else {
                    errno = 0;
                    if (unpack(req->buffer, req->length, decoded) != 0) {
                        switch(errno) {
                            case TYPE_IS_WRONG:
                                fprintf(stderr, "[%s] Type is wrong: %d\n", ip_as_str, decoded->type);
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

                        if (!stream_enqueue(cfg->tx, node_rx, false)) {
                            free(node_rx->content);
                            free(node_rx);
                            fprintf(stderr, "[HD] Failed to enqueue packet\n");
                        }

                        continue;
                    };

                    pthread_mutex_lock(client->lock);
                    bool remove = false;
                    if (decoded->truncated) {
                        fprintf(
                            stderr, 
                            "[%s][%5u] Packet truncated: %02X\n", 
                            ip_as_str,
                            client->address->sin6_port,
                            decoded->seqnum
                        );
                        
                        s_node_t *send_node = stream_pop(cfg->send_rx, false);

                        if (send_node == NULL) {
                            send_node = calloc(1, sizeof(s_node_t));
                            if (send_node == NULL) {
                                fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                    free(node_rx->content);
                                    free(node_rx);
                                    fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                }

                                continue;
                            } else {
                                send_node->content = calloc(1, sizeof(tx_req_t));
                                if (send_node->content == NULL) {
                                    fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");

                                    if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                        free(node_rx->content);
                                        free(node_rx);
                                        fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                    }

                                    continue;
                                } 
                            }
                        }

                        tx_req_t *send_req = (tx_req_t *) send_node->content;

                        send_req->stop = false;
                        send_req->address = client->address;

                        to_send.type = NACK;
                        to_send.truncated = false;
                        to_send.window = 31 - client->window->length;
                        to_send.long_length = false;
                        to_send.length = 0;
                        to_send.seqnum = decoded->seqnum;
                        to_send.timestamp = decoded->timestamp;

                        if (pack(send_req->to_send, &to_send, false)) {
                            fprintf(stderr, "[%s] Failed to pack NACk\n", ip_as_str);
                        } 

                        stream_enqueue(cfg->send_tx, send_node, true);
                    } else if (decoded->type == DATA) {
                        buf_t *window = client->window;
                        node_t *spot;

                        if (!sequences[client->window->window_low][decoded->seqnum]) {
                            fprintf(
                                stderr, 
                                "[%s][%5u] Out of order packet (window low: %d, got: %d)\n",
                                ip_as_str,
                                client->address->sin6_port,
                                client->window->window_low,
                                decoded->seqnum
                            );

                            spot = NULL;

                        } else if(is_used(window, decoded->seqnum)) {
                            fprintf(
                                stderr, 
                                "[%s][%5u] Packet received twice (window low: %d, got: %d)\n",
                                ip_as_str,
                                client->address->sin6_port,
                                client->window->window_low,
                                decoded->seqnum
                            );
                        } else {
                            spot = next(window, decoded->seqnum, true);
                        }

                        if (spot != NULL) {
                            packet_t *temp = (packet_t *) spot->value;
                            packet_t *value = decoded;

                            spot->value = decoded;
                            decoded = temp;

                            unlock(spot);

                            uint8_t i = window->window_low;
                            uint8_t cnt = 0;
                            node_t* node = NULL;
                            packet_t *pak;
                            do {
                                unlock(node);

                                node = get(window, i, false, false);
                                if (node != NULL) {
                                    pak = (packet_t *) node->value;

                                    if (pak->length > 0) {
                                        int result = fwrite(
                                            pak->payload,
                                            sizeof(uint8_t),
                                            pak->length,
                                            client->out_file
                                        );

                                        if (result != ((packet_t *) node->value)->length) {
                                            fprintf(stderr, "[HD] Failed to write to file\n");
                                            break;
                                        }
                                    }

                                    cnt++;
                                    i++;
                                }
                            } while(i & 0xFF < (window->window_low + 30) & 0xFF && node != NULL);

                            unlock(node);

                            if (cnt > 0) {
                                fflush(client->out_file);

                                if (pak->length == 0) {
                                    remove = true;
                                } else {
                                    remove = false;
                                }

                                window->length -= cnt;
                                window->window_low += cnt;

                                s_node_t *send_node = stream_pop(cfg->send_rx, false);
                                if (send_node == NULL) {
                                    send_node = malloc(sizeof(s_node_t));
                                    if (send_node == NULL) {
                                        fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                            free(node_rx->content);
                                            free(node_rx);
                                            fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                        }

                                        continue;
                                    }

                                    send_node->content = NULL;
                                    send_node->next = NULL;
                                }

                                if (send_node->content == NULL) {
                                    tx_req_t* content = malloc(sizeof(tx_req_t));
                                    if (content == NULL) {
                                        fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                            free(node_rx->content);
                                            free(node_rx);
                                            fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                        }

                                        continue;
                                    }

                                    content->address = NULL;
                                    content->deallocate_address = false;
                                    content->stop = false;
                                    memset(content->to_send, 0, 11);

                                    send_node->content = content;
                                }

                                send_node->next = NULL;

                                tx_req_t *send_req = (tx_req_t *) send_node->content;

                                send_req->stop = false;
                                send_req->address = client->address;
                                send_req->deallocate_address = false;

                                to_send.type = ACK;
                                to_send.truncated = false;
                                to_send.long_length = false;
                                to_send.length = 0;
                                to_send.seqnum = window->window_low;
                                to_send.timestamp = value->timestamp;
                                to_send.window = 31 - window->length;

                                if (pack(send_req->to_send, &to_send, false)) {
                                    fprintf(stderr, "[%s][%5u] Failed to pack ACK\n", ip_as_str, client->address->sin6_port);

                                    /** 
                                     * We move the window back by one to let the
                                     * retransmission timer do its thing
                                     */
                                    client->window->window_low--;
                                } else {
                                    if (remove) {
                                        uint16_t port = client->address->sin6_port;
                                        fprintf(stderr, "[%s][%5u] Done transfering file\n", ip_as_str, client->address->sin6_port);

                                        /*ht_remove(cfg->clients, port, client->address->sin6_addr.__in6_u.__u6_addr8);

                                        fclose(client->out_file);

                                        free(client->addr_len);
                                        
                                        deallocate_buffer(client->window);
                                        free(client->window);

                                        pthread_mutex_unlock(client->file_mutex);
                                        pthread_mutex_destroy(client->file_mutex);
                                        free(client->file_mutex);

                                        free(client);*/

                                        fprintf(stderr, "[%s][%5u] Destroyed\n", ip_as_str, port);
                                    }

                                    stream_enqueue(cfg->send_tx, send_node, true);
                                }
                            }
                        }
                    }

                    if (!stream_enqueue(cfg->tx, node_rx, false)) {
                        free(node_rx->content);
                        free(node_rx);
                    }
                    
                    if (!remove) {
                        pthread_mutex_unlock(client->lock);
                    }
                }
            }
        }
    }
    
    free(decoded);
    fprintf(stderr, "[HD] Stopped\n");
    
    pthread_exit(0);

    return NULL;
}