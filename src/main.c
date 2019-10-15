#include "../headers/main.h"

#define MAX_STREAM_LEN 2048*2048

bool global_stop;
pthread_mutex_t stop_mutex;
pthread_cond_t stop_cond;

void handle_stop(int signo) {
    fprintf(stderr, "[MAIN] Received SIGINT, stopping\n");

    if (global_stop) {
        exit(-1);
    }

    pthread_mutex_lock(&stop_mutex);
    global_stop = true;

    pthread_cond_broadcast(&stop_cond);
    
    pthread_mutex_unlock(&stop_mutex);
}

void print_usage(char *exec) {
    fprintf(stderr, "Multithreaded TRTP receiver.\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <ip> <port>\n\n", exec);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -m  Max. number of connection  [default: 100]\n");
    fprintf(stderr, "  -o  Output file format         [default: %%d]\n");
    fprintf(stderr, "  -n  Number of handler threads  [default: 4]\n");
}

void deallocate_everything(
    config_rcv_t *config,
    int sockfd,
    stream_t *rx_to_hd,
    stream_t *hd_to_rx,
    stream_t *hd_to_tx,
    stream_t *tx_to_hd,
    ht_t *clients,
    rx_cfg_t *rx_config,
    int hd_count,
    hd_cfg_t **hd_configs,
    tx_cfg_t *tx_config
) {
    drain(rx_to_hd);
    drain(hd_to_rx);
    drain(hd_to_tx);
    drain(tx_to_hd);

    fprintf(stderr, "[STOP] Deallocation called\n");
    if (rx_config != NULL) {
        if (rx_config->thread != NULL) {
            rx_config->stop = true;
            fprintf(stderr, "[STOP] Waiting for RX\n");
            pthread_join(*rx_config->thread, NULL);
            free(rx_config->thread);
        }

        free(rx_config);
    }

    if (tx_config != NULL) {
        if (tx_config->thread != NULL) {
            if (tx_config->send_rx != NULL) {
                tx_req_t *stop_req = malloc(sizeof(tx_req_t));

                s_node_t *stop_node = malloc(sizeof(s_node_t));

                if (stop_req != NULL && stop_node != NULL) {
                    stop_req->deallocate_address = false;
                    stop_req->address = NULL;
                    stop_req->stop = true;
                    memset(stop_req->to_send, 0, 11);

                    stop_node->content = stop_req;
                    stop_node->next = NULL;
                    stream_enqueue(tx_config->send_rx, stop_node, true);
                }
            }
            
            fprintf(stderr, "[STOP] Waiting for TX\n");
            pthread_join(*tx_config->thread, NULL);
            free(tx_config->thread);
        }

        free(tx_config);
    }

    int i;
    for(i = 0; i < hd_count; i++) {
        s_node_t *node = calloc(1, sizeof(s_node_t));
        hd_req_t *stop_req = calloc(1, sizeof(hd_req_t));
        if (node != NULL && stop_req != NULL) {
            stop_req->stop = true;
            node->content = stop_req;

            stream_enqueue(rx_to_hd, node, true);
        }
    }

    if (hd_configs != NULL) {
        for(i = 0; i < hd_count; i++) {
            if (hd_configs[i] != NULL) {
                if (hd_configs[i]->thread != NULL) {
                    fprintf(stderr, "[STOP] Waiting for HD #%d\n", i);
                    pthread_join(*hd_configs[i]->thread, NULL);
                    free(hd_configs[i]->thread);
                }
                free(hd_configs[i]);
            }
        }
        free(hd_configs);
    }

    if (config->addr_info != NULL) {
        freeaddrinfo(config->addr_info);
    }

    if (sockfd != -1) {
        close(sockfd);
    }

    if (rx_to_hd != NULL) {
        dealloc_stream(rx_to_hd);
        free(rx_to_hd);
    }

    if (hd_to_rx != NULL) {
        dealloc_stream(hd_to_rx);
        free(hd_to_rx);
    }

    if (hd_to_tx != NULL) {
        dealloc_stream(hd_to_tx);
        free(hd_to_tx);
    }

    if (tx_to_hd != NULL) {
        dealloc_stream(tx_to_hd);
        free(tx_to_hd);
    }

    if (clients != NULL) {
        dealloc_ht(clients);
        free(clients);
    }

}

