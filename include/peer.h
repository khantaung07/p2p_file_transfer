#ifndef PEER_H
#define PEER_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <net/packet.h>
#include "thread_array.h"

struct client_thread_args {
    int client_sock_fd;
    struct sockaddr_in client_addr;
    struct dynamic_connection_array *connections;
    struct dynamic_package_array *packages;
};

struct server_loop_arguments {
    int server_socket;
    int port;
    int MAX_PEERS;
    struct dynamic_connection_array *connections;
    struct dynamic_package_array *packages;
    struct thread_array *threads;
};

/* 
 * Function that initialises a server socket 
 */
int init_server_socket(in_port_t port_no);

/* This thread function is to handle one specific client connection
 * Receives packets and responds correspondingly.
 */
void* server_handle_client(void *arg);

 /*
  * This is for a server thread function
  * Loops and waits for connections until it connects to one, and then creates a 
  * new thread to handle this connection
  */
void* server_loop(void *arg);

/*
 * This thread is responsible for outgoing connections (when our program wants 
 * to connect with others and is the client).
 * Creates a client socket and connects to the server and receives packets and 
 * responds correspondingly.
 */
void* client_handle_server(void *arg);

/* 
 * Function that creates a thread to handle a new client connection
 * Returns the file descriptor for the created socket.
 */
int create_client_thread(int port_to_connect, char* ip_address, 
                            struct dynamic_connection_array *connections,
                            struct dynamic_package_array *packages,
                            struct thread_array *threads);

#endif