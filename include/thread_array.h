#ifndef THREAD_ARRAY_H
#define THEAD_ARRAY_H

#include <pthread.h>

struct thread_array {
    pthread_t *array;
    size_t size;
    size_t capacity;
    
};

// Function that creates an array that stores threads
struct thread_array *create_thread_array();

// Function that resizes a thread array
void resize_thread_array(struct thread_array *arr);

// Function that adds a thread to an array
void append_thread_array(struct thread_array *arr, pthread_t thread);

// Function that frees a thread array
void free_thread_array(struct thread_array *arr);

#endif