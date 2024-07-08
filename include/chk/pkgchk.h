#ifndef PKGCHK_H
#define PKGCHK_H

#include <stddef.h>
#include <stdint.h>
#include <tree/merkletree.h>

#define IDENT_LENGTH 1025
#define FILE_NAME_LENGTH 257


/**
 * Query object, allows you to assign
 * hash strings to it.
 * Typically: malloc N number of strings for hashes
 *    after malloc the space for each string
 *    Make sure you deallocate in the destroy function
 */
struct bpkg_query {
	char** hashes;
	size_t len;
};

struct chunk {
	char hash[SHA256_HEXLEN];
	uint32_t offset;
	uint32_t size;
	struct merkle_tree_node* chunk_node;
};

/*
 * Bpkg object, holds a merkle tree with the respective data
*/
struct bpkg_obj {
	// .bpgk file properties
    char ident[IDENT_LENGTH];
    char filename[FILE_NAME_LENGTH];
	uint32_t size;
	uint32_t nhashes; // 2ˆ(h-1)-1 (Internal nodes)
	char** hashes;
	uint32_t nchunks; // 2ˆ(h-1) (Leaf nodes)
	struct chunk* chunks;
	// Merkle tree that represents this bpkg file
	struct merkle_tree* tree;
};

/**
 * Loads the package for when a value path is given
 */
struct bpkg_obj* bpkg_load(const char* path);

/**
 * Checks to see if the referenced filename in the bpkg file
 * exists or not.
 * @param bpkg, constructed bpkg object
 * @return query_result, a single string should be
 *      printable in hashes with len sized to 1.
 * 		If the file exists, hashes[0] should contain "File Exists"
 *		If the file does not exist, hashes[0] should contain "File Created"
 */
struct bpkg_query bpkg_file_check(struct bpkg_obj* bpkg);

/**
 * Retrieves a list of all hashes within the package/tree
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_all_hashes(struct bpkg_obj* bpkg);

/**
 * Retrieves all completed chunks of a package object
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_completed_chunks(struct bpkg_obj* bpkg);


/**
 * Gets the mininum of hashes to represented the current completion state
 * Example: If chunks representing start to mid have been completed but
 * 	mid to end have not been, then we will have (N_CHUNKS/2) + 1 hashes
 * 	outputted
 *
 * @param bpkg, constructed bpkg object
 * @return query_result, This structure will contain a list of hashes
 * 		and the number of hashes that have been retrieved
 */
struct bpkg_query bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg); 


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
														 char* hash);


/**
 * Deallocates the query result after it has been constructed from
 * the relevant queries above.
 */
void bpkg_query_destroy(struct bpkg_query* qry);

/**
 * Deallocates memory at the end of the program,
 * make sure it has been completely deallocated
 */
void bpkg_obj_destroy(struct bpkg_obj* obj);

/*
 * Function that parses the data values in the bpkg file
 */
int parse_data(const char* line, char* label, char* data);

/*
 * Creates a node for a merkle tree with an expected hash value
 */
struct merkle_tree_node* create_new_node(const char* hash, int is_leaf);

/*
 * Build a merkle tree with the values of a given bpkg obj and returns it
 */
struct merkle_tree* build_merkle_tree(struct bpkg_obj *obj);

/*
 * Returns a specific hash from a tree given the root
*/
struct merkle_tree_node* find_hash(struct merkle_tree_node *current, 
									char *hash);

#endif

