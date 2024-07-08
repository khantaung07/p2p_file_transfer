#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#define DATA_MAX 2998

#include "dyn_array.h"
#include "peer.h"

/* 
 * Function that initialises a server socket 
 */
int init_server_socket(in_port_t port_no) {
    errno = 0;
    // Create the socket
    int server_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock_fd < 0) {
        // Error
        exit(1);
    }

    // Normal initialisation
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port_no),
    };

    errno = 0;
    // Bind attempt
    int bind_res = bind(server_sock_fd, (const struct sockaddr *)(&server_addr),
                        sizeof(struct sockaddr_in));
    if (bind_res < 0) {
        // Error
        close(server_sock_fd);
        exit(1);
    }

    // Success
    return server_sock_fd;
}


/* This thread function is to handle one specific client connection
 * Receives packets and responds correspondingly.
 */
void* server_handle_client(void *arg) {
    struct client_thread_args *args = (struct client_thread_args *)arg;
    // This struct contains the necessary data to read and write from the client
    
    struct dynamic_connection_array *connections = args->connections;
    struct dynamic_package_array *packages = args->packages;

    // This is the socket used to communicate with the client
    int client_socket = args->client_sock_fd;

    struct sockaddr_in address = args->client_addr;

     // Allocate buffer for IP address
    char client_ip_buffer[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &args->client_addr.sin_addr, client_ip_buffer,
             sizeof(client_ip_buffer));
    
    int new_port = ntohs(address.sin_port);

    // First send the ACP packet to acknowledge acceptance
    struct btide_packet ACP = generate_ACP();

    if (send(client_socket, &ACP, sizeof(struct btide_packet), 0) == -1) {
        // Error occured: could not send ACP packet back
        return NULL;
    }

    // Free the arguments
    free(args);

    // Loop while connection is valid
    while (1) {
        
        // Waiting for packets from the socket
        struct btide_packet current_packet = {0};

        // Receive packet
        if (recv(client_socket, &current_packet, sizeof(struct btide_packet), 
            0) <= 0) {
            // Error or socket has closed
            break;
        }

        // Check what the packet is 

        // If ACK, continue
        if (current_packet.msg_code == PKT_MSG_ACK) {
            continue;
        }
        if (current_packet.msg_code == PKT_MSG_DSN) {
            // Package is a disconnect package 
            remove_connection_array(connections, client_ip_buffer, new_port);
            break;
        }
        if (current_packet.msg_code == PKT_MSG_PNG) {
            // Send POG pack
            struct btide_packet POG = generate_POG();
            send(client_socket, &POG, sizeof(struct btide_packet), 0);
            continue;
        }
        if (current_packet.msg_code == PKT_MSG_POG) {
            continue;
        }
        if (current_packet.msg_code == PKT_MSG_REQ) {
            // Received a REQ packet; send RES packets back
            // Extract the data from the packet
            uint32_t offset = 0;
            uint32_t data_len = 0;
            char hash[SHA256_HEXLEN] = {0};
            char identifier[IDENT_LENGTH] = {0};

            memcpy(&offset, current_packet.pl.data, sizeof(uint32_t));
            memcpy(&data_len, current_packet.pl.data + sizeof(uint32_t), 
                    sizeof(uint32_t));
            memcpy(hash, current_packet.pl.data + 2 * sizeof(uint32_t), 
                    SHA256_HEXLEN);
            memcpy(identifier, 
                    current_packet.pl.data + 2 * sizeof(uint32_t) 
                    + SHA256_HEXLEN, IDENT_LENGTH-1);

            // Handle sending RES packets in separate function
            // Pass the required arguments
            generate_and_send_RES(client_socket, identifier, hash, 
                                    data_len, offset, packages);

        }
        if (current_packet.msg_code == PKT_MSG_RES) {
            // Received a RES packet
            if (current_packet.error != 0) {
                // Do nothing
                continue;
            }
            // Extract data from the packet
            uint32_t offset = 0;
            uint16_t data_len = 0;
            char hash[SHA256_HEXLEN] = {0};
            char identifier[IDENT_LENGTH] = {0};
            char data[DATA_MAX];

            memcpy(&offset, current_packet.pl.data, sizeof(uint32_t));
            memcpy(data, current_packet.pl.data + sizeof(uint32_t), DATA_MAX);
            memcpy(&data_len, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t), sizeof(uint16_t));
            memcpy(hash, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t) + sizeof(uint16_t), SHA256_HEXLEN);
            memcpy(identifier, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t) + sizeof(uint16_t) + SHA256_HEXLEN, 
                    IDENT_LENGTH-1);

            // Write the data to the corresponding data file
            struct bpkg_obj *package = get_package(packages, identifier);

            if (package == NULL) {
                // Some kind of error
                continue;
            }

            // Check if the file exists in the directory, otherwise create it
            struct bpkg_query temp = bpkg_file_check(package);
            bpkg_query_destroy(&temp);

            // Write the data to the file
            FILE *file = fopen(package->filename, "rb+");

            // Seek to the specified offset within the file
            if (fseek(file, offset, SEEK_SET) != 0) {
                // Malformed file, some error
                fclose(file);
                continue; 
            }

            fwrite(data, 1, data_len, file);
            fclose(file);
        }

    }

    // Close the connection
    // Remove the connection from the connections array (set it to disconnected)
    set_disconnected(connections, client_ip_buffer, new_port);
    close(client_socket);
    
    return NULL;

}

 /*
  * This is for a server thread function
  * Loops and waits for connections until it connects to one, and then creates a 
  * new thread to handle this connection
  */
