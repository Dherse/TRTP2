#include "../headers/receiver.h"
#include "../headers/global.h"

/*
 * Refer to headers/receiver.h
 */
void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t to_send;
    packet_t *decoded = calloc(1, sizeof(packet_t));
    if (decoded == NULL) {
        fprintf(stderr, "[HD] Failed to start handle thread: alloc failed\n");
        return NULL;
    }

    bool exit = false;

    while(!exit) {
        s_node_t *node_rx = stream_pop(cfg->rx, true);
        if (node_rx != NULL) {
            hd_req_t *req = (hd_req_t *) node_rx->content;
            if (req != NULL) {
                if (req->stop) {
                    free(req);
                    free(node_rx);
                    
                    exit = true;
                    break;
                }

                uint16_t port = req->port;
                uint8_t *ip = req->ip;
                ssize_t length = req->length;

                char ip_as_str[40];
                ip_to_string(ip, ip_as_str);

                client_t *client = (client_t *) ht_get(cfg->clients, port, ip);
                if (client == NULL) {
                    fprintf(stderr, "[%s] Unknown client\n", ip_as_str);
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

                        if (!stream_enqueue(cfg->tx, node_rx, true)) {
                            free(node_rx->content);
                            free(node_rx);
                            fprintf(stderr, "[HD] Failed to enqueue packet\n");
                        }

                        continue;
                    }

                    pthread_mutex_lock(client->file_mutex);
                    bool remove = false;
                    if (decoded->seqnum > client->window->window_low + 31 || decoded->seqnum < client->window->window_low) {
                        fprintf(stderr, "[%s] Out of sequence packet: %03d\n", ip_as_str, decoded->seqnum);
                    } else if (decoded->truncated) {
                        fprintf(
                            stderr, 
                            "[%s] Packet truncated: %02X\n", 
                            ip_as_str, 
                            decoded->seqnum
                        );

                        s_node_t *send_node = stream_pop(cfg->send_rx, false);

                        if (send_node == NULL) {
                            send_node = calloc(1, sizeof(s_node_t));
                            if (send_node == NULL) {
                                fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                    free(node_rx->content);
                                    free(node_rx);
                                    fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                }

                                continue;
                            } else {
                                send_node->content = calloc(1, sizeof(tx_req_t));
                                if (send_node->content == NULL) {
                                    fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");

                                    if (!stream_enqueue(cfg->tx, node_rx, true)) {
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

                        client->window_len = decoded->window + 1;

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

                        if (!stream_enqueue(cfg->send_tx, send_node, true)) {
                            free(send_node->content);
                            free(send_node);
                            fprintf(stderr, "[HD] Failed to enqueue send request\n");
                        }
                    } else if (decoded->type == DATA) {
                        buf_t *window = client->window;
                        
                        node_t *spot = next(window, decoded->seqnum, true);
                        if (spot != NULL) {
                            packet_t *temp = (packet_t *) spot->value;
                            packet_t *value = decoded;

                            spot->value = decoded;
                            decoded = temp;

                            unlock(spot);

                            client->first = false;

                            uint8_t i = window->window_low;
                            uint8_t cnt = 0;
                            uint8_t last_seqnum = window->window_low;
                            node_t* node = NULL;
                            do {
                                node = get(window, i, false, false);
                                if (node != NULL) {
                                    packet_t *pak = (packet_t *) node->value;

                                    int result = fwrite(
                                        pak->payload,
                                        sizeof(uint8_t),
                                        pak->length,
                                        client->out_file
                                    );

                                    if (pak->length == 0) {
                                        remove = true;
                                    } else {
                                        remove = false;
                                    }

                                    if (result != ((packet_t *) node->value)->length) {
                                        fprintf(stderr, "[HD] Failed to write to file\n");
                                        break;
                                    }

                                    cnt++;
                                    last_seqnum = i;

                                    unlock(node);
                                }
                                i++;
                            } while(i & 0xFF < (window->window_low + 30) & 0xFF && node != NULL && cnt <= value->seqnum);

                            if (cnt > 0) {
                                fflush(client->out_file);

                                window->length -= cnt;
                                window->window_low = last_seqnum + 1;

                                s_node_t *send_node = stream_pop(cfg->send_rx, false);
                                if (send_node == NULL) {
                                    send_node = calloc(1, sizeof(s_node_t));
                                    if (send_node == NULL) {
                                        fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                            free(node_rx->content);
                                            free(node_rx);
                                            fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                        }

                                        continue;
                                    } else {
                                        send_node->content = calloc(1, sizeof(tx_req_t));
                                        if (send_node->content == NULL) {
                                            fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");

                                            if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                                free(node_rx->content);
                                                free(node_rx);
                                                fprintf(stderr, "[HD] Failed to enqueue packet\n");
                                            }

                                            continue;
                                        } 
                                    }
                                }

                                send_node->next = NULL;

                                tx_req_t *send_req = (tx_req_t *) send_node->content;

                                send_req->stop = false;
                                send_req->address = client->address;
                                send_req->deallocate_address = remove;

                                client->window_len = decoded->window + 1;

                                to_send.type = ACK;
                                to_send.truncated = false;
                                to_send.long_length = false;
                                to_send.length = 0;
                                to_send.seqnum = last_seqnum + 1;
                                to_send.timestamp = value->timestamp;

                                if(value->window == 0) {
                                    to_send.window = 1;
                                } else {
                                    to_send.window = 31 - client->window->window_low;
                                }

                                if (pack(send_req->to_send, &to_send, false)) {
                                    fprintf(stderr, "[%s] Failed to pack ACk\n", ip_as_str);

                                    /** 
                                     * We move the window back by one to let the
                                     * retransmission timer do its thing
                                     */
                                    client->window->window_low--;
                                } else {
                                    if (remove) {
                                        fprintf(stderr, "[%s] Done transfering file\n", ip_as_str);

                                        ht_remove(cfg->clients, port, ip);

                                        fclose(client->out_file);

                                        free(client->addr_len);
                                        
                                        deallocate_buffer(client->window);
                                        free(client->window);

                                        pthread_mutex_unlock(client->file_mutex);
                                        pthread_mutex_destroy(client->file_mutex);
                                        free(client->file_mutex);

                                        free(client);

                                        fprintf(stderr, "[%s] Destroyed\n", ip_as_str);
                                    }

                                    if (!stream_enqueue(cfg->send_tx, send_node, true)) {
                                        free(send_node->content);
                                        free(send_node);
                                        fprintf(stderr, "[HD] Failed to enqueue send request\n");
                                    }
                                }
                            }
                        }
                    }

                    if (!stream_enqueue(cfg->tx, node_rx, false)) {
                        free(node_rx->content);
                        free(node_rx);
                        fprintf(stderr, "[HD] Failed to enqueue packet\n");
                    }
                    
                    if (!remove) {
                        pthread_mutex_unlock(client->file_mutex);
                    }
                }
            }
        }
    }
    
    free(decoded);
    fprintf(stderr, "[HD] Stopped\n");

    return NULL;
}