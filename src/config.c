#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 4096

/*
 * Function that parses a configuration file provided in the command line 
 * arguments and stores the value in given pointer addresses
 * Returns 0 upon success, otherwise returning the corresponding error code
 */
int parse_config(const char* config_path, char* directory, int* max_peers, 
uint16_t* port) {

    FILE* config = fopen(config_path, "r");

    if (config == NULL) {
        // If the configuration file does not exist
        return -1;
    }

    // Read from the file
    char buffer[MAX_LINE_LENGTH];
    int line_count = 0;

    while (fgets(buffer, sizeof(buffer), config) != NULL) {
        line_count++;

        if (line_count > 3) {
            // Extra lines in config file
            fclose(config);
            return -1;
        }

        char* label = strtok(buffer, ":");
        char* value = strtok(NULL, ":");

        if (label != NULL && value != NULL) {
            if (strcmp(label, "directory") == 0) {
                // Store the directory path
                // Strip newline
                value[strcspn(value, "\n")] = '\0';
                strcpy(directory, value);
                // Check if the directory exists
                struct stat sb;
                if (stat(value, &sb) == 0) {
                    // Exists - check if it is a directory
                    if (S_ISDIR(sb.st_mode)) {
                        // It is a directory
                        continue;
                    }
                    else {
                        // It is a file
                        fclose(config);
                        return 3;
                    }
                }
                // Otherwise create the directory
                else {
                    if (mkdir(value, 0700) == -1) {
                         // Error if it cannot be created
                         fclose(config);
                         return 3;
                    }
                }
            }
            else if (strcmp(label, "max_peers") == 0) {
                *max_peers = atoi(value);
                // Check for valid value
                if (*max_peers < 1 || *max_peers > 2048) {
                    fclose(config);
                    return 4;
                }
            }
            else if (strcmp(label, "port") == 0) {
                int x = atoi(value);
                if (x <= 1024 || x > 65535) {
                    fclose(config);
                    return 5;
                }
                *port = atoi(value); 
            }
        }
        else {
            // Error
            fclose(config);
            return -1;
        }
    }
    // Success
    fclose(config);
    if (line_count == 3) {
        return 0;
    }
    return -1;
    
}