#include <unistd.h>
#include "../headers/sender.h"
#include "../headers/packet.h"


int make_sender_socket(const struct sockaddr *dest_addr, socklen_t addrlen) {
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if(sock < 0) { //errno handeled by socket
        return -1;
    }

    int err = connect(sock, dest_addr, addrlen);
    if(err == -1) {
        return -1; // errno handeled by connect
    }

    return sock;
}

int send_packet(int sock, packet_t *msg, size_t msg_len, uint8_t *buf) {
    if(pack(buf, msg, false) == -1) {
        return -1;
    }

    send_msg(sock, (void *) buf, msg_len);
    return 0;
}

int send_msg(int sock, void *msg, size_t msg_len) {
    ssize_t written = write(sock, msg, msg_len);
    if (written == -1) {
        return -1; // errno already set
    }

    return 0;
}

int receive_ack(int sock, packet_t *packet, uint8_t *buf) {
    ssize_t amount_read = read(sock, (void *) buf, sizeof(packet_t));
    if (amount_read == -1) {
        return -1;
    }

    if(unpack(buf, packet) == -1) {
        return -1;
    }

    return 0;
}