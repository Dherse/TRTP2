#include <CUnit/CUnit.h>

void receive_body(hd_req_t *req, bool *already_popped, struct sockaddr_in6 *sockaddr_, rx_cfg_t *rcv_cfg, socklen_t addr_len, s_node_t *node, bool *failed_recvfrom) {
    char ip_as_str[40];
    struct sockaddr_in6 sockaddr = *sockaddr_;
    if(req->length == -1) {
        // pas de paquet reçus/echec de la reception
        switch(errno) {
            /**
             * case 1 :
             * recvfrom would have blocked and waited for a message
             * but didn't because of the flag
             * -> loop again
             */
            case EWOULDBLOCK : 
                *already_popped = true;
                *failed_recvfrom = false;
                return;
            /**
             * case 2 :
             * a message was 'received', but it failed
             * print error
             */
            default :
                *already_popped = true;
                *failed_recvfrom = true;
        }
    } else {
        /** set the last handle_request parameters */
        req->port = (uint16_t) sockaddr.sin6_port;
        move_ip(req->ip, sockaddr.sin6_addr.__in6_u.__u6_addr8);

        /** check if sockaddr is already known in the hash-table */
        if(!ht_contains(rcv_cfg->clients, req->port, req->ip)) {
            ip_to_string(req->ip, ip_as_str);

            /** add new client in `clients` */
            client_t *new_client = (client_t *) malloc(sizeof(client_t));
            if(new_client == NULL){
                return;
            } 
            if(allocate_client(new_client) == -1) {
                return;
            }
            *(new_client->address) = sockaddr;
            *(new_client->addr_len) = addr_len;
            ht_put(rcv_cfg->clients, req->port, req->ip, (void *) new_client);
        }

        /** send handle_request */
        stream_enqueue(rcv_cfg->tx, node, true);
        already_popped = 0;
    }
}

void test_recvfrom(){
    //stream in/out   *
    stream_t *rx, *tx;
    rx = calloc(1, sizeof(stream_t));
    CU_ASSERT(rx != NULL);
    if (rx == NULL) { return; }

    tx = calloc(1, sizeof(stream_t));
    CU_ASSERT(tx != NULL);
    if (tx == NULL) { return; }
    uint8_t ip[16] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    
    allocate_stream(rx, 2);
    allocate_stream(tx, 2);
    


    //window   *
    buf_t *window = calloc(1, sizeof(buf_t));
    CU_ASSERT(window != NULL);
    if (window == NULL) { return; }

    allocate_buffer(window, 32);




    //client   *
    client_t *client = calloc(1, sizeof(client_t));
    CU_ASSERT(client != NULL);
    if (client == NULL) { return; }

    client->file_mutex = calloc(1, sizeof(pthread_mutex_t));
    CU_ASSERT(client->file_mutex != NULL);
    if (client->file_mutex == NULL) { return; }

    pthread_mutex_init(client->file_mutex, NULL);

    /** stdout */
    client->out_fd = 1;
    client->address = NULL;
    client->addr_len = NULL;
    client->window = window;


    //hash-table   *
    ht_t *clients = calloc(1, sizeof(ht_t));
    CU_ASSERT(clients != NULL);
    if (clients == NULL) { return; }

    allocate_ht(clients);
    //ht_put(clients, 1000, ip, client);
    


    //config  *
    rx_cfg_t *cfg = calloc(1, sizeof(rx_cfg_t));
    CU_ASSERT(cfg != NULL);
    if (cfg == NULL) { return; }

    cfg->thread = NULL;
    cfg->stop = false;
    cfg->tx = tx;
    cfg->rx = rx;
    cfg->clients = clients;
    cfg->sockfd = 2;


    /** Create packet to received *  */
    packet_t *packet = calloc(1, sizeof(packet_t));
    CU_ASSERT(packet != NULL);
    if (packet == NULL) { return; }
    
    char *hello = "Hello, world!";
    memcpy(packet->payload, hello, strlen(hello));
    packet->length = strlen(hello) + 1;
    packet->long_length = false;
    packet->seqnum = 0;
    packet->window = 20;
    packet->timestamp = 0;
    packet->truncated = false;
    packet->type = DATA;


    /** Fill rx with some data  * */ 
    hd_req_t *hd_req = calloc(1, sizeof(hd_req_t));
    CU_ASSERT(hd_req != NULL);
    if (hd_req == NULL) { return; }

    hd_req->stop = false;
    pack(hd_req->buffer, packet, true);
    hd_req->length = -1;


    //s_node_t   *
    s_node_t *req = calloc(1, sizeof(s_node_t));
    CU_ASSERT(req != NULL);
    if (req == NULL) { return; }

    req->content = hd_req;
    stream_enqueue(rx, req, false);

    bool already_popped = false;
    struct sockaddr_in6 sockaddr = { .sin6_family = AF_INET6, .sin6_port = 1000};
    for(int i = 0; i < 16; i++){
        sockaddr.sin6_addr.__in6_u.__u6_addr8[i] = ip[i];
    }
    socklen_t addr_len = sizeof(sockaddr);
    bool failed_recvfrom = true;

    errno = EWOULDBLOCK;
    receive_body(hd_req, &already_popped, &sockaddr, cfg, addr_len, req, &failed_recvfrom);
    CU_ASSERT(already_popped == true);
    CU_ASSERT(failed_recvfrom == false);


    failed_recvfrom = false;
    already_popped = false;
    errno = 0;
    receive_body(hd_req, &already_popped, &sockaddr, cfg, addr_len, req, &failed_recvfrom);
    CU_ASSERT(already_popped == true);
    CU_ASSERT(failed_recvfrom == true);

    hd_req->length = 29;
    failed_recvfrom = false;
    already_popped = false;
    CU_ASSERT(ht_length(clients) == false);
    receive_body(hd_req, &already_popped, &sockaddr, cfg, addr_len, req, &failed_recvfrom);
    CU_ASSERT(hd_req->port == sockaddr.sin6_port);
    CU_ASSERT(hd_req->length == 29);
    for(int i = 0; i < 16; i++){
        CU_ASSERT(hd_req->ip[i] == sockaddr.sin6_addr.__in6_u.__u6_addr8[i]);
    }
    CU_ASSERT(hd_req->stop == false);
    
    CU_ASSERT((hd_req_t *) stream_pop(cfg->tx, false)->content == hd_req);
    CU_ASSERT(ht_length(clients) == 1);
    CU_ASSERT(ht_contains(clients, 1000, ip) == true);

    dealloc_stream(rx);    
    free(rx);

    dealloc_stream(tx);   
    free(tx);

    deallocate_buffer(window);
    free(window);

    pthread_mutex_destroy(client->file_mutex);
    free(client->file_mutex);
    free(client);

    dealloc_ht(clients);
    free(clients);

    free(cfg);

    free(packet);
    
    free(hd_req);

    free(req);
}


int add_receiver_tests() {
    CU_pSuite pSuite = CU_add_suite("buffer_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(pSuite, "test_recvfrom", test_recvfrom)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    

    return 0;
}