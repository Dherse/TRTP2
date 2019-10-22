#define _GNU_SOURCE

#include "../headers/main.h"

#define MAX_STREAM_LEN 2048*2048

bool global_stop;
pthread_mutex_t stop_mutex;
pthread_cond_t stop_cond;

volatile uint32_t idx = 0;

/**
 * Handles the SIGINT signal
 */
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

/**
 * Just read the name
 */
void print_usage(char *exec) {
    fprintf(stderr, "Multithreaded TRTP receiver.\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [options] <ip> <port>\n\n", exec);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -m  Max. number of connection   [default: 100]\n");
    fprintf(stderr, "  -o  Output file format          [default: %%d]\n");
    fprintf(stderr, "  -s  Enables sequential mode     [default: false]\n");
    fprintf(stderr, "  -N  Number of receiver threads  [default: 1]\n");
    fprintf(stderr, "  -n  Number of handler threads   [default: 2]\n");
    fprintf(stderr, "  -W  Maximum receive buffer      [default: %d]\n", MAX_WINDOW_SIZE);
    fprintf(stderr, "  -w  Maximum window size         [default: %d]\n\n", MAX_WINDOW_SIZE);
    fprintf(stderr, "Sequential:\n");
    fprintf(stderr, "  In sequential mode, only a single thread (the main thread) is used\n");
    fprintf(stderr, "  for the entire receiver. This means the parameters n & N will be\n");
    fprintf(stderr, "  ignored. Affinities are also ignored.\n");
    fprintf(stderr, "  Sequential mode comes with a huge performance penalty as the different\n");
    fprintf(stderr, "  components are ran sequentially while being designed for multithreaded use.\n\n");
    fprintf(stderr, "  /!\\ You can expect about half the speed using sequential mode.\n\n");
    fprintf(stderr, "Affinities:\n");
    fprintf(stderr, "  Affinities are set using a affinity.cfg file in the\n");
    fprintf(stderr, "  working directory. This file should be formatted like this:\n");
    fprintf(stderr, "\tcomma separated affinity for each receiver. Count must match N\n");
    fprintf(stderr, "\tcomma separated affinity for each handler. Count must match n\n");
    fprintf(stderr, "  Here is an example file: (remove the tabs)\n");
    fprintf(stderr, "\t0,1\n\t2,3,4,5\n");
    fprintf(stderr, "  It means the affinities of the receivers will be on CPU 0 & 1\n");
    fprintf(stderr, "  And the affinities of the handlers will be on CPU 2, 3, 4 & 5\n");
    fprintf(stderr, "  To learn more about affinity: https://en.wikipedia.org/wiki/Processor_affinity\n");
}

/**
 * Just read the name
 */
