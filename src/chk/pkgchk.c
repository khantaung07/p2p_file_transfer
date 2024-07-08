#define _XOPEN_SOURCE 700 

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <chk/pkgchk.h>
#include <crypt/sha256.h>

#define MAX_VARIABLE_NAME_LENGTH 10

// PART 1

/*
 * Function that parses the data values in the bpkg file
 */
int parse_data(const char* line, char* label, char* data) {
    return sscanf(line, "%[^:]:%[^\n]", label, data);
}

/**
 * Loads the package for when a valid path is given
 */
struct bpkg_obj* bpkg_load(const char* path) {
    // Check for valid path
    FILE *bpkg_file = fopen(path, "r");

    if (bpkg_file == NULL) {
        // .bpkg file does not exist; return NULL
        return NULL; 
    }

    // Create new bpkg_obj and tree
    struct bpkg_obj *obj = calloc(1, sizeof(struct bpkg_obj));

    // Read the file to fill the fields and build the merkle tree
    char label[MAX_VARIABLE_NAME_LENGTH] = {0};
    char data[IDENT_LENGTH] = {0};
    char line[MAX_VARIABLE_NAME_LENGTH+IDENT_LENGTH] = {0};
    
    while (fgets(line, sizeof(line), bpkg_file) != NULL) {
        // Clear previous data
        memset(label, 0, sizeof(label));
        memset(data, 0, sizeof(data));

        // Strip newline character
        line[strcspn(line, "\n")] = '\0';

        if(parse_data(line, label, data) == 2) {
            // Store data in the bpkg obj
            if (strcmp(label, "ident") == 0) {
                strcpy(obj->ident, data);
            }
            else if (strcmp(label, "filename") == 0) {
                strcpy(obj->filename, data);
            }
            else if (strcmp(label, "size") == 0) {
                obj->size = strtol(data, NULL, 10);
            }
            else if (strcmp(label, "nhashes") == 0) {
                obj->nhashes = strtol(data, NULL, 10);

                int nhashes = obj->nhashes;

                // Allocate memory to store hashes in the obj
                // Allocate memory for pointers
                obj->hashes = malloc(sizeof(char*)*nhashes);

                // Allocate memory for each hash
                for (int i = 0; i < nhashes; i++) {
                    // Extra character for null terminator
                    obj->hashes[i] = calloc((SHA256_HEXLEN)+1, sizeof(char));
                }

                // Check for "hashes:"
                fgets(line, sizeof(line), bpkg_file);

                if (strcmp(line, "hashes:\n") != 0) {
                    // Invalid label
                    bpkg_obj_destroy(obj);
                    fclose(bpkg_file);
                    return NULL;
                }

                // Parse hashes
                int count = 0;

                while (count < nhashes) {
                    if (fgets(line, sizeof(line), bpkg_file) != NULL) {
                        // Strip new line character and tab space
                        char *hash_ptr = line;
                        hash_ptr++;
                        hash_ptr[strcspn(hash_ptr, "\n")] = '\0';
                        // Copy the hash to the object
                        strncpy(obj->hashes[count], hash_ptr, 64); 
                        count++;
                    }
                    else {
                        // Error encountered: line was null before hash count
                        bpkg_obj_destroy(obj);
                        fclose(bpkg_file);
                        return NULL;
                    } 
                }
            }
            else if (strcmp(label, "nchunks") == 0) {
                obj->nchunks = strtol(data, NULL, 10);

                int nchunks = obj->nchunks;
                
                // Allocate memory for the chunks
                obj->chunks = malloc(sizeof(struct chunk)*nchunks);

                // Check for "hashes:"
                fgets(line, sizeof(line), bpkg_file);

                if (strcmp(line, "chunks:\n") != 0) {
                    // Invalid label
                    bpkg_obj_destroy(obj);
                    fclose(bpkg_file);
                    return NULL;
                }

                // Parse chunks
                int count = 0;

                while (count < nchunks) {
                    if (fgets(line, sizeof(line), bpkg_file) != NULL) {
                        // Strip tab space 
                        // sscanf does not accept newline into %u (no stripping)
                        char *chunk_ptr = line;
                        chunk_ptr++;
                        // Fill the chunk object with the data
                        sscanf(chunk_ptr, "%64[^,],%u,%u", 
                        obj->chunks[count].hash, 
                        &obj->chunks[count].offset, 
                        &obj->chunks[count].size);
                        obj->chunks[count].chunk_node = NULL;
                        count++;
                    }
                    else {
                        // Error encountered: line was null before chunk count
                        bpkg_obj_destroy(obj);
                        fclose(bpkg_file);
                        return NULL;
                    } 
                }
            }
        }
        else {
            // Did not match any of the labels: invalid .bpkg file
            bpkg_obj_destroy(obj);
            fclose(bpkg_file);
            return NULL;
        }
    }
    // Check if the object was created successfully 
    if (obj == NULL ||
        obj->ident[0] == '\0' ||
        obj->filename[0] == '\0' ||
        obj->hashes == NULL ||
        obj->chunks == NULL) {
        bpkg_obj_destroy(obj);
        fclose(bpkg_file);
        return NULL;
    } 