void* server_loop(void *arg) {
    struct server_loop_arguments *args = (struct server_loop_arguments*)(arg);

    struct dynamic_connection_array *connections = args->connections;
    struct dynamic_package_array *packages = args->packages;
    struct thread_array *threads = args->threads;

    // Set up the server
    int server_sock_fd = args->server_socket;

    // Start listening
    int MAX_PEERS = args->MAX_PEERS;

    errno = 0;
    if (listen(server_sock_fd, MAX_PEERS) < 0) {
        // Error
        close(server_sock_fd);
        exit(1);
    }

    // Infinite loop to get client connections
    // Thread needs to be able to exit when the socket connection is closed
    while (1) {
        // Accept a client and make a new thread
        struct sockaddr_in client_addr = {0};

        // Accept the connection
        errno = 0;
        int client_sock_fd = accept(server_sock_fd,
        (struct sockaddr *)&client_addr,
        &(socklen_t){sizeof(struct sockaddr_in)});

        if (client_sock_fd < 0) {
            // Error with connection
            break;
        }

        // Check if we have maximum connections
        // Cannot accept any connections if this condition is true
        // Instantly disconnect
        if (connections_true_size(connections) == connections->max_peers) {
            close(client_sock_fd);
            continue;
        }

        int new_port = ntohs(client_addr.sin_port);

        // Add the socket to the connections array
        struct connection_info connection = {client_sock_fd, new_port};
        connection.ip_address = malloc(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(client_addr.sin_addr), connection.ip_address,
                             INET_ADDRSTRLEN);
        
        // Set default connection status to 1
        // Server connections will always be automatically connected 
        // as they just send ACP messages; don't receive ACK to confirm
        connection.connected = 1;

        append_dynamic_connection_array(connections, connection);

        // Otherwise, a succesful connection to the client, create a new thread
        // to handle this connection
        pthread_t new_thread;
        
        // Struct to pass to thread function
        struct client_thread_args *thread_args = 
        malloc(sizeof(struct client_thread_args));
        // Need to pass required arguments
        thread_args->client_sock_fd = client_sock_fd;
        memcpy(&thread_args->client_addr, &client_addr, 
        sizeof(struct sockaddr_in));
        thread_args->connections = connections;
        thread_args->packages = packages;
        

        // Create the thread
        if (pthread_create(&new_thread, NULL, server_handle_client, 
        thread_args) != 0) {
            // Error creating thread
            perror("Error creating server thread.\n");
            close(client_sock_fd);
            exit(1);
        }

        
        // Thread should now be running and managing that connection
        append_thread_array(threads, new_thread);
    }

    // Socket closed
    close(server_sock_fd);
    // Don't need to close the client socket: the other threads will
    
    return NULL;
}

/*
 * This thread is responsible for outgoing connections (when our program wants 
 * to connect with others and is the client).
 * Creates a client socket and connects to the server 
 */
