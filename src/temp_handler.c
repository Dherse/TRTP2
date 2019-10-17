#include "../headers/receiver.h"
#include "../headers/handler.h"
#include "../headers/global.h"

/*
 * Refer to headers/handler.h
 */
void *handle_thread(void *config) {
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

        //if failed to unpack, print error message, enqueue and continue
        if(unpack(req->buffer, req->length, decoded)) { 
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
            
            //if failed to enqueue, free data
            enqueue_or_free(cfg->tx,node_rx);
            continue;
        }
        remove = false;
        pthread_mutex_lock(client_get_lock(client));

        //we don't expect any ACK/NACK or IGNORE type
        if(decoded->type == DATA) {

            /** 
             * upon truncated DATA packet reception, send NACK
             * 
             * it is not necessary to check this field after the type 
             * as `unpack` already does it but, we do it to stay 
             * consistent with the protocol
             */
            //mettre les pop et allocation de request en dehors des if pour ne pas les multiplier
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
                node_t *spot = NULL;

                pthread_mutex_lock(buf_get_lock(window));
                if (!sequences[client->window->window_low][decoded->seqnum]) {
                    /**
                     * we store this value so to unlock the mutex
                     * before the `fprintf` and `ip_to_string` call, 
                     * because it is slow
                     */
                    uint8_t temp = client->window->window_low;
                    pthread_mutex_unlock(buf_get_lock(window));
                    ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                    fprintf(
                        stderr, 
                        "[%s][%5u] Out of order packet (window low: %d, got: %d)\n",
                        ip_as_str,
                        client->address->sin6_port,
                        temp,
                        decoded->seqnum
                    );
                    //TODO : si reception de packet hors sequence send last ACK
                } else if(is_used_nolock(window, decoded->seqnum)) {
                    /**
                     * we store this value so to unlock the mutex
                     * before the `fprintf` and `ip_to_string` call, 
                     * because it is slow
                     */
                    uint8_t temp = client->window->window_low;
                    pthread_mutex_unlock(buf_get_lock(window));
                    ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                    fprintf(
                        stderr, 
                        "[%s][%5u] Packet received twice (window low: %d, got: %d)\n",
                        ip_as_str,
                        client->address->sin6_port,
                        temp,
                        decoded->seqnum
                    );
                } else {
                    node_t *spot = next_nolock(window, decoded->seqnum, true);
                    uint32_t temp_timestamp = decoded->timestamp;

                    /**
                     * switch spot->value and decoded
                     */
                    packet_t *temp = (packet_t *) spot->value;
                    spot->value = decoded;
                    decoded = temp;

                    unlock(spot);
                    

                    /**
                     * printing packets already in buffer and to 
                     * send only one cumulative ack
                     */
                    // seqnum actually being checked
                    uint8_t i = window->window_low;

                    // number of packet successfully checked
                    uint8_t cnt = 0;

                    // node conatining `pak`
                    node_t* node = NULL;

                    // packet actually being checked
                    packet_t *pak;
                    do {
                        unlock(node);

                        node = get_nolock(window, i, false, false);
                        if (node == NULL) {
                            continue;
                        }
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
                                //TODO : peut être free un truc?
                                break;
                            }
                        }

                        cnt++;
                        i++;
                    } while(sequences[client->window->window_low][i] && node != NULL);

                    pthread_mutex_unlock(buf_get_lock(window));
                    unlock(node);

                    if (cnt > 0) {
                        /**
                         * forces data to actually be printed to the files
                         * if not called, the data to print may be stored
                         * in memmory
                         */
                        fflush(client->out_file);
                        
                        /**
                         * according to TRTP protocol, receiving a valid,
                         * in-sequence DATA packet with length = 0
                         * means the transfer should end
                         */
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
                            if(initialize_node(send_node, allocate_send_request)) {
                                free(send_node);
                                enqueue_or_free(cfg->tx, node_rx);
                                continue;
                            }
                        }

                        tx_req_t *send_req = (tx_req_t *) send_node->content;

                        send_req->address = client->address;

                        //remove or false ? -> pas le même dans handler.c
                        send_req->deallocate_address = remove;

                        to_send.type = ACK;
                        to_send.truncated = false;
                        to_send.long_length = false;
                        to_send.length = 0;
                        to_send.seqnum = window->window_low;
                        to_send.timestamp = temp_timestamp;
                        to_send.window = min(31 - window->length, cfg->max_window_size);

                        if (pack(send_req->to_send, &to_send, false)) {
                            fprintf(stderr, "[%s][%5u] Failed to pack ACK\n", ip_as_str, client->address->sin6_port);

                            /** 
                             * We move the window back by one to let the
                             * retransmission timer do its thing
                             */
                            client->window->window_low--;
                            continue;
                        } 
                        if (remove) {
                            uint16_t port = client->address->sin6_port;

                            ip_to_string(req->client->address->sin6_addr.__in6_u.__u6_addr8, ip_as_str);
                            fprintf(stderr, "[%s][%5u] Done transfering file\n", ip_as_str, client->address->sin6_port);
                            
                            ht_remove(cfg->clients, port, client->address->sin6_addr.__in6_u.__u6_addr8);

                            deallocate_client(client, false, true);

                            fprintf(stderr, "[%s][%5u] Destroyed\n", ip_as_str, port);
                        }

                        stream_enqueue(cfg->send_tx, send_node, true);
                    }
                }
            }
        }
        
        enqueue_or_free(cfg->tx, node_rx);
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

/*
 * Refer to headers/receiver.h
 */
void *allocate_handle_request() {
    hd_req_t *req = (hd_req_t *) malloc(sizeof(hd_req_t));
    if(req == NULL) {
        errno = FAILED_TO_ALLOCATE;
        return NULL;
    }
    
    req->stop = false;
    req->client = NULL;
    req->length = 0;
    memset(req->buffer, 0, 528);

    return req;
}