    // Close the file
    fclose(bpkg_file);

    // Construct merkle tree
    obj->tree = build_merkle_tree(obj);

    return obj;
}

/*
 * Creates a node for a merkle tree with an expected hash value
 */
struct merkle_tree_node* create_new_node(const char* hash, int is_leaf) {
    struct merkle_tree_node *node = malloc(sizeof(struct merkle_tree_node));
    // Copy expected hash value
    strncpy(node->expected_hash, hash, 64);
    node->key = NULL;
    node->value = NULL;
    node->left = NULL;
    node->right = NULL;
    node->is_leaf = is_leaf;
    return node;
}

/*
 * Build a merkle tree with the values of a given bpkg obj and returns it
 */
struct merkle_tree* build_merkle_tree(struct bpkg_obj *obj) {
    // Allocate memory for new tree
    struct merkle_tree *tree = malloc(sizeof(struct merkle_tree));
    tree->n_nodes = obj->nchunks + obj->nhashes;
    
    // Build tree in order
    struct merkle_tree_node *root = {0};

    // Assign root
    if (obj->nhashes > 0) {
        root = create_new_node(obj->hashes[0], 0);
        tree->root = root;
    }
    /* Special case where there are no hashes
       Therefore only one chunk (and it should be the root) */
    else {
        root = create_new_node(obj->chunks[0].hash, 1);
        tree->root = root;
        // Return early since the tree/chunk is complete
        return tree;
    }
    

    // Level-order traversal of the hashes

    // Temporary array of nodes to store the levels of the tree
    struct merkle_tree_node **layers = \
    malloc(sizeof(struct merkle_tree_node*) * obj->nhashes);

    layers[0] = root;
    int current_node = 1; 


    // Build tree with hashes
    for (int i = 1; i < obj->nhashes; i++) {
        // Calculate parent of the current node
        // Formula: floor((current node - 1)/2)
        struct merkle_tree_node *parent = layers[(i - 1) / 2];
        // Create new node
        struct merkle_tree_node *node = create_new_node(obj->hashes[i],
         0);

        // Connect the current node to its parent
        // If odd, it is a left child, otherwise it is a right child
        if (i % 2 != 0) {
            parent->left = node;
        } else {
            parent->right = node;
        }
        layers[current_node] = node;
        current_node++;
    }

    // Finish building the tree with chunks
    int current_chunk = current_node;

    for (int i = 0; i < obj->nchunks; i++) {
        struct merkle_tree_node *parent = layers[(current_chunk - 1) / 2];
        // Create new leaf node
        struct merkle_tree_node *node = create_new_node(obj->chunks[i].hash, 1);

        // Assign the node to the chunk
        obj->chunks[i].chunk_node = node;
        
        // Connect the current node to its parent
        if (current_chunk % 2 != 0) {
            parent->left = node;
        } else {
            parent->right = node;
        }
        current_chunk++;
    }

    // Free temporary array
    free(layers);

    return tree;
}

/**
 * Checks to see if the referenced filename in the bpkg file
 * exists or not.
 * @param bpkg, constructed bpkg object
 * @return query_result, a single string should be
 *      printable in hashes with len sized to 1.
 * 		If the file exists, hashes[0] should contain "File Exists"
 *		If the file does not exist, hashes[0] should contain "File Created"
 */