int main(int argc, char *argv[]) {
    // -------------------------------------------------------------------------
    // Config parsing
    // -------------------------------------------------------------------------
    config_rcv_t config;
    int parse = parse_receiver(argc, argv, &config);
    if (parse != 0) {
        switch(errno) {
            case CLI_O_VALUE_MISSING:
                fprintf(stderr, "[MAIN] Argument value missing\n");
                print_usage(argv[0]);
                break;
            case CLI_UNKNOWN_OPTION:
                fprintf(stderr, "[MAIN] Unknown option\n");
                print_usage(argv[0]);
                break;
            case CLI_IP_MISSING:
                fprintf(stderr, "[MAIN] IP mask missing\n");
                print_usage(argv[0]);
                break;
            case CLI_PORT_MISSING:
                fprintf(stderr, "[MAIN] Port missing\n");
                print_usage(argv[0]);
                break;
            case CLI_TOO_MANY_ARGUMENTS:
                fprintf(stderr, "[MAIN] Too many arguments\n");
                print_usage(argv[0]);
                break;
            case CLI_FORMAT_VALIDATION_FAILED:
                fprintf(stderr, "[MAIN] File format is incorrect\n");
                print_usage(argv[0]);
                break;
            case CLI_MAX_INVALID:
                fprintf(stderr, "[MAIN] Invalid maximum number of connections\n");
                print_usage(argv[0]);
                break;
            case CLI_PORT_INVALID:
                fprintf(stderr, "[MAIN] Invalid port number\n");
                print_usage(argv[0]);
                break;
            case CLI_HANDLE_INVALID:
                fprintf(stderr, "[MAIN] Invalid handler thread count\n");
                print_usage(argv[0]);
                break;
            case CLI_IP_INVALID:
                fprintf(stderr, "[MAIN] Invalid IP mask\n");
                print_usage(argv[0]);
                break;
            default:
                fprintf(stderr, "[MAIN] Internal error (errno: %d)\n", errno);
                break;
        }

        return -1;
    }

    stream_t *rx_to_hd;
    stream_t *hd_to_rx;

    stream_t *hd_to_tx;
    stream_t *tx_to_hd;

    ht_t *clients;

    rx_cfg_t *rx_config;
    hd_cfg_t **hd_configs;
    tx_cfg_t *tx_config;

    int sockfd;

    // -------------------------------------------------------------------------
    // Socket initialization
    // -------------------------------------------------------------------------

    sockfd = socket(
        config.addr_info->ai_family, 
        config.addr_info->ai_socktype, 
        config.addr_info->ai_protocol
    );

    if (sockfd == -1) {
        fprintf(stderr, "[MAIN] Failed to create socket\n");

        return -1;
    }

    fprintf(stderr, "[MAIN] Socket opened (fd: %d)\n", sockfd);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
        fprintf(stderr, "[MAIN] Failed to set receive timeout");

        close(sockfd);

        return -1;
    }

    int reuseaddr = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))) {
        fprintf(stderr, "[MAIN] Failed to set reuse addr");

        close(sockfd);

        return -1;
    }

    int status = bind(sockfd, config.addr_info->ai_addr, config.addr_info->ai_addrlen);
    if (status) {
        fprintf(stderr, "[MAIN] Failed to bind socket");

        close(sockfd);

        return -1;
    }

    // -------------------------------------------------------------------------
    // Data structure allocations
    // -------------------------------------------------------------------------
    rx_to_hd = calloc(1, sizeof(stream_t));
    if (rx_to_hd == NULL || allocate_stream(rx_to_hd, MAX_STREAM_LEN)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'rx_to_hd'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );

        return -1;
    }

    hd_to_rx = calloc(1, sizeof(stream_t));
    if (hd_to_rx == NULL || allocate_stream(hd_to_rx, MAX_STREAM_LEN)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'hd_to_rx'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );

        return -1;
    }

    hd_to_tx = calloc(1, sizeof(stream_t));
    if (hd_to_tx == NULL || allocate_stream(hd_to_tx, MAX_STREAM_LEN)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'hd_to_tx'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );

        return -1;
    }

    tx_to_hd = calloc(1, sizeof(stream_t));
    if (tx_to_hd == NULL || allocate_stream(tx_to_hd, MAX_STREAM_LEN)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'tx_to_hd'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    clients = calloc(1, sizeof(ht_t));
    if (clients == NULL || allocate_ht(clients)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'clients'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    rx_config = calloc(1, sizeof(rx_cfg_t));
    if (rx_config == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'rx_config'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    hd_configs = (hd_cfg_t **) calloc(config.handle_num, sizeof(hd_cfg_t *));
    if (hd_configs == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'hd_configs'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    int i = 0;
    for (; i < config.handle_num; i++) {
        hd_configs[i] = calloc(1, sizeof(hd_cfg_t));
        if (hd_configs[i] == NULL) {
            fprintf(stderr, "[MAIN] Failed to initialize 'hd_configs[%d]'\n", i);
        
            deallocate_everything(
                &config,
                sockfd,
                rx_to_hd, 
                hd_to_rx, 
                hd_to_tx, 
                tx_to_hd, 
                clients, 
                rx_config,
                config.handle_num,
                hd_configs, 
                tx_config
            );
        
            return -1;
        }
    }

    tx_config = calloc(1, sizeof(tx_cfg_t));
    if (tx_config == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'tx_config'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    // -------------------------------------------------------------------------
    // Thread configs initialization
    // -------------------------------------------------------------------------

    rx_config->clients = clients;
    rx_config->file_format = config.format;
    rx_config->idx = 0;
    rx_config->max_clients = config.max_connections;
    rx_config->sockfd = sockfd;
    rx_config->stop = false;
    rx_config->rx = hd_to_rx;
    rx_config->tx = rx_to_hd;
    rx_config->addr_len = &config.addr_info->ai_addrlen;

    tx_config->send_rx = hd_to_tx;
    tx_config->send_tx = tx_to_hd;
    tx_config->sockfd = sockfd;

    for (i = 0; i < config.handle_num; i++) {
        hd_configs[i]->clients = clients;
        hd_configs[i]->rx = rx_to_hd;
        hd_configs[i]->tx = hd_to_rx;
        hd_configs[i]->send_tx = hd_to_tx;
        hd_configs[i]->send_rx = tx_to_hd;
    }

    // -------------------------------------------------------------------------
    // Thread initialization
    // -------------------------------------------------------------------------

    rx_config->thread = malloc(sizeof(pthread_t));
    if (rx_config->thread == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'rx_config->thread'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    if (pthread_create(rx_config->thread, NULL, &receive_thread, (void *) rx_config)) {
        fprintf(stderr, "[MAIN] Failed to start 'rx_config->thread'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    tx_config->thread = malloc(sizeof(pthread_t));
    if (tx_config->thread == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'tx_config->thread'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    if (pthread_create(tx_config->thread, NULL, &send_thread, (void *) tx_config)) {
        fprintf(stderr, "[MAIN] Failed to start 'tx_config->thread'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
        
        return -1;
    }

    for (i = 0; i < config.handle_num; i++) {
        hd_configs[i]->thread = calloc(1, sizeof(hd_cfg_t));
        if (hd_configs[i]->thread == NULL) {
            fprintf(stderr, "[MAIN] Failed to initialize 'hd_configs[%d]->thread'\n", i);
        
            deallocate_everything(
                &config,
                sockfd,
                rx_to_hd, 
                hd_to_rx, 
                hd_to_tx, 
                tx_to_hd, 
                clients, 
                rx_config,
                config.handle_num,
                hd_configs, 
                tx_config
            );
        
            return -1;
        }

        if (pthread_create(hd_configs[i]->thread, NULL, &handle_thread, (void *) hd_configs[i])) {
            fprintf(stderr, "[MAIN] Failed to start 'hd_configs[%d]->thread'\n", i);
            
            deallocate_everything(
                &config,
                sockfd,
                rx_to_hd, 
                hd_to_rx, 
                hd_to_tx, 
                tx_to_hd, 
                clients, 
                rx_config,
                config.handle_num,
                hd_configs, 
                tx_config
            );
            
            return -1;
        }
    }

    // -------------------------------------------------------------------------
    // Stop signal & mutex init
    // -------------------------------------------------------------------------

    global_stop = false;

    if (pthread_mutex_init(&stop_mutex, NULL)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'stop_mutex'");

        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
    }

    if (pthread_cond_init(&stop_cond, NULL)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'stop_mutex'");

        pthread_mutex_destroy(&stop_mutex);

        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx, 
            hd_to_tx, 
            tx_to_hd, 
            clients, 
            rx_config,
            config.handle_num,
            hd_configs, 
            tx_config
        );
    }

    signal(SIGINT, handle_stop);

    pthread_mutex_lock(&stop_mutex);
    
    while (!global_stop) {
        pthread_cond_wait(&stop_cond, &stop_mutex);
    }

    pthread_mutex_unlock(&stop_mutex);


    // -------------------------------------------------------------------------
    // Final deallocation
    // -------------------------------------------------------------------------

    pthread_mutex_destroy(&stop_mutex);
    pthread_cond_destroy(&stop_cond);
    
    deallocate_everything(
        &config,
        sockfd,
        rx_to_hd, 
        hd_to_rx, 
        hd_to_tx, 
        tx_to_hd, 
        clients, 
        rx_config,
        config.handle_num,
        hd_configs, 
        tx_config
    );

    return 0;
}