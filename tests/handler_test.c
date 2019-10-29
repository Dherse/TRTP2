#define _GNU_SOURCE

#include "./headers/handler_test.h"
#include "../headers/global.h"
#include "../headers/handler.h"

void test_global() {
    int whatever_that_is = 28;

    struct sockaddr_in6 address;
    address.sin6_addr.__in6_u.__u6_addr32[0] = 1;
    address.sin6_addr.__in6_u.__u6_addr32[0] = 0;
    address.sin6_addr.__in6_u.__u6_addr32[0] = 0;
    address.sin6_addr.__in6_u.__u6_addr32[0] = 0;
    address.sin6_family = AF_INET6;
    address.sin6_port = 5556;

    int send_sock = socket(AF_INET6, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    CU_ASSERT(send_sock > 0);
    CU_ASSERT( bind(send_sock, &address, whatever_that_is) == 0);

    uint8_t buf[528];
    packet_t received;
    CU_ASSERT(init_packet(&received) == 0);

    int sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    CU_ASSERT(sockfd > 0);

    stream_t rx_to_hd;
    CU_ASSERT(initialize_stream(&rx_to_hd) == 0);
    
    stream_t hd_to_rx;
    CU_ASSERT(initialize_stream(&hd_to_rx) == 0);
    bool wait = false;

    hd_cfg_t *cfg = (hd_cfg_t *) malloc(sizeof(hd_cfg_t));
    cfg->id = 0;
    cfg->thread = NULL;
    cfg->rx = &rx_to_hd;
    cfg->tx = &hd_to_rx;
    cfg->clients = NULL;
    cfg->affinity = NULL;
    cfg->max_window_size = 31;
    cfg->sockfd = sockfd;

    client_t client;
    CU_ASSERT(initialize_client(&client, 0, "./bin/%d", &address, &whatever_that_is) == 0);
    
    packet_t **decoded = alloca(sizeof(packet_t *));
    CU_ASSERT(decoded != NULL);
    *decoded = allocate_packet();
    CU_ASSERT(*decoded != NULL);

    bool exit = false;

    uint8_t file_buffer[528 * 31];


    uint8_t packets_to_send[31][12];
    struct mmsghdr msg[31];
    struct iovec hd_iovecs[31];

    memset(hd_iovecs, 0, (size_t) 31);
    int i;
    for(i = 0; i < 31; i++) {
        memset(&hd_iovecs[i], 0, sizeof(struct iovec));
        hd_iovecs[i].iov_base = packets_to_send[i];
        hd_iovecs[i].iov_len  = 11;

        memset(&msg[i], 0, sizeof(struct mmsghdr));
        msg[i].msg_hdr.msg_iov = &hd_iovecs[i];
        msg[i].msg_hdr.msg_iovlen = 1;
        msg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in6);
    }
    

    s_node_t *node1 = (s_node_t *)malloc(sizeof(s_node_t));
    CU_ASSERT(node1 != NULL);
    CU_ASSERT(initialize_node(node1, allocate_handle_request) == 0);

    s_node_t *node2 = (s_node_t *)malloc(sizeof(s_node_t));
    CU_ASSERT(node2 != NULL);
    CU_ASSERT(initialize_node(node2, allocate_handle_request) == 0);

    s_node_t *node3 = (s_node_t *)malloc(sizeof(s_node_t));
    CU_ASSERT(node3 != NULL);
    CU_ASSERT(initialize_node(node3, allocate_handle_request) == 0);

    s_node_t *node4 = (s_node_t *)malloc(sizeof(s_node_t));
    CU_ASSERT(node4 != NULL);
    CU_ASSERT(initialize_node(node4, allocate_handle_request) == 0);
    
    /**
     * init node1
     */
    hd_req_t *req1 = node1->content;
    packet_t pkt1;
    CU_ASSERT(init_packet(&pkt1) == 0);
    pkt1.type = DATA;
    pkt1.timestamp = 123456789;
    pkt1.length = 13;
    strcpy(pkt1.payload, "NOUGATERIGNO\n");
    pkt1.seqnum = 0;

    pack(req1->buffer[0], &pkt1, true);
    //packet_to_string(&pkt1, true);
    req1->client = &client;
    req1->lengths[0] = 11 + 13 + 4;
    req1->num = 1;
    req1->stop = false;

    /**
     * init node2
     */
    hd_req_t *req2 = node2->content;
    packet_t pkt2;
    CU_ASSERT(init_packet(&pkt2) == 0);
    pkt2.type = DATA;
    pkt2.timestamp = 69420;
    pkt2.length = 30;
    strcpy(pkt2.payload, "ZA WAAAARUUUUDOOOOOOOOO!!!!!!\n");
    pkt2.seqnum = 0;

    pack(req2->buffer[0], &pkt2, true);
    //packet_to_string(&pkt2, true);
    req2->client = &client;
    req2->lengths[0] = 11 + 30 + 4;
    req2->num = 1;
    req2->stop = false;

    /**
     * init node3
     */
    hd_req_t *req3 = node3->content;
    packet_t pkt3;
    CU_ASSERT(init_packet(&pkt3) == 0);
    pkt3.type = DATA;
    pkt3.timestamp = 99999;
    pkt3.length = 17;
    strcpy(pkt3.payload, "DIIIOOO DAAA!!!!\n");
    pkt3.seqnum = 1;

    pack(req3->buffer[0], &pkt3, true);
    //packet_to_string(&pkt3, true);
    req3->client = &client;
    req3->lengths[0] = 11 + 17 + 4;
    req3->num = 1;
    req3->stop = false;

    
    /**
     * init node4
     */
    hd_req_t *req4 = node4->content;
    packet_t pkt4;
    CU_ASSERT(init_packet(&pkt4) == 0);
    pkt4.type = DATA;
    pkt4.timestamp = 10101010;
    pkt4.length = 0;
    pkt4.seqnum = 2;

    pack(req4->buffer[0], &pkt4, true);
    //packet_to_string(&pkt4, true);
    req4->client = &client;
    req4->lengths[0] = 11;
    req4->num = 1;
    req4->stop = false;




    stream_enqueue(&rx_to_hd, node1, true);

    hd_run_once(wait, cfg, decoded, &exit, file_buffer, packets_to_send, msg, hd_iovecs);

    size_t nreceived = recv(send_sock, buf, sizeof(buf), 0);
    CU_ASSERT(nreceived > 0);
    unpack(buf, nreceived, &received);
    CU_ASSERT(received.type == ACK);
    CU_ASSERT(received.seqnum == 1);
    CU_ASSERT(received.timestamp == 123456789);
    CU_ASSERT(received.truncated == false);
    CU_ASSERT(received.length == 0);
    CU_ASSERT(received.window == 31);





    stream_enqueue(&rx_to_hd, node2, true);

    hd_run_once(wait, cfg, decoded, &exit, file_buffer, packets_to_send, msg, hd_iovecs);
    
    memset(buf, 0, 528);
    memset(&received, 0, sizeof(packet_t));
    nreceived = recv(send_sock, buf, sizeof(buf), 0);
    CU_ASSERT(nreceived > 0);
    unpack(buf, nreceived, &received);
    CU_ASSERT(received.type == ACK);
    CU_ASSERT(received.seqnum == 1);
    CU_ASSERT(received.timestamp == 69420);
    CU_ASSERT(received.truncated == false);
    CU_ASSERT(received.length == 0);
    CU_ASSERT(received.window == 31);





    stream_enqueue(&rx_to_hd, node3, true);

    hd_run_once(wait, cfg, decoded, &exit, file_buffer, packets_to_send, msg, hd_iovecs);
    
    memset(buf, 0, 528);
    memset(&received, 0, sizeof(packet_t));
    nreceived = recv(send_sock, buf, sizeof(buf), 0);
    CU_ASSERT(nreceived > 0);
    unpack(buf, nreceived, &received);
    CU_ASSERT(received.type == ACK);
    CU_ASSERT(received.seqnum == 2);
    CU_ASSERT(received.timestamp == 99999);
    CU_ASSERT(received.truncated == false);
    CU_ASSERT(received.length == 0);
    CU_ASSERT(received.window == 31);





    stream_enqueue(&rx_to_hd, node4, true);

    hd_run_once(wait, cfg, decoded, &exit, file_buffer, packets_to_send, msg, hd_iovecs);
    
    memset(buf, 0, 528);
    memset(&received, 0, sizeof(packet_t));
    nreceived = recv(send_sock, buf, sizeof(buf), 0);
    CU_ASSERT(nreceived > 0);
    unpack(buf, nreceived, &received);
    CU_ASSERT(received.type == ACK);
    CU_ASSERT(received.seqnum == 3);
    CU_ASSERT(received.timestamp == 10101010);
    CU_ASSERT(received.truncated == false);
    CU_ASSERT(received.length == 0);
    CU_ASSERT(received.window == 31);

    CU_ASSERT(client.active == false);

    FILE *fd = fopen("./bin/0", "rb");
    CU_ASSERT(fd > 0);
    char string[528];
    memset(string, 0, sizeof(string));
    fgets(string, sizeof(string), fd);
    CU_ASSERT(strcmp(string, "NOUGATERIGNO\n") == 0);
    fgets(string, sizeof(string), fd);
    CU_ASSERT(strcmp(string, "DIIIOOO DAAA!!!!\n") == 0);
    fclose(fd);


    
    close(send_sock);
    close(sockfd);
    
    pthread_mutex_destroy(client.lock);
    free(client.lock);
    free(client.address);
    deallocate_buffer(client.window);

    dealloc_stream(&rx_to_hd);
    dealloc_stream(&hd_to_rx);
    free(cfg);
    free(*decoded);

}

int add_global_tests() {
    CU_pSuite pSuite = CU_add_suite("buffer_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(pSuite, "test_global", test_global)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}