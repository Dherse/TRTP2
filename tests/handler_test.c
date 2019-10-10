#include <CUnit/CUnit.h>

#include "../headers/receiver.h"

void test_handler_data() {
    /** Initialize all the required config and data structs */
    uint8_t ip[16] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    stream_t *rx, *tx, *send_tx, *send_rx;
    rx = calloc(1, sizeof(stream_t));
    CU_ASSERT(rx != NULL);
    if (rx == NULL) { return; }

    tx = calloc(1, sizeof(stream_t));
    CU_ASSERT(tx != NULL);
    if (tx == NULL) { return; }
    
    send_tx = calloc(1, sizeof(stream_t));
    CU_ASSERT(send_tx != NULL);
    if (send_tx == NULL) { return; }
    
    send_rx = calloc(1, sizeof(stream_t));
    CU_ASSERT(send_rx != NULL);
    if (send_rx == NULL) { return; }

    allocate_stream(rx, 2);
    allocate_stream(tx, 2);
    allocate_stream(send_tx, 2);
    allocate_stream(send_rx, 2);

    buf_t *window = calloc(1, sizeof(buf_t));
    CU_ASSERT(window != NULL);
    if (window == NULL) { return; }

    allocate_buffer(window, 32);

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

    ht_t *clients = calloc(1, sizeof(ht_t));
    CU_ASSERT(clients != NULL);
    if (clients == NULL) { return; }

    allocate_ht(clients);
    ht_put(clients, 1000, ip, client);

    hd_cfg_t *cfg = calloc(1, sizeof(hd_cfg_t));
    CU_ASSERT(cfg != NULL);
    if (cfg == NULL) { return; }

    cfg->thread = NULL;
    cfg->rx = rx;
    cfg->tx = tx;
    cfg->send_tx = send_tx;
    cfg->send_rx = send_rx;
    cfg->clients = clients;

    /** Create packet to received */
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

    /** Fill rx with some data */
    hd_req_t *hd_req = calloc(1, sizeof(hd_req_t));
    CU_ASSERT(hd_req != NULL);
    if (hd_req == NULL) { return; }

    hd_req->stop = false;
    memcpy(hd_req->ip, ip, 16);
    hd_req->port = 1000;
    pack(hd_req->buffer, packet, true);
    hd_req->length = 29;


    s_node_t *req = calloc(1, sizeof(s_node_t));
    CU_ASSERT(req != NULL);
    if (req == NULL) { return; }

    req->content = hd_req;
    stream_enqueue(rx, req, false);

    hd_req_t *stop_req = calloc(1, sizeof(hd_req_t));
    CU_ASSERT(stop_req != NULL);
    if (stop_req == NULL) { return; }

    stop_req->stop = true;

    s_node_t *req2 = calloc(1, sizeof(s_node_t));
    CU_ASSERT(req2 != NULL);
    if (req2 == NULL) { return; }

    req2->content = stop_req;
    stream_enqueue(rx, req2, false);

    /** Execute the receive */
    handle_thread((void *) cfg);

    /** tests that all messages have been consummed */
    CU_ASSERT_EQUAL(rx->length, 0);

    /** tests that buffer are being reused */
    CU_ASSERT_EQUAL(tx->length, 1);

    /** tests that the ack has been sent */
    CU_ASSERT_EQUAL(send_tx->length, 1);

    /** tests that no message is sent on the wrong stream */
    CU_ASSERT_EQUAL(send_rx->length, 0);

    s_node_t *resp = stream_pop(send_tx, false);
    CU_ASSERT(resp != NULL);
    if (resp == NULL) { return; }

    tx_req_t *tx_req = (tx_req_t *) resp->content;
    CU_ASSERT(tx_req != NULL);
    if (tx_req == NULL) { return; }

    CU_ASSERT_EQUAL(tx_req->stop, false);

    CU_ASSERT_EQUAL(tx_req->address, NULL);

    CU_ASSERT_EQUAL(tx_req->to_send.type, ACK);

    CU_ASSERT_EQUAL(tx_req->to_send.seqnum, 0);

    CU_ASSERT_EQUAL(tx_req->to_send.window, 31);
}

