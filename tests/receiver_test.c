#define _GNU_SOURCE

#include "./headers/receiver_test.h"
#include "../headers/global.h"
#include "../headers/receiver.h"

void test_receiver() {


    uint8_t buffers[31][MAX_PACKET_SIZE]; //dopne
    socklen_t addr_len = sizeof(struct sockaddr_in6); //done
    struct sockaddr_in6 addrs[31]; //done
    struct mmsghdr msgs[31]; //done
    struct iovec iovecs[31]; //done

    struct sockaddr_in6 address;
    address.sin6_addr.__in6_u.__u6_addr32[0] = 0;
    address.sin6_addr.__in6_u.__u6_addr32[1] = 0;
    address.sin6_addr.__in6_u.__u6_addr32[2] = 0;
    address.sin6_addr.__in6_u.__u6_addr32[3] = htonl(1);
    address.sin6_family = AF_INET6;
    address.sin6_port = 5556;

    int i;
    for(i = 0; i < 31; i++) {
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

    stream_t rx_to_hd; //done
    CU_ASSERT(initialize_stream(&rx_to_hd) == 0);
    
    stream_t hd_to_rx; //done 
    CU_ASSERT(initialize_stream(&hd_to_rx) == 0);

    ht_t clients;
    CU_ASSERT(allocate_ht(&clients) == 0);




    rx_cfg_t cfg;
    cfg.id = 0;
    cfg.thread = NULL;
    int idx = 0;
    cfg.idx = &idx;
    cfg.file_format = "./bin/%d";
    cfg.stop = false;
    cfg.tx = &rx_to_hd;
    cfg.rx = &hd_to_rx;
    cfg.clients = &clients;
    cfg.sockfd = socket(AF_INET6, SOCK_DGRAM, 0);
    CU_ASSERT(cfg.sockfd != -1);
    CU_ASSERT(bind(cfg.sockfd, &address, addr_len) == 0);
    cfg.addr_len = &addr_len;
    cfg.affinity = NULL;
    cfg.max_clients = 100;
    cfg.window_size = 31;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    CU_ASSERT(setsockopt(cfg.sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0);

    int send_sock = socket(AF_INET6, SOCK_DGRAM|SOCK_NONBLOCK, 0);
    CU_ASSERT(send_sock != -1);

        
    uint8_t send_buf[528];
    packet_t pkt;
    CU_ASSERT(init_packet(&pkt) == 0);

    pkt.type = DATA;
    pkt.window = 1;
    pkt.length = 20;
    pkt.seqnum = 0;
    pkt.timestamp = 99999; //NEIN  NEIN  NEIN  NEIN  NEIN

    CU_ASSERT(pack(send_buf, &pkt, true) == 0);

    size_t n_sent = sendto(send_sock, send_buf, pkt.length + 11 + 4, 0, &address, addr_len);
    CU_ASSERT(n_sent > 0);

    struct sockaddr_in6 sin6;
    CU_ASSERT(getsockname(send_sock, &sin6, &addr_len) != -1);
    uint16_t send_port = sin6.sin6_port;
    sin6.sin6_addr.__in6_u.__u6_addr32[0] = 0;
    sin6.sin6_addr.__in6_u.__u6_addr32[1] = 0;
    sin6.sin6_addr.__in6_u.__u6_addr32[2] = 0;
    sin6.sin6_addr.__in6_u.__u6_addr32[3] = htonl(1);
    
    rx_run_once(&cfg, buffers, addr_len, addrs, msgs, iovecs);

    client_t *client = ht_get(&clients, send_port, sin6.sin6_addr.__in6_u.__u6_addr8);
    CU_ASSERT(client != NULL);
    CU_ASSERT(client->active == true);
    CU_ASSERT(client->window->length == 0);
    CU_ASSERT(client->id == 0);
    CU_ASSERT(client->last_timestamp == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[0] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[1] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[2] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[3] == htonl(1));
    CU_ASSERT(client->addr_len == addr_len);

    s_node_t *s_node = stream_pop(&rx_to_hd, false);
    CU_ASSERT(s_node != NULL);
    CU_ASSERT(s_node->content != NULL);
    hd_req_t *req = s_node->content;
    CU_ASSERT(req->client->address->sin6_port == send_port);
    CU_ASSERT(req->lengths[0] == pkt.length + 11 + 4); // payload + header + CRC2
    CU_ASSERT(req->num == 1);
    CU_ASSERT(req->stop == false);
    deallocate_node(s_node);



    n_sent = sendto(send_sock, send_buf, pkt.length + 11 + 4, 0, &address, addr_len);
    CU_ASSERT(n_sent > 0);

    CU_ASSERT(init_packet(&pkt) == 0);
    pkt.type = DATA;
    pkt.window = 8;
    pkt.length = 88;
    pkt.seqnum = 8;
    pkt.timestamp = 88888; 

    CU_ASSERT(pack(send_buf, &pkt, true) == 0);

    n_sent = sendto(send_sock, send_buf, pkt.length + 11 + 4, 0, &address, addr_len);
    CU_ASSERT(n_sent > 0);

    rx_run_once(&cfg, buffers, addr_len, addrs, msgs, iovecs);

    client = ht_get(&clients, send_port, sin6.sin6_addr.__in6_u.__u6_addr8);
    CU_ASSERT(client != NULL);
    CU_ASSERT(client->active == true);
    CU_ASSERT(client->window->length == 0);
    CU_ASSERT(client->id == 0);
    CU_ASSERT(client->last_timestamp == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[0] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[1] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[2] == 0);
    CU_ASSERT(client->address->sin6_addr.__in6_u.__u6_addr32[3] == htonl(1));
    CU_ASSERT(client->addr_len == addr_len);

    s_node = stream_pop(&rx_to_hd, false);
    CU_ASSERT(s_node != NULL);
    CU_ASSERT(s_node->content != NULL);
    req = s_node->content;
    CU_ASSERT(req->client->address->sin6_port == send_port);
    CU_ASSERT(req->lengths[0] == 20 + 11 + 4); // payload + header + CRC2
    CU_ASSERT(req->lengths[1] == 88 + 11 + 4); // payload + header + CRC2
    CU_ASSERT(req->num == 2);
    CU_ASSERT(req->stop == false);
    deallocate_node(s_node);


    close(cfg.sockfd);
    dealloc_stream(&rx_to_hd);
    dealloc_stream(&hd_to_rx);
    dealloc_ht(&clients);

}



int add_receiver_test() {
    CU_pSuite pSuite = CU_add_suite("receiver_test_suite", 0, 0);

    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    if (NULL == CU_add_test(pSuite, "test_receiver", test_receiver)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    return 0;
}