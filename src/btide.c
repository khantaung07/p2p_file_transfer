#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "config.c"
#include "peer.h"
#include "cli.h"
#include "dyn_array.h"

#define MAX_DIR_LEN 4096
#define MAX_COMMAND_LEN 5520
#define MAX_ADDRESS_LEN 100

//
// PART 2
//

/* 
 * This thread acts as the main thread that creates client threads and sockets
 * when it wants to connect with a peer. Is also responsible for input parsing 
 * different commands.
 */ 
int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);
    
    // Read from config file
    if (argc < 2) {
        printf("Usage: ./btide <config file>\n");
        exit(1);
    }

    char* config_path = argv[1];

    char directory[MAX_DIR_LEN] = {0};
    int max_peers = 0;
    uint16_t port = 0;

    int parse_config_result = parse_config(config_path, directory,
                                             &max_peers, &port);
    
    // Main program needs to maintain a list of all connections in memory
    // This will be passed to all threads 
    struct dynamic_connection_array *connections = 
    create_dynamic_connection_array(max_peers);

    // Needs to maintain a list of packages in memory
    struct dynamic_package_array *packages = 
    create_dynamic_package_array();

    // Array to store all created threads
    struct thread_array *threads = create_thread_array();
    
    if (parse_config_result != 0) {
        // Exit with corresponding error code
        free_dynamic_connection_array(connections);
        free_dynamic_package_array(packages);
        free_thread_array(threads);
        exit(parse_config_result);
    }

    // Move into the specified directory
    chdir(directory); 

    // Start server
    int server_sock_fd = init_server_socket((port));
    
    // Pass the required information to the server thread
    struct server_loop_arguments args = {server_sock_fd, port, max_peers, 
                                        connections, packages, threads};
    
    // Start server thread
    pthread_t server_thread;
    // Use the port and max peers from the config
    if (pthread_create(&server_thread, NULL, server_loop, &args) != 0) {
        printf("error\n");
        free_dynamic_connection_array(connections);
        free_dynamic_package_array(packages);
        free_thread_array(threads);
        return -1;
    }

    // Buffer to store trailing characters
    char trailing[MAX_LINE_LENGTH] = {0};

    // Input loop
    char buffer[MAX_COMMAND_LEN] = {0};
    while (fgets(buffer, MAX_COMMAND_LEN, stdin) != NULL) {

        // Get command type
        char buffer_copy[MAX_COMMAND_LEN] = {0};
        snprintf(buffer_copy, sizeof(buffer_copy), "%s", buffer);
        char *command_input = strtok(buffer_copy," ");

        command_type command = get_command_type(command_input);

        if (command == CONNECT) {
            char ip_address[MAX_ADDRESS_LEN] = {0};
            int port = 0;

            if (sscanf(buffer, "CONNECT %99[^:]:%d%s", ip_address,
                        &port, trailing) != 2) {
                printf("Missing address and port argument\n");
                continue;
            }  

            // Check if connection already exists
            int connected = 0;
            
            for (int i = 0; i < connections->size; i++) {
                if (strcmp(connections->array[i].ip_address, ip_address) == 0 
                    && connections->array[i].port == port) {
                        printf("Already connected to peer\n");
                        connected = 1;
                    }
            }

            if (connected == 1) {
                continue;
            }

            // Check if we have max connections
            if (connections_true_size(connections) == max_peers) {
                printf("Unable to connect to request peer\n");
                continue;
            }

            // Attempt connection
            int sockfd = create_client_thread(port, ip_address, connections, 
                                                packages, threads);

            if (sockfd < 0) {
                printf("Unable to connect to request peer\n");
                continue;
            }
            
            /* New thread has been created and will receive the ACP packet and 
               print "Connection established with peer" */

            /* Check if the connection was successful (if ACP was received)
               Wait for 0.5 seconds */
            usleep(500000);
            // Check the connection
            if (check_connected(connections, ip_address, port) != 1) {
                // Remove the connection from connections and disconnect
                shutdown(sockfd, 0);
                close(sockfd);
                remove_connection_array(connections, ip_address, port);
                printf("Unable to connect to request peer\n");
            }

            // Otherwise, connection was successful

        }
        else if (command == DISCONNECT) {
            char ip_address[MAX_ADDRESS_LEN] = {0};
            int port = 0;

            if (sscanf(buffer, "DISCONNECT %99[^:]:%d%s", ip_address,
                        &port, trailing) != 2) {
                printf("Missing address and port argument\n");
                continue;
            }  

            // Check if connection exists or not
            int connected = 0;
            struct connection_info connection = {0};
            
            for (int i = 0; i < connections->size; i++) {
                if (strcmp(connections->array[i].ip_address, ip_address) == 0 
                    && connections->array[i].port == port) {
                        connection = connections->array[i];
                        connected = 1;
                        break;
                    }
            }

            if (connected == 0) {
                printf("Unknown peer, not connected\n");
                continue;
            }

            // Then disconnect
            printf("Disconnected from peer\n");
            // Send DSN packet to the peer
            struct btide_packet DSN = generate_DSN();
            send(connection.socket_fd, &DSN, sizeof(struct btide_packet), 0);
            // Close the socket and remove from the connections list
            shutdown(connection.socket_fd, 0);
            close(connection.socket_fd);
            remove_connection_array(connections, connection.ip_address,
                                     connection.port);

        }
        else if (command == ADDPACKAGE) {
            char file_name[MAX_COMMAND_LEN] = {0};

            if (sscanf(buffer, "ADDPACKAGE %[^\n]", file_name) != 1) {
                printf("Missing file argument\n");
                continue;
            }

            // Attempt to access file
            FILE* file = fopen(file_name, "r");
            if (file == NULL) {
                printf("Cannot open file\n");
                continue;
            }

            // Attempt to parse bpkg file
            struct bpkg_obj *package = bpkg_load(file_name);
            if(!package) {
                printf("Unable to parse bpkg file\n");
                fclose(file);
                continue;
		    }

            // Otherwise, add the package to the program's list of packages
            append_dynamic_package_array(packages, package);

            // Close the file
            fclose(file);
        }
        else if (command == REMPACKAGE) {
            char identifier[MAX_COMMAND_LEN] = {0};

            if (sscanf(buffer, "REMPACKAGE %[^\n]", identifier) != 1) {
                printf("Missing identifier argument, please specify whole 102");
                printf("4 character or at least 20 characters\n");
                continue;
            }

            // Check if the package exists in the array
            int result = remove_package_array(packages, identifier);

            if (result == 0) {
                printf("Package has been removed\n");
            }
            else {
                printf("Identifier provided does not match managed packages\n");
            }

            continue;
            
        }
        else if (command == PACKAGES) {
            
            if (packages->size == 0) {
                printf("No packages managed\n");
                continue;
            }
            
            /* Check completion status 
               Compare the return value of min_completed hashes to the root in 
               the bpkg file */
            for (int i = 0; i < packages->size; i++) {
                
                struct bpkg_obj *current = packages->array[i];

                struct bpkg_query result = {0};
                // Get min complete hashes
                result = bpkg_get_min_completed_hashes(current);
                
                // If the hashes is empty
                if (result.len == 0) {
                    printf("%d. %.32s, %s/%s : INCOMPLETE\n", i+1,
                    current->ident, directory, current->filename);
                    bpkg_query_destroy(&result);
                    continue;
                }

                int completed = 1;
                if (result.hashes != NULL) {
                    char hash[64] = {0};
                    memcpy(hash, result.hashes[0], 64);
                    // Check if the root expected hash is returned
                    completed = strncmp(current->hashes[0], hash, 64);
                }   
                // Print file information and completion status
                if (completed == 0) {
                    printf("%d. %.32s, %s/%s : COMPLETED\n", i+1, 
                    current->ident, directory, current->filename);
                }
                else {
                    printf("%d. %.32s, %s/%s : INCOMPLETE\n", i+1,
                    current->ident, directory, current->filename);
                }

                bpkg_query_destroy(&result);
                
            }
        
        }
        else if (command == PEERS) {
            // Check if the connections array is empty
            if (connections_true_size(connections) == 0) {
                printf("Not connected to any peers\n");
                continue;
            }

            // Otherwise, we have valid connections

            printf("Connected to:\n\n");

            // Create PNG packet
            struct btide_packet PNG = generate_PNG();

            int true_count = 0;

            // Print connections and send PNG packets
            for (int i = 0; i < connections->size; i++) {
                if (connections->array[i].connected == 1) {
                    printf("%d. %s:%d\n", true_count+1, connections->array[i].ip_address, 
                        connections->array[i].port);
                    // Send PNG
                    send(connections->array[i].socket_fd, &PNG, 
                        sizeof(struct btide_packet), 0);
                    true_count++;
                }
            }
        }
        else if (command == FETCH) {
            char ip_address[MAX_ADDRESS_LEN] = {0};
            int port = 0;
            char identifier[IDENT_LENGTH] = {0};
            char hash[MAX_COMMAND_LEN] = {0};
            uint32_t offset = -1;

            // Check for valid input
            int num_args = sscanf(buffer, "FETCH %99[^:]:%d %s %s %u", 
                                ip_address, &port, identifier, hash, &offset);
            
            if (num_args != 4 && num_args != 5) {
                printf("Missing arguments from command\n");
                continue;
            }  

            // Check if connection exists
            if (check_connected(connections, ip_address, port) != 1) {
                printf("Unable to request chunk, peer not in list\n");
                continue;
            }
            // Check if package exists
            struct bpkg_obj *package = get_package(packages, identifier);

            if (package == NULL) {
                printf("Unable to request chunk, package is not managed\n");
                continue;
            }

            // Check for valid hash
            int valid = 0;
            struct chunk desired_chunk = {0};

            int num_hashes = package->nchunks;
            for (int i = 0; i < num_hashes; i++) {
                char* current_hash = package->chunks[i].hash;
                if (strncmp(current_hash, hash, 64) == 0) {
                    valid = 1;
                    desired_chunk = package->chunks[i];
                    break;
                }
            }

            if (valid == 0) {
                printf("Unable to request chunk, "
                        "chunk hash does not belong to package\n");
                continue;
            }

            // Otherwise send request 
            // Get the connection
            struct connection_info connection = {0};
            
            for (int i = 0; i < connections->size; i++) {
                if (strcmp(connections->array[i].ip_address, ip_address) == 0 
                    && connections->array[i].port == port) {
                        connection = connections->array[i];
                        break;
                    }
            }

            // Get data length
            uint32_t data_len = desired_chunk.size;
            // If offset was not provided, use offset of the chunk found
            if (offset == -1) {
                offset = desired_chunk.offset;
            }
            // Create and send the REQ packet
            struct btide_packet REQ = generate_REQ(identifier, hash,
                                                     data_len, offset);
            send(connection.socket_fd, &REQ, sizeof(struct btide_packet), 0);
            
        }
        else if (command == QUIT) {
            break;
        }
        else {
            printf("Invalid Input\n");
            continue;
        }

    }

    shutdown(server_sock_fd, 0);
    close(server_sock_fd);
    pthread_join(server_thread, NULL);

    // Close all sockets (causing all threads to die)
    for (int i = 0; i < connections->size; i++) {
        int socket = connections->array[i].socket_fd;
        shutdown(socket, 0);
        close(socket);
    }

    // Wait for all threads to finish 
    for (int i = 0; i < threads->size; i++) {
        pthread_join(threads->array[i], NULL);
    }

    // Free the dynamic arrays
    free_dynamic_connection_array(connections);
    free_dynamic_package_array(packages);
    free_thread_array(threads);

}