struct bpkg_query bpkg_file_check(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = { 0 };
    qry.hashes = malloc(sizeof(char*));
    qry.len = 1;

    // Check if the file exists
    FILE *file = fopen(bpkg->filename, "r");

    if (file == NULL) {
        // Create new file and edit hashes
        qry.hashes[0] = malloc(13);
        strcpy(qry.hashes[0], "File Created");

        file = fopen(bpkg->filename, "w");
        // Get the file descriptor
        int fd = fileno(file);
        // Set the size
        ftruncate(fd, bpkg->size);
        fclose(file);
    }
    else {
        qry.hashes[0] = malloc(12);
        strcpy(qry.hashes[0], "File Exists");
        fclose(file);
    }
    
    return qry;
}

/**
 * Retrieves a list of all hashes within the package/tree
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_all_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = { 0 };

    // Calculate number of total hashes
    int total_hashes = bpkg->nhashes + bpkg->nchunks;
    qry.hashes = malloc(sizeof(char*) * total_hashes);
    qry.len = total_hashes;

    // Internal nodes (hashes)
    for (int i = 0; i < bpkg->nhashes; i++) {
        qry.hashes[i] = malloc(SHA256_HEXLEN + 1);
        strncpy(qry.hashes[i], bpkg->hashes[i], SHA256_HEXLEN + 1);
    }

    // Leaf nodes (chunks)
    for (int i = 0; i < bpkg->nchunks; i++) {
        qry.hashes[bpkg->nhashes + i] = malloc(SHA256_HEXLEN + 1);
        strncpy(qry.hashes[bpkg->nhashes + i], 
        bpkg->chunks[i].hash, SHA256_HEXLEN + 1);
    }
    
    return qry;
}

/**
 * Retrieves all completed chunks of a package object
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_completed_chunks(struct bpkg_obj* bpkg) { 
    struct bpkg_query qry = { 0 };

    // Check if the file exists
    FILE *file = fopen(bpkg->filename, "rb");

    if (file == NULL) {
        return qry;
    }

    // Allocate memory for pointers to hashes
    // Note: there may be extra null pointers if incomplete hashes exist
    // These are cleaned up in the destroy function
    qry.hashes = malloc(sizeof(char*)*bpkg->nchunks);

    // For each chunk, compare the expected hash with the actual hashed data
    // If it is correct, add it to the query object
    int count = 0;

    for (int i = 0; i < bpkg->nchunks; i++) {
        // Go to offset
        if (fseek(file, bpkg->chunks[i].offset, SEEK_SET) != 0) {
            // Some error encountered e.g. EOF
            // No other chunks will be correct
            fclose(file);
            return qry;
        }

        int chunk_size = bpkg->chunks[i].size;

        // Allocate memory for a buffer to store the data
        char* buffer = malloc(chunk_size);
        
        // Read from the file (byte by byte)
        if (fread(buffer, 1, chunk_size, file)
         != chunk_size) {
            // Did not read full chunk amount
            fclose(file);
            return qry;
        }

        // Compute the SHA256 value of this data
        struct sha256_compute_data cdata = { 0 };
        char final_hash[65] = { 0 };
        sha256_compute_data_init(&cdata);
        sha256_update(&cdata, buffer, chunk_size);
        sha256_finalize(&cdata, NULL);
        sha256_output_hex(&cdata, final_hash);

        /* Compare the expected and computed hash values
           If computed hash matches expected, add it to the query */
        if (strncmp(final_hash, bpkg->chunks[i].hash, 64) == 0) {
            qry.hashes[count] = malloc(SHA256_HEXLEN);
            strncpy(qry.hashes[count], final_hash, 64);
            count++;
        }

        // Free the temporary buffer
        free(buffer);
    }  

    qry.len = count;
    fclose(file);

    return qry;
}

/*
 * Function that computes the hashes of all non-leaf nodes in a tree
 */
void bpkg_compute_hashes(struct merkle_tree_node *current) {
    // Some error encountered
    if (current == NULL) {
        return;
    }

    /* Don't need to calculate for leaf nodes
      Hash is calculated from the data file */
    if (current->is_leaf == 1) {
        return;
    }

    bpkg_compute_hashes(current->left);
    bpkg_compute_hashes(current->right);

    // Concatenate the child hashes
    char combined_hash[2 * SHA256_HEXLEN + 1] = {0};

    strncpy(combined_hash, current->left->computed_hash, 64);
    strncat(combined_hash, current->right->computed_hash, 64);

    // Compute the SHA256 value of this concatenated string
    struct sha256_compute_data cdata = { 0 };
    char final_hash[65] = { 0 };
    sha256_compute_data_init(&cdata);
    sha256_update(&cdata, combined_hash, 2 * SHA256_HEXLEN);
    sha256_finalize(&cdata, NULL);
    sha256_output_hex(&cdata, final_hash);

    // Store the final hash
    strncpy(current->computed_hash, final_hash, 64);

    return;

}

