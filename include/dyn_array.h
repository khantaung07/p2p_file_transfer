#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H

#include <chk/pkgchk.h>

struct connection_info {
    int socket_fd;
    int port; 
    char* ip_address;
    int connected;
};

struct dynamic_connection_array {
    struct connection_info *array;
    size_t size;
    size_t max_peers;
    size_t capacity;
};

struct dynamic_package_array {
    struct bpkg_obj **array;
    size_t size;
    size_t capacity;
};

// Functions that create arrays to store connections/packages
struct dynamic_connection_array *create_dynamic_connection_array(
    size_t max_peers);
struct dynamic_package_array *create_dynamic_package_array();

// Functions that resize the arrays
void resize_dynamic_connection_array(struct dynamic_connection_array *arr);
void resize_dynamic_package_array(struct dynamic_package_array *arr);

// Functions that append new connections/packages to an array
void append_dynamic_connection_array(struct dynamic_connection_array *arr, 
                            struct connection_info connection);

void append_dynamic_package_array(struct dynamic_package_array *arr,
                            struct bpkg_obj *package);

// Functions that free the memory of the array
void free_dynamic_connection_array(struct dynamic_connection_array *arr);
void free_dynamic_package_array(struct dynamic_package_array *arr);

/* Functions that remove a connection/package from an array
   Return 0 upon success, returns 1 if the connection/package does not exist */
int remove_connection_array(struct dynamic_connection_array *arr, 
                            char *ip_address, int port);
int remove_package_array(struct dynamic_package_array *arr, 
                            char *identifier);

/*
 * Function that sets a particular connection to 'connected'
 */
void set_connected(struct dynamic_connection_array *arr,
                       char *ip_address, int port);
/*
 * Function that sets a particular connection to 'disconnected'
 */
void set_disconnected(struct dynamic_connection_array *arr,
                       char *ip_address, int port);

/*
 * Function that returns the connection status of a particular connection
 * Connection is set after ACP/ACK process.
 */
int check_connected(struct dynamic_connection_array *arr,
                       char *ip_address, int port);

/* 
 * Returns the number of connected connections
 */
int connections_true_size(struct dynamic_connection_array *arr);

/* 
 * Function that returns a package from an array given its identifier. 
 * Returns the package, or NULL if the package does not exist in the array.
 */
struct bpkg_obj *get_package(struct dynamic_package_array *arr, char* ident);

#endif