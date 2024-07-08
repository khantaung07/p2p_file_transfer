#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "dyn_array.h"
#include "peer.h"

// Mutexes to prevent race condition across threads 
pthread_mutex_t connections_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packages_mutex = PTHREAD_MUTEX_INITIALIZER;

#define INITIAL_CAPACITY 10

struct dynamic_connection_array *create_dynamic_connection_array(
    size_t max_peers) {
    struct dynamic_connection_array *arr = 
    malloc(sizeof(struct dynamic_connection_array));
    
    arr->array = malloc(INITIAL_CAPACITY * sizeof(struct connection_info));
    
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;
    arr->max_peers = max_peers;
    return arr;
}

void resize_dynamic_connection_array(struct dynamic_connection_array *arr) {
    pthread_mutex_lock(&connections_mutex);
    arr->capacity *= 2;
    arr->array = realloc(arr->array, arr->capacity *
                         sizeof(struct connection_info));
    pthread_mutex_unlock(&connections_mutex);
}

void append_dynamic_connection_array(struct dynamic_connection_array *arr, 
                            struct connection_info connection) {
    pthread_mutex_lock(&connections_mutex);
    if (arr->size == arr->capacity) {
        resize_dynamic_connection_array(arr);
    }
    arr->array[arr->size++] = connection;
    pthread_mutex_unlock(&connections_mutex);
}

void free_dynamic_connection_array(struct dynamic_connection_array *arr) {
    pthread_mutex_lock(&connections_mutex);
    for (size_t i = 0; i < arr->size; i++) {
        if (arr->array[i].ip_address != NULL) {
            free(arr->array[i].ip_address);
        }
    }
    free(arr->array);
    free(arr);
    pthread_mutex_unlock(&connections_mutex);
}

/*
 * Function that removes a connection from the array if it exists
 * Returns 0 upon success, returns 1 if it does not exist
 */
int remove_connection_array(struct dynamic_connection_array *arr, 
                            char *ip_address, int port) {
    pthread_mutex_lock(&connections_mutex);
    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the IP address and port of the current connection match
        if (strcmp(arr->array[i].ip_address, ip_address) == 0 
                    && arr->array[i].port == port) {
            // Free memory in ip address
            free(arr->array[i].ip_address);
            // Shift elements over
            for (int j = i; j < arr->size - 1; j++) {
                arr->array[j] = arr->array[j + 1];
            }
            // Decrement the size
            arr->size--;
            // Success
            pthread_mutex_unlock(&connections_mutex);
            return 0;
        }
    }
    // Not found
    pthread_mutex_unlock(&connections_mutex);
    return 1; 
    
}

/*
 * Function that sets a particular connection to 'connected'
 */
void set_connected(struct dynamic_connection_array *arr,
                       char *ip_address, int port) {
    pthread_mutex_lock(&connections_mutex);
    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the IP address and port of the current connection match
        if (strcmp(arr->array[i].ip_address, ip_address) == 0 
                    && arr->array[i].port == port) {
            // Set it to connected
            arr->array[i].connected = 1;
            // Success
            pthread_mutex_unlock(&connections_mutex);
            return;
        }
    }
    // Not found
    pthread_mutex_unlock(&connections_mutex);
    return; 
}


/*
 * Function that sets a particular connection to 'disconnected'
 */
void set_disconnected(struct dynamic_connection_array *arr,
                       char *ip_address, int port) {
    pthread_mutex_lock(&connections_mutex);
    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the IP address and port of the current connection match
        if (strcmp(arr->array[i].ip_address, ip_address) == 0 
                    && arr->array[i].port == port) {
            // Set it to disconnected
            arr->array[i].connected = 0;
            // Success
            pthread_mutex_unlock(&connections_mutex);
            return;
        }
    }
    // Not found
    pthread_mutex_unlock(&connections_mutex);
    return; 
}


