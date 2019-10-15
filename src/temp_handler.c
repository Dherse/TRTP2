#include "../headers/receiver.h"
#include "../headers/handler.h"
#include "../headers/global.h"

/*
 * Refer to headers/handler.h
 */
void *handle_thread_temp(void *config) {
    if(config == NULL) {
        pthread_exit(NULL);
    }
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t to_send;
    packet_t *decoded = (packet_t *) allocate_packet();
    if (decoded == NULL) {
        fprintf(stderr, "[HD] Failed to start handle thread: alloc failed\n");
        pthread_exit(NULL);
    }

    /** string to print error messages */ 
    char ip_as_str[40];

    //should the client be removed from the hash-table
    bool remove = false;
    while(true) {
        //wait for a request and check if NULL
        s_node_t *node_rx = stream_pop(cfg->rx, true);
        if (node_rx == NULL) {
            continue;
        }

        hd_req_t *req = (hd_req_t *) node_rx->content;
        if(req == NULL) {
            //TODO
        }
        
        //check if the thread should stop
        if (req->stop) {
            free(req);
            free(node_rx);
            break;
        }
        
        ssize_t length = req->length;
        client_t *client = req->client;
        if(client == NULL) {
            fprintf(stderr, "Unknown client\n");
            enqueue_or_free(cfg->tx,node_rx);
            continue;
        }

        // set ip_as_str for error messages
        ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);

        //if failed to unpack, print error message, enqueue and continue
        if(unpack(req->buffer, req->length, decoded)) {
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
            
            //if failed to enqueue, free data
            enqueue_or_free(cfg->tx,node_rx);
            continue;
        }
        remove = false;
        pthread_mutex_lock(client->lock);

        //we don't expect any ACK/NACK or IGNORE type
        if(decoded->type == DATA) {

            // upon truncated DATA packet reception, send NACK
            /** 
             * it is not necessary to check this field after the type 
             * as `unpack` already does it but, we do it to stay 
             * consistent with the protocol
             */
            if(decoded->truncated) {
                fprintf(
                    stderr, 
                    "[%s][%5u] Packet truncated: %02X\n", 
                    ip_as_str,
                    client->address->sin6_port,
                    decoded->seqnum
                );
                
                s_node_t *send_node = stream_pop(cfg->send_rx, false);

                //if there is no buffer available, allocate a new one
                if (send_node == NULL) {
                    send_node = malloc(sizeof(s_node_t));
                    if(initialize_node(send_node, allocate_send_request)) {
                        free(send_node);
                        enqueue_or_free(cfg->tx,node_rx);
                    }
                }

                tx_req_t *send_req = (tx_req_t *) send_node->content;
                //TODO : check if send_req == NULL

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
                    //TODO : reuse buffer instead of free
                    free(send_req);
                    free(send_node);
                } else {
                    stream_enqueue(cfg->send_tx, send_node, true);
                }
            } else {
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
                                enqueue_or_free(cfg->tx,node_rx);

                                continue;
                            }

                            send_node->content = NULL;
                            send_node->next = NULL;
                        }

                        if (send_node->content == NULL) {
                            tx_req_t* content = malloc(sizeof(tx_req_t));
                            if (content == NULL) {
                                fprintf(stderr, "[HD] Failed to allocated tx_req_t\n");
                                enqueue_or_free(cfg->tx,node_rx);

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
                        send_req->deallocate_address = remove;

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

                                ht_remove(cfg->clients, port, client->address->sin6_addr.__in6_u.__u6_addr8);

                                fclose(client->out_file);

                                free(client->addr_len);
                                
                                deallocate_buffer(client->window);
                                free(client->window);

                                pthread_mutex_unlock(client->lock);
                                pthread_mutex_destroy(client->lock);
                                free(client->lock);

                                free(client);

                                fprintf(stderr, "[%s][%5u] Destroyed\n", ip_as_str, port);
                            }

                            stream_enqueue(cfg->send_tx, send_node, true);
                        }
                    }
                }
            }
        }
        
        enqueue_or_free(cfg->tx,node_rx);
        if (!remove) {
            pthread_mutex_unlock(client->lock);
        }
    }
}

/*
 * Refer to headers/handler.h
 */
inline void enqueue_or_free(stream_t *stream, s_node_t *node) {
    if (!stream_enqueue(stream, node, false)) {
        //TODO : replace with getter
        free(node->content);
        free(node);
    }
}