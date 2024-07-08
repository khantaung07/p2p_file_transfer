#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include "thread_array.h"

pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

#define INITIAL_CAPACITY 10

struct thread_array *create_thread_array() {
    struct thread_array *arr = malloc(sizeof(struct thread_array));

    // Initialise the array
    arr->array = malloc(INITIAL_CAPACITY * sizeof(pthread_t));
    arr->size = 0;
    arr->capacity = INITIAL_CAPACITY;

    return arr;
}

void resize_thread_array(struct thread_array *arr) {
    pthread_mutex_lock(&thread_mutex);
    arr->capacity *= 2;
    arr->array = realloc(arr->array, arr->capacity * sizeof(pthread_t));
    pthread_mutex_unlock(&thread_mutex);
}

void append_thread_array(struct thread_array *arr, pthread_t thread) {
    pthread_mutex_lock(&thread_mutex);
    if (arr->size == arr->capacity) {
        resize_thread_array(arr);
    }
    arr->array[arr->size++] = thread;
    pthread_mutex_unlock(&thread_mutex);
}

void free_thread_array(struct thread_array *arr) {
    pthread_mutex_lock(&thread_mutex);
    free(arr->array);
    free(arr);
    pthread_mutex_unlock(&thread_mutex);
}