/*
 * Function that returns the connection status of a particular connection
 * Connection is set after ACP/ACK process.
 */
int check_connected(struct dynamic_connection_array *arr,
                       char *ip_address, int port) {
    pthread_mutex_lock(&connections_mutex);
    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the IP address and port of the current connection match
        if (strcmp(arr->array[i].ip_address, ip_address) == 0 
                    && arr->array[i].port == port) {
            // Return it
            pthread_mutex_unlock(&connections_mutex);
            return (arr->array[i].connected);
        }
    }
    // Not found
    pthread_mutex_unlock(&connections_mutex);
    return -1; 
}

/* 
 * Returns the number of connected connections
 */
int connections_true_size(struct dynamic_connection_array *arr) {
    int result = 0;
    pthread_mutex_lock(&connections_mutex);

    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the connection is connected
        if (arr->array[i].connected == 1) {
            // Add to the count
            result++;
        }
    }
    pthread_mutex_unlock(&connections_mutex);
    return result;
}

// Package array

struct dynamic_package_array *create_dynamic_package_array() {
    struct dynamic_package_array *arr = 
    malloc(sizeof(struct dynamic_package_array));
    
    arr->array = malloc(INITIAL_CAPACITY * sizeof(struct bpkg_obj *));
    
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;
    return arr;
}

void resize_dynamic_package_array(struct dynamic_package_array *arr) {
    pthread_mutex_lock(&packages_mutex);
    arr->capacity *= 2;
    arr->array = realloc(arr->array, arr->capacity *
                         sizeof(struct bpkg_obj *));
    pthread_mutex_unlock(&packages_mutex);
}

void append_dynamic_package_array(struct dynamic_package_array *arr, 
                            struct bpkg_obj *package) {
    pthread_mutex_lock(&packages_mutex);
    if (arr->size == arr->capacity) {
        resize_dynamic_package_array(arr);
    }
    arr->array[arr->size++] = package;
    pthread_mutex_unlock(&packages_mutex);
}

void free_dynamic_package_array(struct dynamic_package_array *arr) {
    pthread_mutex_lock(&packages_mutex);
    // Free each bpkg object
    for (int i = 0; i < arr->size; ++i) {
        // Free each individual bpkg_obj pointer
        bpkg_obj_destroy(arr->array[i]);
    }
    free(arr->array);
    free(arr);
    pthread_mutex_unlock(&packages_mutex);
}

/*
 * Function that removes a package from the array if it exists
 * Returns 0 upon success, returns 1 if it does not exist
 */
int remove_package_array(struct dynamic_package_array *arr, 
                        char *identifier) {
    pthread_mutex_lock(&packages_mutex);
    // Iterate through the array
    for (int i = 0; i < arr->size; i++) {
        // Check if the identifier of the current package matches
        if (strncmp(arr->array[i]->ident, identifier, 20) == 0) {
            // Shift elements over

            bpkg_obj_destroy(arr->array[i]);

            for (int j = i; j < arr->size - 1; j++) {
                arr->array[j] = arr->array[j + 1];
            }
            // Decrement the size
            arr->size--;
            // Success
            pthread_mutex_unlock(&packages_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&packages_mutex);
    return 1;
}

/* 
 * Function that returns a package from an array given its identifier. 
 * Returns the package, or NULL if the package does not exist in the array.
 */
struct bpkg_obj *get_package(struct dynamic_package_array *arr, char* ident) {
    pthread_mutex_lock(&packages_mutex);

    for (int i = 0; i < arr->size; i++) {
        // Check if the identifier of the current package matches
        if (strncmp(arr->array[i]->ident, ident, 20) == 0) {
            // Return the package
            struct bpkg_obj *package = arr->array[i];
            // Success
            pthread_mutex_unlock(&packages_mutex);
            return package;
        }
    }
    // Not found
    pthread_mutex_unlock(&packages_mutex);
    return NULL;
}