void deallocate_everything(
    config_rcv_t *config,
    int sockfd,
    stream_t *rx_to_hd,
    stream_t *hd_to_rx,
    ht_t *clients,
    rx_cfg_t **rx_configs,
    hd_cfg_t **hd_configs
) {
    fprintf(stderr, "[STOP] Deallocation called\n");
    int i ;
    for (i = 0; i < config->receive_num; i++) {
        if (rx_configs[i] != NULL) {
            if (rx_configs[i]->thread != NULL) {
                rx_configs[i]->stop = true;
                fprintf(stderr, "[STOP] Waiting for RX #%d\n", i);
                pthread_join(*rx_configs[i]->thread, NULL);
                free(rx_configs[i]->thread);
            }

            free(rx_configs[i]);
        }
    }
    free(rx_configs);

    for(i = 0; i < config->handle_num; i++) {
        s_node_t *stop_node = malloc(sizeof(s_node_t));

        if (stop_node != NULL && !initialize_node(stop_node, allocate_handle_request)) {
            hd_req_t *stop_req = (hd_req_t *) stop_node->content;
            stop_req->stop = true;
            
            stream_enqueue(rx_to_hd, stop_node, true);
        }
    }

    if (hd_configs != NULL) {
        for(i = 0; i < config->handle_num; i++) {
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

    if (config->receive_affinities != NULL) {
        free(config->receive_affinities);
    }

    if (config->handle_affinities != NULL) {
        free(config->handle_affinities);
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

    if (clients != NULL) {
        dealloc_ht(clients);
        free(clients);
    }

}

/*
 * Refer to headers/main.h
 */
int main(int argc, char *argv[]) {
    // -------------------------------------------------------------------------
    // Config parsing
    // -------------------------------------------------------------------------
    config_rcv_t config;
    memset(&config, 0, sizeof(config_rcv_t));

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

    parse_affinity_file(&config);

    if (config.sequential) {
        config.handle_num = 1;
        config.receive_num = 1;
    }

    print_config(&config);

    stream_t *rx_to_hd;
    stream_t *hd_to_rx;

    ht_t *clients;

    rx_cfg_t **rx_configs;
    hd_cfg_t **hd_configs;

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

    int size = 4000000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size))) {
        fprintf(stderr, "[MAIN] Failed to set send buffer size");

        close(sockfd);

        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size))) {
        fprintf(stderr, "[MAIN] Failed to set receive buffer size");

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
    if (rx_to_hd == NULL || allocate_stream(rx_to_hd)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'rx_to_hd'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx,
            clients, 
            rx_configs,
            hd_configs
        );

        return -1;
    }

    hd_to_rx = calloc(1, sizeof(stream_t));
    if (hd_to_rx == NULL || allocate_stream(hd_to_rx)) {
        fprintf(stderr, "[MAIN] Failed to initialize 'hd_to_rx'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx,
            clients, 
            rx_configs,
            hd_configs
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
            clients, 
            rx_configs,
            hd_configs
        );
        
        return -1;
    }

    rx_configs = calloc(config.receive_num, sizeof(rx_cfg_t *));
    if (rx_configs == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'rx_config'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx,
            clients, 
            rx_configs,
            hd_configs
        );
        
        return -1;
    }
    
    int i;
    for (i = 0; i < config.receive_num; i++) {
        rx_configs[i] = calloc(1, sizeof(rx_cfg_t));
        if (rx_configs[i] == NULL) {
            fprintf(stderr, "[MAIN] Failed to initialize 'rx_config'\n");
            
            deallocate_everything(
                &config,
                sockfd,
                rx_to_hd, 
                hd_to_rx,
                clients, 
                rx_configs,
                hd_configs
            );
            
            return -1;
        }
    }

    hd_configs = (hd_cfg_t **) calloc(config.handle_num, sizeof(hd_cfg_t *));
    if (hd_configs == NULL) {
        fprintf(stderr, "[MAIN] Failed to initialize 'hd_configs'\n");
        
        deallocate_everything(
            &config,
            sockfd,
            rx_to_hd, 
            hd_to_rx,
            clients, 
            rx_configs,
            hd_configs
        );
        
        return -1;
    }

    for (i = 0; i < config.handle_num; i++) {
        hd_configs[i] = calloc(1, sizeof(hd_cfg_t));
        if (hd_configs[i] == NULL) {
            fprintf(stderr, "[MAIN] Failed to initialize 'hd_configs[%d]'\n", i);
        
            deallocate_everything(
                &config,
                sockfd,
                rx_to_hd, 
                hd_to_rx, 
                clients, 
                rx_configs,
                hd_configs
            );
        
            return -1;
        }
    }

    // -------------------------------------------------------------------------
    // Thread configs initialization
    // -------------------------------------------------------------------------

    for (i = 0; i < config.receive_num; i++) {
        rx_configs[i]->id = i;
        rx_configs[i]->clients = clients;
        rx_configs[i]->file_format = config.format;
        rx_configs[i]->idx = &idx;
        rx_configs[i]->max_clients = config.max_connections;
        rx_configs[i]->sockfd = sockfd;
        rx_configs[i]->stop = false;
        rx_configs[i]->rx = hd_to_rx;
        rx_configs[i]->tx = rx_to_hd;
        rx_configs[i]->addr_len = &config.addr_info->ai_addrlen;
        rx_configs[i]->window_size = config.receive_window_size;
        rx_configs[i]->affinity = config.receive_affinities == NULL ? NULL : &config.receive_affinities[i];
    }

    for (i = 0; i < config.handle_num; i++) {
        hd_configs[i]->id = i;
        hd_configs[i]->sockfd = sockfd;
        hd_configs[i]->clients = clients;
        hd_configs[i]->rx = rx_to_hd;
        hd_configs[i]->tx = hd_to_rx;
        hd_configs[i]->max_window_size = config.max_window;
        hd_configs[i]->affinity = config.handle_affinities == NULL ? NULL : &config.handle_affinities[i];
    }

    // -------------------------------------------------------------------------
    // Thread initialization
    // -------------------------------------------------------------------------

    if (!config.sequential) {
        for (i = 0; i < config.receive_num; i++) {
            rx_configs[i]->thread = malloc(sizeof(pthread_t));
            if (rx_configs[i]->thread == NULL) {
                fprintf(stderr, "[MAIN] Failed to initialize 'rx_config[%d]->thread'\n", i);
                
                deallocate_everything(
                    &config,
                    sockfd,
                    rx_to_hd, 
                    hd_to_rx, 
                    clients, 
                    rx_configs,
                    hd_configs
                );
                
                return -1;
            }

            if (pthread_create(rx_configs[i]->thread, NULL, &receive_thread, (void *) rx_configs[i])) {
                fprintf(stderr, "[MAIN] Failed to start 'rx_configs[%d]->thread'\n", i);
                
                deallocate_everything(
                    &config,
                    sockfd,
                    rx_to_hd, 
                    hd_to_rx, 
                    clients, 
                    rx_configs,
                    hd_configs
                );
                
                return -1;
            }
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
                    clients, 
                    rx_configs,
                    hd_configs
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
                    clients, 
                    rx_configs,
                    hd_configs
                );
                
                return -1;
            }
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
            clients, 
            rx_configs,
            hd_configs
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
            clients, 
            rx_configs,
            hd_configs
        );
    }

    signal(SIGINT, handle_stop);

    if (config.sequential) {
        socklen_t addr_len = sizeof(struct sockaddr_in6);
        uint8_t buffers[config.receive_window_size][MAX_PACKET_SIZE];
        struct sockaddr_in6 addrs[config.receive_window_size];
        struct mmsghdr msgs[config.receive_window_size];
        struct iovec iovecs[config.receive_window_size];

        int i;
        for(i = 0; i < config.receive_window_size; i++) {
            memset(&addrs[i], 0, sizeof(struct sockaddr_in6));
            memset(&iovecs[i], 0, sizeof(struct iovec));
            memset(&msgs[i], 0, sizeof(struct mmsghdr));
            
            iovecs[i].iov_base = buffers[i];
            iovecs[i].iov_len = MAX_PACKET_SIZE;

            msgs[i].msg_hdr.msg_name = &addrs[i];
            msgs[i].msg_hdr.msg_namelen = addr_len;
            msgs[i].msg_hdr.msg_iov = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
            msgs[i].msg_hdr.msg_flags = 0;
            msgs[i].msg_hdr.msg_control = NULL;
        }

        bool exit = false;
        uint8_t file_buffer[MAX_PACKET_SIZE * MAX_WINDOW_SIZE];
        packet_t **decoded = malloc(sizeof(packet_t *));
        if (decoded == NULL) {
            fprintf(stderr, "[MAIN] Failed to start handle thread: alloc failed\n");
            return -1;
        }
        *decoded = allocate_packet();
        if (*decoded == NULL) {
            fprintf(stderr, "[MAIN] Failed to start handle thread: alloc failed\n");
            return -1;
        }

        uint8_t packets_to_send[MAX_WINDOW_SIZE][12];
        struct mmsghdr msg[MAX_WINDOW_SIZE];
        struct iovec hd_iovecs[MAX_WINDOW_SIZE];

        memset(hd_iovecs, 0, sizeof(iovecs));
        for(i = 0; i < MAX_WINDOW_SIZE; i++) {
            memset(&hd_iovecs[i], 0, sizeof(struct iovec));
            hd_iovecs[i].iov_base = packets_to_send[i];
            hd_iovecs[i].iov_len  = 11;

            memset(&msg[i], 0, sizeof(struct mmsghdr));
            msg[i].msg_hdr.msg_iov = &hd_iovecs[i];
            msg[i].msg_hdr.msg_iovlen = 1;
            msg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in6);
        }

        while (true) {
            pthread_mutex_lock(&stop_mutex);
            if (global_stop) {
                pthread_mutex_unlock(&stop_mutex);
                break;
            }
            pthread_mutex_unlock(&stop_mutex);

            rx_run_once(
                rx_configs[0],
                buffers,
                addr_len,
                addrs,
                msgs,
                iovecs
            );

            while(hd_configs[0]->rx->length != 0) {
                hd_run_once(
                    false,
                    hd_configs[0],
                    decoded,
                    &exit,
                    file_buffer,
                    packets_to_send,
                    msg,
                    hd_iovecs
                );
            }
        }

    } else {
        pthread_mutex_lock(&stop_mutex);
        while (!global_stop) {
            pthread_cond_wait(&stop_cond, &stop_mutex);
        }
        pthread_mutex_unlock(&stop_mutex);
    }
    
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
        clients, 
        rx_configs,
        hd_configs
    );

    return 0;
}