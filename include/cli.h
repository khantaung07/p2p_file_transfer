#ifndef CLI_H
#define CLI_H

#include <stdio.h>
#include <string.h>

typedef enum {
    CONNECT,
    DISCONNECT,
    ADDPACKAGE,
    REMPACKAGE,
    PACKAGES,
    PEERS,
    FETCH,
    QUIT,
    INVALID
} command_type; 

/*
 * Function that checks what command input was given.
 * Returns the corresponding command type.
 */
command_type get_command_type(char* command);

#endif