struct bpkg_query bpkg_get_min_completed_hashes_helper(
    struct merkle_tree_node* current) {
    struct bpkg_query qry = {0};

    /* This is the case where a chunk is incomplete and recursion has brought 
     * the function to the child of the chunk (NULL)
     */
    if (current == NULL) {
        return qry;
    }
    
    // The current node is complete
    // Therefore return it
    if (strncmp(current->expected_hash, current->computed_hash, 64) == 0) {
        qry.hashes = malloc(sizeof(char*));
        qry.hashes[0] = malloc(SHA256_HEXLEN);
        strncpy(qry.hashes[0], current->expected_hash, 64);
        qry.len = 1;
        return qry;
    }
    // The current node is incomplete
    // Return left and right subtrees seperately 
    struct bpkg_query left_qry = bpkg_get_min_completed_hashes_helper(
        current->left);
    struct bpkg_query right_qry = bpkg_get_min_completed_hashes_helper(
        current->right);
    
    // Merge query object of left and right subtrees
    qry.len = left_qry.len + right_qry.len;
    qry.hashes = malloc(sizeof(char*) * qry.len);

    int current_index = 0;

    for (int i = 0; i < left_qry.len; i++) {
        qry.hashes[current_index] = left_qry.hashes[i];
        current_index++;
    }
    for (int i = 0; i < right_qry.len; i++) {
        qry.hashes[current_index] = right_qry.hashes[i];
        current_index++;
    }

    // Free the char* pointer
    free(left_qry.hashes);
    free(right_qry.hashes);

    return qry;

}


/**
 * Gets only the required/min hashes to represent the current completion state
 * Return the smallest set of hashes of completed branches to represent
 * the completion state of the file.
 *
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = { 0 };
    
    // Check if the file exists
    FILE *file = fopen(bpkg->filename, "rb");

    if (file == NULL) {
        return qry;
    }

    // For each chunk, calculate the computed hash value
    for (int i = 0; i < bpkg->nchunks; i++) {
        // Go to offset
        if (fseek(file, bpkg->chunks[i].offset, SEEK_SET) != 0) {
            // Some error encountered e.g. EOF
            // No other chunks will be correct
            fclose(file);
            return qry;
        }

        int chunk_size = bpkg->chunks[i].size;

        // Allocate memory for a buffer to store the data
        char* buffer = malloc(chunk_size);
        
        // Read from the file (byte by byte)
        if (fread(buffer, 1, chunk_size, file)
         != chunk_size) {
            // Did not read full chunk amount
            fclose(file);
            free(buffer);
            return qry;
        }

        // Compute the SHA256 value of this data
        struct sha256_compute_data cdata = { 0 };
        char final_hash[65] = { 0 };
        sha256_compute_data_init(&cdata);
        sha256_update(&cdata, buffer, chunk_size);
        sha256_finalize(&cdata, NULL);
        sha256_output_hex(&cdata, final_hash);

        // Store the computed hash in the node
        strncpy(bpkg->chunks[i].chunk_node->computed_hash, final_hash, 64);

        // Free the temporary buffer
        free(buffer);
    }  

    fclose(file);

    // Traverse the tree and calculate the hash value for the non-leaf nodes
    bpkg_compute_hashes(bpkg->tree->root);

    // Get the min completed hashes
    qry = bpkg_get_min_completed_hashes_helper(bpkg->tree->root);

    return qry;
}


/* 
 * Function that recursively searches tree for a specified hash
 */
struct merkle_tree_node* find_hash(struct merkle_tree_node *current, char *hash)
{   
    // Base case: hash not present
    if (current == NULL) {
        return NULL; 
    }

    if (strncmp(current->expected_hash, hash, 64) == 0) {
        return current;
    }

