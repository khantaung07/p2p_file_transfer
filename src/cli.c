#include "cli.h"

/*
 * Function that checks what command input was given.
 * Returns the corresponding command type.
 */
command_type get_command_type(char* command) {
    switch(command[0]) {
        case 'C':
            if (strcmp(command, "CONNECT") == 0 || 
            strcmp(command, "CONNECT\n") == 0)
                return CONNECT;
            break;
        case 'D':
            if (strcmp(command, "DISCONNECT") == 0 || 
            strcmp(command, "DISCONNECT\n") == 0)
                return DISCONNECT;
            break;
        case 'A':
            if (strcmp(command, "ADDPACKAGE") == 0 || 
            strcmp(command, "ADDPACKAGE\n") == 0)
                return ADDPACKAGE;
            break;
        case 'R':
            if (strcmp(command, "REMPACKAGE") == 0 || 
            strcmp(command, "REMPACKAGE\n") == 0)
                return REMPACKAGE;
            break;
        case 'P':
            if (strcmp(command, "PACKAGES") == 0 || 
            strcmp(command, "PACKAGES\n") == 0)
                return PACKAGES;
            else if (strcmp(command, "PEERS") == 0 || 
            strcmp(command, "PEERS\n") == 0)
                return PEERS;
            break;
        case 'F':
            if (strcmp(command, "FETCH") == 0 || 
            strcmp(command, "FETCH\n") == 0)
                return FETCH;
            break;
        case 'Q':
            if (strcmp(command, "QUIT") == 0 || 
            strcmp(command, "QUIT\n") == 0)
                return QUIT;
            break;
        default:
            return INVALID;
    }
    return INVALID;
}