void test_handler_nack() {
    /** Initialize all the required config and data structs */
    uint8_t ip[16] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };

    stream_t *rx, *tx, *send_tx, *send_rx;
    rx = calloc(1, sizeof(stream_t));
    CU_ASSERT(rx != NULL);
    if (rx == NULL) { return; }

    tx = calloc(1, sizeof(stream_t));
    CU_ASSERT(tx != NULL);
    if (tx == NULL) { return; }
    
    send_tx = calloc(1, sizeof(stream_t));
    CU_ASSERT(send_tx != NULL);
    if (send_tx == NULL) { return; }
    
    send_rx = calloc(1, sizeof(stream_t));
    CU_ASSERT(send_rx != NULL);
    if (send_rx == NULL) { return; }

    allocate_stream(rx, 2);
    allocate_stream(tx, 2);
    allocate_stream(send_tx, 2);
    allocate_stream(send_rx, 2);

    buf_t *window = calloc(1, sizeof(buf_t));
    CU_ASSERT(window != NULL);
    if (window == NULL) { return; }

    allocate_buffer(window, 32);

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

    ht_t *clients = calloc(1, sizeof(ht_t));
    CU_ASSERT(clients != NULL);
    if (clients == NULL) { return; }

    allocate_ht(clients);
    ht_put(clients, 1000, ip, client);

    hd_cfg_t *cfg = calloc(1, sizeof(hd_cfg_t));
    CU_ASSERT(cfg != NULL);
    if (cfg == NULL) { return; }

    cfg->thread = NULL;
    cfg->rx = rx;
    cfg->tx = tx;
    cfg->send_tx = send_tx;
    cfg->send_rx = send_rx;
    cfg->clients = clients;

    /** Create packet to received */
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
    packet->truncated = true;
    packet->type = DATA;

    /** Fill rx with some data */
    hd_req_t *hd_req = calloc(1, sizeof(hd_req_t));
    CU_ASSERT(hd_req != NULL);
    if (hd_req == NULL) { return; }

    hd_req->stop = false;
    memcpy(hd_req->ip, ip, 16);
    hd_req->port = 1000;
    pack(hd_req->buffer, packet, true);
    hd_req->length = 29;


    s_node_t *req = calloc(1, sizeof(s_node_t));
    CU_ASSERT(req != NULL);
    if (req == NULL) { return; }

    req->content = hd_req;
    stream_enqueue(rx, req, false);

    hd_req_t *stop_req = calloc(1, sizeof(hd_req_t));
    CU_ASSERT(stop_req != NULL);
    if (stop_req == NULL) { return; }

    stop_req->stop = true;

    s_node_t *req2 = calloc(1, sizeof(s_node_t));
    CU_ASSERT(req2 != NULL);
    if (req2 == NULL) { return; }

    req2->content = stop_req;
    stream_enqueue(rx, req2, false);

    /** Execute the receive */
    handle_thread((void *) cfg);

    /** tests that all messages have been consummed */
    CU_ASSERT_EQUAL(rx->length, 0);

    /** tests that buffer are being reused */
    CU_ASSERT_EQUAL(tx->length, 1);

    /** tests that the ack has been sent */
    CU_ASSERT_EQUAL(send_tx->length, 1);

    /** tests that no message is sent on the wrong stream */
    CU_ASSERT_EQUAL(send_rx->length, 0);

    s_node_t *resp = stream_pop(send_tx, false);
    CU_ASSERT(resp != NULL);
    if (resp == NULL) { return; }

    tx_req_t *tx_req = (tx_req_t *) resp->content;
    CU_ASSERT(tx_req != NULL);
    if (tx_req == NULL) { return; }

    CU_ASSERT_EQUAL(tx_req->stop, false);

    CU_ASSERT_EQUAL(tx_req->address, NULL);

    CU_ASSERT_EQUAL(tx_req->to_send.type, NACK);

    CU_ASSERT_EQUAL(tx_req->to_send.seqnum, 0);

    CU_ASSERT_EQUAL(tx_req->to_send.window, 31);
}

int add_handler_tests() {
    CU_pSuite pSuite = CU_add_suite("handler_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_handler_data", test_handler_data)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if (NULL == CU_add_test(pSuite, "test_handler_nack", test_handler_nack)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    return 0;
}