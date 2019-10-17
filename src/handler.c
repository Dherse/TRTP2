#include "../headers/receiver.h"
#include "../headers/handler.h"

/*
 * Refer to headers/handler.h
 */
void *handle_thread_temp(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t to_send;
    packet_t *decoded = (packet_t *) allocate_packet();
    char ip_as_str[46];
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
                    deallocate_node(node_rx);
                    
                    exit = true;
                    break;
                }

                ssize_t length = req->length;

                client_t *client = req->client;
                if (client == NULL) {
                    fprintf(stderr, "[HD] Unknown client\n");
                } else {
                    pthread_mutex_lock(client_get_lock(client));
                    if (unpack(req->buffer, req->length, decoded)) {
                        ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
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
                            deallocate_node(node_rx);
                        }

                        pthread_mutex_unlock(client_get_lock(client));
                        continue;
                    };

                    bool remove = false;
                    if (decoded->truncated) {
                        buf_t *window = client->window;
                        
                        ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                        fprintf(
                            stderr, 
                            "[%s][%5u] Packet truncated: %02X\n", 
                            ip_as_str,
                            ntohs(client->address->sin6_port),
                            decoded->seqnum
                        );
                        
                        s_node_t *send_node = stream_pop(cfg->send_rx, false);

                        if (send_node == NULL) {
                            send_node = calloc(1, sizeof(s_node_t));
                            if (send_node == NULL || initialize_node(send_node, allocate_send_request)) {
                                fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                    deallocate_node(node_rx);
                                }

                                continue;
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
                        to_send.seqnum = min(cfg->max_window_size, 31 - window->window_low);
                        to_send.timestamp = decoded->timestamp;

                        if (pack(send_req->to_send, &to_send, false)) {
                            fprintf(stderr, "[%s] Failed to pack NACk\n", ip_as_str);
                        } 

                        stream_enqueue(cfg->send_tx, send_node, true);
                    } else if (decoded->type == DATA) {
                        buf_t *window = client->window;

                        pthread_mutex_lock(buf_get_lock(window));

                        if (!sequences[client->window->window_low][decoded->seqnum]) {
                            ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                            pthread_mutex_unlock(buf_get_lock(window));

                            fprintf(
                                stderr, 
                                "[%s][%5u] Out of order packet (window low: %d, got: %d)\n",
                                ip_as_str,
                                ntohs(client->address->sin6_port),
                                client->window->window_low,
                                decoded->seqnum
                            );
                        } else if(is_used_nolock(window, decoded->seqnum)) {
                            ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                            pthread_mutex_unlock(buf_get_lock(window));

                            fprintf(
                                stderr, 
                                "[%s][%5u] Packet received twice (window low: %d, got: %d)\n",
                                ip_as_str,
                                ntohs(client->address->sin6_port),
                                client->window->window_low,
                                decoded->seqnum
                            );
                        } else {
                            node_t *spot = next_nolock(window, decoded->seqnum, true);
                            packet_t *temp = (packet_t *) spot->value;
                            packet_t *value = decoded;

                            spot->value = decoded;
                            decoded = temp;

                            unlock(spot);

                            int i = window->window_low;
                            int cnt = 0;
                            node_t *node = NULL;
                            packet_t *pak;
                            do {
                                node = get_nolock(window, i & 0xFF, false, false);
                                if (node != NULL) {
                                    pak = (packet_t *) node->value;

                                    if (pak->length > 0) {
                                        remove = false;
                                        int result = fwrite(
                                            pak->payload,
                                            sizeof(uint8_t),
                                            pak->length,
                                            client->out_file
                                        );

                                        if (result != pak->length) {
                                            fprintf(stderr, "[HD] Failed to write to file\n");
                                            break;
                                        }
                                    } else {
                                        remove = true;
                                    }

                                    cnt++;
                                    i++;
                                }
                                
                                unlock(node);
                            } while(sequences[client->window->window_low][i & 0xFF] && node != NULL && cnt < 31);

                            pthread_mutex_unlock(buf_get_lock(window));

                            
                            if (cnt > 0) {
                                //fflush(client->out_file);

                                //fprintf(stderr, "LEN: old: %3d, new: %3d, count: %2d\n", window->length, window->length - cnt, cnt);
                                //fprintf(stderr, "LOW: old: %3d, new: %3d, count: %2d\n", window->window_low, window->window_low + cnt, cnt);
                                window->length = (window->length - cnt);
                                window->window_low += cnt;

                                s_node_t *send_node = stream_pop(cfg->send_rx, false);
                                if (send_node == NULL) {
                                    send_node = malloc(sizeof(s_node_t));
                                    if (send_node == NULL || initialize_node(send_node, allocate_send_request)) {
                                        fprintf(stderr, "[HD] Failed to allocated s_node_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                            deallocate_node(node_rx);
                                        }

                                        continue;
                                    }

                                    send_node->content = NULL;
                                    send_node->next = NULL;
                                }

                                if (send_node->content == NULL) {
                                    send_node->content = allocate_send_request();
                                    if (send_node->content == NULL) {
                                        fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");

                                        if (!stream_enqueue(cfg->tx, node_rx, false)) {
                                            deallocate_node(node_rx);
                                        }

                                        continue;
                                    }
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
                                to_send.window = min(cfg->max_window_size, 31 - window->length);

                                if (pack(send_req->to_send, &to_send, false)) {
                                    ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                                    fprintf(stderr, "[%s][%5u] Failed to pack ACK\n", ip_as_str, client->address->sin6_port);

                                    /** 
                                     * We move the window back by one to let the
                                     * retransmission timer do its thing
                                     */
                                    client->window->window_low--;
                                } else {
                                    if (remove) {
                                        uint16_t port = client->address->sin6_port;

                                        ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                                        fprintf(stderr, "[%s][%5u] Done transfering file\n", ip_as_str, client->address->sin6_port);
                                        pthread_mutex_unlock(client->lock);

                                        /*ht_remove(cfg->clients, port, client->address->sin6_addr.__in6_u.__u6_addr8);

                                        fclose(client->out_file);

                                        free(client->addr_len);
                                        
                                        deallocate_buffer(client->window);
                                        free(client->window);

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
                        deallocate_node(node_rx);
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

