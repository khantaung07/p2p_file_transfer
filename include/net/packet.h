#ifndef NETPKT_H
#define NETPKT_H

#include <stdint.h>
#include <dyn_array.h>

#define PAYLOAD_MAX (4092)

#define PKT_MSG_ACK 0x0c
#define PKT_MSG_ACP 0x02
#define PKT_MSG_DSN 0x03
#define PKT_MSG_REQ 0x06
#define PKT_MSG_RES 0x07
#define PKT_MSG_PNG 0xFF
#define PKT_MSG_POG 0x00


union btide_payload {
    uint8_t data[PAYLOAD_MAX];

};

struct btide_packet {
    uint16_t msg_code;
    uint16_t error;
    union btide_payload pl;
};

/* The following functions generate the corresponding packets for communication
 * over a network.
 */

struct btide_packet generate_ACP(void);

struct btide_packet generate_ACK(void);

struct btide_packet generate_DSN(void);

struct btide_packet generate_PNG(void);

struct btide_packet generate_POG(void);

struct btide_packet generate_REQ(char* identifier, char* hash,
                                uint32_t data_len, uint32_t offset);

/* 
 * This function takes in the necessary information required for a RES packet 
 * and is also responsible for sending the RES packets after receiving a REQ 
 * packets, provided the socket file descriptor.
 * Returns 0 upon success, otherwise returns 1.
 */
int generate_and_send_RES(int socket_fd, char* identifier, char* hash, 
                    uint32_t data_len, uint32_t offset, 
                    struct dynamic_package_array *packages);

#endif