void* client_handle_server(void *arg) {
    struct client_thread_args *args = (struct client_thread_args *)arg;

    // Has access to all connections/packages via args
    struct dynamic_connection_array *connections = args->connections;
    struct dynamic_package_array *packages = args->packages;
    
    // Client port created in the create_client_thread function
    int client_socket = args->client_sock_fd;

    struct sockaddr_in address = args->client_addr;
     // Allocate buffer for IP address
    char server_ip_buffer[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &(address.sin_addr), server_ip_buffer, INET_ADDRSTRLEN);
    // Get the port
    int port = ntohs(address.sin_port);

    // Free arguments
    free(args);

    while (1) {
        // Waiting for packets from the socket
        struct btide_packet current_packet = {0};

        // Receive packet
        if (recv(client_socket, &current_packet, sizeof(struct btide_packet), 
            0) <= 0) {
            // Connection has shutdown for whatever reason
            break;
        }

        // Check what the packet is 
        // If ACP, send an ACK packet back
        if (current_packet.msg_code == PKT_MSG_ACP) {
            printf("Connection established with peer\n");
            struct btide_packet ACK = generate_ACK();
            send(client_socket, &ACK, sizeof(struct btide_packet), 0);
            // Set the connection to connected for this specific connection
            set_connected(connections, server_ip_buffer, port);
            continue;
        }

        if (current_packet.msg_code == PKT_MSG_DSN) {
            // Package is a disconnect package 
            remove_connection_array(connections, server_ip_buffer, port);
            break;
        }

        if (current_packet.msg_code == PKT_MSG_PNG) {
            // Send POG pack
            struct btide_packet POG = generate_POG();
            send(client_socket, &POG, sizeof(struct btide_packet), 0);
            continue;
        }

        if (current_packet.msg_code == PKT_MSG_POG) {
            continue;
        }

        if (current_packet.msg_code == PKT_MSG_REQ) {
            // Received a REQ packet; send RES packets back
            // Extract the data from the packet
            uint32_t offset = 0;
            uint32_t data_len = 0;
            char hash[SHA256_HEXLEN] = {0};
            char identifier[IDENT_LENGTH] = {0};

            memcpy(&offset, current_packet.pl.data, sizeof(uint32_t));
            memcpy(&data_len, current_packet.pl.data + sizeof(uint32_t), 
                    sizeof(uint32_t));
            memcpy(hash, current_packet.pl.data + 2 * sizeof(uint32_t), 
                    SHA256_HEXLEN);
            memcpy(identifier, 
                    current_packet.pl.data + 2 * sizeof(uint32_t) 
                    + SHA256_HEXLEN, IDENT_LENGTH-1);

            // Handle sending RES packets in separate function
            // Pass the required arguments
            generate_and_send_RES(client_socket, identifier, hash, 
                                    data_len, offset, packages);

        }
        if (current_packet.msg_code == PKT_MSG_RES) {
            // Received a RES packet
            if (current_packet.error != 0) {
                // Do nothing
                continue;
            }
            // Extract data from the packet
            uint32_t offset = 0;
            uint16_t data_len = 0;
            char hash[SHA256_HEXLEN] = {0};
            char identifier[IDENT_LENGTH] = {0};
            char data[DATA_MAX];

            memcpy(&offset, current_packet.pl.data, sizeof(uint32_t));

            memcpy(data, current_packet.pl.data + sizeof(uint32_t), DATA_MAX);

            memcpy(&data_len, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t), sizeof(uint16_t));

            memcpy(hash, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t) + sizeof(uint16_t), SHA256_HEXLEN);

            memcpy(identifier, current_packet.pl.data + DATA_MAX + 
                    sizeof(uint32_t) + sizeof(uint16_t) + SHA256_HEXLEN, 
                    IDENT_LENGTH-1);

            // Write the data to the corresponding data file
            struct bpkg_obj *package = get_package(packages, identifier);

            if (package == NULL) {
                // Some kind of error
                continue;
            }

            // Check if the file exists in the directory, otherwise create it
            struct bpkg_query temp = bpkg_file_check(package);
            bpkg_query_destroy(&temp);

            // Write the data to the file
            FILE *file = fopen(package->filename, "rb+");

            // Seek to the specified offset within the file
            if (fseek(file, offset, SEEK_SET) != 0) {
                // Malformed file, some error
                fclose(file);
                continue; 
            }

            fwrite(data, 1, data_len, file);
            fclose(file);
        }

    }

    // Connection closed

    // Set connection to disconnected
    set_disconnected(connections, server_ip_buffer, port);
    close(client_socket);

    return NULL;

}

/* 
 * Function that creates a thread to handle a new client connection
 * Returns the file descriptor for the created socket.
 */
int create_client_thread(int port_to_connect, char* ip_address, 
                            struct dynamic_connection_array *connections,
                            struct dynamic_package_array *packages,
                            struct thread_array *threads) {

    pthread_t new_thread;

    struct sockaddr_in serv_addr;
    // Configure server address
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_to_connect);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        return -1;
    }
    
    int client_socket = 0;
    
    // Create socket file descriptor
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&serv_addr, 
        sizeof(serv_addr)) < 0) {
        return -1;
    }

    struct client_thread_args *thread_args = 
    malloc(sizeof(struct client_thread_args));
    
    // Add the socket to the connections array
    struct connection_info connection = {client_socket, port_to_connect, 
    strdup(ip_address)};

    // Set default connection status to 0: need to wait for ACP packet
    connection.connected = 0;
    append_dynamic_connection_array(connections, connection);

    // Pass information to handler thread
    thread_args->client_sock_fd = client_socket;
    memcpy(&thread_args->client_addr, &serv_addr, sizeof(struct sockaddr_in));
    thread_args->connections = connections;
    thread_args->packages = packages;

    // Create the thread
    if (pthread_create(&new_thread, NULL, client_handle_server, 
    thread_args) != 0) {
        // Error creating thread
        perror("Error creating client thread.\n");
        return -1;
    }

    append_thread_array(threads, new_thread);

    // Thread should now be running and managing that connection
    return client_socket;

}
