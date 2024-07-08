#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <net/packet.h>

#define IDENT_LEN 1024
#define HASH_LEN 64
#define DATA_MAX 2998

struct btide_packet generate_ACP(void) {
    struct btide_packet packet = {
            .msg_code = PKT_MSG_ACP,
            .error = 0
    };
    return packet;
}

struct btide_packet generate_ACK(void) {
    struct btide_packet packet = {
            .msg_code = PKT_MSG_ACK,
            .error = 0
    };
    return packet;
}

struct btide_packet generate_DSN(void) {
    struct btide_packet packet = {
            .msg_code = PKT_MSG_DSN,
            .error = 0
    };
    return packet;
}

struct btide_packet generate_PNG(void) {
    struct btide_packet packet = {
            .msg_code = PKT_MSG_PNG,
            .error = 0
    };
    return packet;
}

struct btide_packet generate_POG(void) {
    struct btide_packet packet = {
            .msg_code = PKT_MSG_POG,
            .error = 0
    };
    return packet;
}

struct btide_packet generate_REQ(char* identifier, char* hash, 
                                uint32_t data_len, uint32_t offset) {
    struct btide_packet packet = {
        .msg_code = PKT_MSG_REQ,
        .error = 0
    };
    // Copy data into the packet's payload
    memset(packet.pl.data, 0, PAYLOAD_MAX);
    memcpy(packet.pl.data, &offset, sizeof(uint32_t));
    memcpy(packet.pl.data + sizeof(uint32_t), &data_len, sizeof(uint32_t));
    memcpy(packet.pl.data + 2 * sizeof(uint32_t), hash, HASH_LEN);
    memcpy(packet.pl.data + sizeof(uint32_t) + sizeof(uint32_t) + HASH_LEN, 
            identifier, strlen(identifier));
    
    return packet;
}

/* 
 * This function takes in the necessary information required for a RES packet 
 * and is also responsible for sending the RES packets after receiving a REQ 
 * packets, provided the socket file descriptor.
 * Returns 0 upon success, otherwise returns 1.
 */
int generate_and_send_RES(int socket_fd, char* identifier, char* hash, 
                    uint32_t data_len, uint32_t offset, 
                    struct dynamic_package_array *packages) {
    // Check if we have the corresponding package
    struct bpkg_obj *package = get_package(packages, identifier);

    if (package == NULL) {
        // Send RES packet with error byte - we do not have the package
        struct btide_packet error = {
            .msg_code = PKT_MSG_RES,
            .error = 1
        };
        send(socket_fd, &error, sizeof(struct btide_packet), 0);
        return 1;
    }

    // Check if our hash is complete or not
    // Traverse tree and check completion status
    struct bpkg_query temp = bpkg_get_min_completed_hashes(package);
    bpkg_query_destroy(&temp);
    // Compare expected to computed of the particular chunk
    struct merkle_tree_node *chunk = find_hash(package->tree->root, hash);
    
    // Compare expected and computed
    if (strncmp(chunk->expected_hash, chunk->computed_hash, 64) != 0) {
        // Chunk is incomplete; send error RES packet
        struct btide_packet error = {
            .msg_code = PKT_MSG_RES,
            .error = 1
        };
        send(socket_fd, &error, sizeof(struct btide_packet), 0);
        return 1;
    }

    // Otherwise, we have the package and the chunk is complete
    // Send the data
    int bytes_to_send = data_len;
    int current_offset = offset;

    // Open the file
    FILE* data = fopen(package->filename, "rb");

    while (bytes_to_send > 0) {

        // Current data length is minimum of (bytes left to send) and max load
        int current_data_len = (bytes_to_send > DATA_MAX) ?
                                 DATA_MAX : bytes_to_send;

        // Set RES packet to appropriate values
        struct btide_packet RES = {
        .msg_code = PKT_MSG_RES,
        .error = 0
        };

        // Copy data into the packet's payload
        memset(RES.pl.data, 0, PAYLOAD_MAX);
        memcpy(RES.pl.data, &current_offset, sizeof(uint32_t));
        // Read the required length of datafrom the actual file
        char buffer[DATA_MAX] = {0};
        fseek(data, current_offset, SEEK_SET);
        fread(buffer, 1, current_data_len, data);
        memcpy(RES.pl.data + sizeof(uint32_t), buffer, current_data_len);
        memcpy(RES.pl.data + DATA_MAX + sizeof(uint32_t), &current_data_len, 
                sizeof(uint16_t));
        memcpy(RES.pl.data + DATA_MAX +  sizeof(uint32_t) + sizeof(uint16_t), 
                hash, HASH_LEN);
        memcpy(RES.pl.data + DATA_MAX + sizeof(uint32_t) + sizeof(uint16_t)
            + HASH_LEN, identifier, IDENT_LEN);
        
        // Send the packet
        send(socket_fd, &RES, sizeof(struct btide_packet), 0);

        // After sending, adjust loop variables
        bytes_to_send-=current_data_len;
        current_offset+=current_data_len;
    }

fclose(data);
return 0;

}