#include "../headers/receiver.h"

/**
 * 
typedef struct handle_thread_config {
    /** Thread reference 
    pthread_t *thread;

    /** Receive to Handle stream 
    stream_t *rx;
    /** Handle to Receive stream 
    stream_t *tx;

    /** Handle to Send stream 
    stream_t *send_tx;
    /** Send to Handle stream 
    stream_t *send_rx;

    /** Hash table of client 
    ht_t *clients;
} hd_cfg_t;
 */

void *handle_thread(void *config) {
    hd_cfg_t *cfg = (hd_cfg_t *) config;

    packet_t decoded;
    bool exit = false;

    while(!exit) {
        s_node_t *node = stream_pop(cfg->rx, true);
        if (node != NULL) {
            hd_req_t *req = NULL;
        }
    }
    

    return NULL;
}