    // Search left subtree recursively
    struct merkle_tree_node *target = find_hash(current->left, hash);
    if (target != NULL) {
        return target;
    }

    // Search right subtree recursively
    return find_hash(current->right, hash);
}


/*
 * Function that recursively gets all children from a specified hash's subtree
 * The chunks are stored in the bpkg query object and is returned
*/
struct bpkg_query bpkg_get_all_chunk_hashes_from_hash_helper(
    struct merkle_tree_node* current) {
    struct bpkg_query qry = {0};

    // Base case (in case of some error)
    if (current == NULL) {
        return qry;
    }

    // Base case: If a leaf is reached return the query object
    if (current->is_leaf) {
        qry.hashes = malloc(sizeof(char*));
        qry.hashes[0] = malloc(SHA256_HEXLEN);
        strncpy(qry.hashes[0], current->expected_hash, 64);
        qry.len = 1;
        return qry;
    }

    // Merge query object of left and right subtrees
    struct bpkg_query left_qry = bpkg_get_all_chunk_hashes_from_hash_helper(
        current->left);
    struct bpkg_query right_qry = bpkg_get_all_chunk_hashes_from_hash_helper(
        current->right);

    qry.len = left_qry.len + right_qry.len;
    qry.hashes = malloc(sizeof(char*) * qry.len);

    int current_index = 0;

    for (int i = 0; i < left_qry.len; i++) {
        qry.hashes[current_index] = left_qry.hashes[i];
        current_index++;
    }
    for (int i = 0; i < right_qry.len; i++) {
        qry.hashes[current_index] = right_qry.hashes[i];
        current_index++;
    }

    // Free the char* pointer
    free(left_qry.hashes);
    free(right_qry.hashes);

    return qry;
}

/**
 * Retrieves all chunk hashes given a certain an ancestor hash (or itself)
 * Example: If the root hash was given, all chunk hashes will be outputted
 * 	If the root's left child hash was given, all chunks corresponding to
 * 	the first half of the file will be outputted
 * 	If the root's right child hash was given, all chunks corresponding to
 * 	the second half of the file will be outputted
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_all_chunk_hashes_from_hash(struct bpkg_obj* bpkg, 
    char* hash) {
    struct bpkg_query qry = { 0 };

    if (bpkg == NULL || hash == NULL) {
        return qry;
    }

    // Find the specified hash
    struct merkle_tree_node* target = find_hash(bpkg->tree->root, hash);

    // Hash does not exist
    if (target == NULL) {
        return qry;
    }

    // Get all leaf nodes from this hash
    qry = bpkg_get_all_chunk_hashes_from_hash_helper(target);

    return qry;
}

/**
 * Deallocates the query result after it has been constructed from
 * the relevant queries above.
 */
void bpkg_query_destroy(struct bpkg_query* qry) {
    if (qry == NULL) {
        return; 
    }

    if (qry->hashes != NULL) {
        // Free the hashes
        for (int i = 0; i < qry->len; i++) {
            if (qry->hashes[i] != NULL) {
                free(qry->hashes[i]);
            }
        }
        // Free the array of pointers
        free(qry->hashes);
    }

    return;
}

/* 
 * Recursively deallocates memory for a merkle tree given a root
 */
void bpkg_tree_destroy(struct merkle_tree_node* root) {
    // Base case
    if (root == NULL) {
        return;
    }

    // Free left and right subtrees
    bpkg_tree_destroy(root->left);
    bpkg_tree_destroy(root->right);
    

    // Free the node's memory
    if (root->key != NULL) {
        free(root->key);
    }
    if (root->value != NULL) {
        free(root->value);
    }
    free(root);
}

/**
 * Deallocates memory at the end of the program,
 * make sure it has been completely deallocated
 */
void bpkg_obj_destroy(struct bpkg_obj* obj) {
    
    if (obj == NULL) {
        return;
    }

    // Free the hashes
    if (obj->hashes != NULL) {
        // Free the hash strings
        for (int i = 0; i < obj->nhashes; i++) {
            free(obj->hashes[i]);
        }
        // Free the array of pointers
        free(obj->hashes);
    }

    // Free the chunks
    free(obj->chunks);

    // Free the merkle tree
    if (obj->tree != NULL) {
        bpkg_tree_destroy(obj->tree->root);
        free(obj->tree);
    }
    
    free(obj);

}

