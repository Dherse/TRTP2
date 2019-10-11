#include "../headers/receiver.h"

void *send_thread(void *sender_config){
    if(sender_config == NULL) {
        pthread_exit(NULL_ARGUMENT);
    }
    tx_cfg_t *snd_cfg = (tx_cfg_t *) sender_config;

    socklen_t addr_len = sizeof(struct sockaddr_in6);
    uint8_t buf[12];
    size_t buf_size = sizeof(buf);
    s_node_t *node;
    tx_req_t *req;

    bool stop = false;

    while(!stop){
        node = stream_pop(snd_cfg->send_rx, true);
        req = (tx_req_t *) node->content;
        if(req == NULL) {
            free(node);
            fprintf(stderr, "[TX] `content` is NULL");
            break;
        }

        switch(req->stop){
            case true : 
                stop = true;
                free(req);
                free(node);
                break;

            default :
                if(pack(buf, &req->to_send, false) == -1){
                    fprintf(stderr, "[TX] packing failed");
                }

                ssize_t n_sent = sendto(snd_cfg->sockfd, buf, buf_size, 0, (struct sockaddr *) req->address, addr_len);
                if(n_sent == -1){
                    fprintf(stderr, "[TX] sendto failed");
                }

                if(!stream_enqueue(snd_cfg->send_tx, node, false)){
                    free(req);
                    free(node);
                }
        }
    }

    pthread_exit(0);
}