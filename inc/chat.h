#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#define CHAT_PORT   1050
#define CHAT_PACKET_SIZE    1024

struct chat_packet {
    envid_t sender_id;
    char data[1024];
};

#endif