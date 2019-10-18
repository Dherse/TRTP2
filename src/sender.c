#define _GNU_SOURCE
#include "../headers/receiver.h"

void *send_thread(void *sender_config){
    if(sender_config == NULL) {
        pthread_exit(NULL_ARGUMENT);
    }

    pthread_t thread = pthread_self();

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    int aff = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (aff == -1) {
        fprintf(stderr, "[TX] Failed to set affinity\n");
    }

    
    tx_cfg_t *snd_cfg = (tx_cfg_t *) sender_config;

    socklen_t addr_len = sizeof(struct sockaddr_in6);
    s_node_t *node;
    tx_req_t *req;

    while (true) {
        req = NULL;
        node = stream_pop(snd_cfg->send_rx, true);
        if (node == NULL) {
            continue;
        }

        req = (tx_req_t *) node->content;
        if (req == NULL) {
            fprintf(stderr, "[TX] `content` is NULL\n");
            deallocate_node(node);
            
            continue;
        }

        if (req->stop) {
            deallocate_node(node);
            break;
        } else {
            ssize_t n_sent = sendto(snd_cfg->sockfd, req->to_send, 11, 0, (struct sockaddr *) req->address, addr_len);
            if (n_sent == -1) {
                fprintf(stderr, "[TX] sendto failed: ");
                switch (errno) {
                    case ENETDOWN:
                        fprintf(stderr, "closed\n");
                        break;
                    case ENETUNREACH:
                        fprintf(stderr, "unreach\n");
                        break;
                    case ENOTCONN:
                        fprintf(stderr, "not connected\n");
                        break;
                    case ENOTSOCK:
                        fprintf(stderr, "invalid socket descriptor\n");
                        break;
                    case ESHUTDOWN:
                        fprintf(stderr, "shutdown\n");
                        break;
                    case ETIMEDOUT:
                        fprintf(stderr, "timed out\n");
                        break;
                    case EWOULDBLOCK:
                        fprintf(stderr, "would block\n");
                        break;
                    default:
                        fprintf(stderr, "other: %d\n", errno);
                        break;
                }
            }

            if (req->deallocate_address) {
                free(req->address);
            }

            if(!stream_enqueue(snd_cfg->send_tx, node, false)){
                deallocate_node(node);
            }
        }
    }

    fprintf(stderr, "[TX] Stopped\n");

    pthread_exit(0);

    return NULL;
}