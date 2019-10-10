#include "../headers/receiver.h"
#include "../headers/global.h"

/*
 * Refer to headers/receiver.h
 */
void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t *decoded = calloc(1, sizeof(packet_t));
    if (decoded == NULL) {
        fprintf(stderr, "Failed to start handle thread: alloc failed\n");
        return NULL;
    }

    bool exit = false;

    packet_t *packets[31];

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
                    if (unpack(req->buffer, decoded) != 0) {
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
                        }
                    }
                    if (decoded->seqnum > (client->window->window_low + 31) || decoded->seqnum < client->window->window_low) {
                        fprintf(stderr, "[%s] Wildly out of sequence packet: %02X\n", ip_as_str, decoded->seqnum);
                    } else if (decoded->truncated) {
                        fprintf(
                            stderr, 
                            "[%s] Packet truncated: %02X\n", 
                            ip_as_str, 
                            decoded->seqnum
                        );

                        s_node_t *pack = stream_pop(cfg->send_rx, false);
                        if (pack == NULL) {
                            pack = calloc(1, sizeof(s_node_t));
                            if (pack == NULL) {
                                fprintf(stderr, "Failed to allocated s_node_t\n");

                                if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                    fprintf(stderr, "Failed to enqueue packet\n");
                                }

                                continue;
                            } else {
                                pack->content = calloc(1, sizeof(tx_req_t));
                                if (pack->content == NULL) {
                                    fprintf(stderr, "Failed to allocated tx_req_t\n");

                                    if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                        fprintf(stderr, "Failed to enqueue packet\n");
                                    }

                                    continue;
                                } 
                            }
                        }

                        tx_req_t *to_send = (tx_req_t *) pack->content;

                        to_send->stop = false;
                        to_send->address = client->address;

                        to_send->to_send.type = NACK;
                        to_send->to_send.window = 31 - client->window->length;
                        to_send->to_send.timestamp = decoded->timestamp;
                        to_send->to_send.seqnum = decoded->seqnum;

                        if (!stream_enqueue(cfg->send_tx, pack, true)) {
                            fprintf(stderr, "Failed to enqueue send request\n");
                        }
                    } else {
                        buf_t *window = client->window;
                        
                        node_t *spot = next(window, decoded->seqnum);
                        if (spot->value == NULL) {
                            spot->value = calloc(1, sizeof(packet_t));
                            if (spot->value == NULL) {
                                fprintf(stderr, "Failed to allocated packet_t\n");

                                if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                    fprintf(stderr, "Failed to enqueue packet\n");
                                }

                                continue;
                            } 
                        }

                        packet_t *temp = (packet_t *) spot->value;
                        packet_t *value = decoded;

                        spot->value = decoded;
                        decoded = temp;

                        unlock(spot);

                        if (value->window > 0) {
                            uint8_t i = window->window_low;
                            uint8_t cnt = 0;
                            uint8_t last_seqnum = window->window_low;
                            node_t* node = NULL;
                            pthread_mutex_lock(client->file_mutex);
                            do {
                                node = get(window, i, false, false);
                                if (node != NULL) {
                                    int result = write(
                                        client->out_fd, 
                                        ((packet_t *) node->value)->payload, 
                                        ((packet_t *) node->value)->length
                                    );

                                    if (result != ((packet_t *) node->value)->length) {
                                        fprintf(stderr, "Failed to write to file\n");
                                        break;
                                    }

                                    cnt++;
                                    last_seqnum = i;

                                    unlock(node);
                                }
                                i++;
                            } while(i < window->window_low + 30 && node != NULL);
                    
                            window->length -= cnt;
                            window->window_low = last_seqnum + 1;

                            pthread_mutex_unlock(client->file_mutex);

                            s_node_t *pack = stream_pop(cfg->send_rx, false);
                            if (pack == NULL) {
                                pack = calloc(1, sizeof(s_node_t));
                                if (pack == NULL) {
                                    fprintf(stderr, "Failed to allocated s_node_t\n");

                                    if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                        fprintf(stderr, "Failed to enqueue packet\n");
                                    }

                                    continue;
                                } else {
                                    pack->content = calloc(1, sizeof(tx_req_t));
                                    if (pack->content == NULL) {
                                        fprintf(stderr, "Failed to allocated tx_req_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, true)) {
                                            fprintf(stderr, "Failed to enqueue packet\n");
                                        }

                                        continue;
                                    } 
                                }
                            }

                            pack->next = NULL;

                            tx_req_t *to_send = (tx_req_t *) pack->content;

                            to_send->stop = false;
                            to_send->address = client->address;

                            to_send->to_send.type = ACK;
                            to_send->to_send.window = 31 - client->window->length;
                            to_send->to_send.timestamp = value->timestamp;
                            to_send->to_send.seqnum = last_seqnum;

                            if (!stream_enqueue(cfg->send_tx, pack, true)) {
                                fprintf(stderr, "Failed to enqueue send request\n");
                            }
                        }
                    }

                    if (!stream_enqueue(cfg->tx, node_rx, true)) {
                        fprintf(stderr, "Failed to enqueue packet\n");
                    }
                }
            }
        }
    }
    

    return NULL;
}