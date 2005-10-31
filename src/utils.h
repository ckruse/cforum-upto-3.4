/**
 * \file utils.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Utilities for the Classic Forum
 *
 * This file contains some utility functions for the Classic Forum, e.g. a string abstraction and a
 * cf_split() function.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _CF_UTILS_H
#define _CF_UTILS_H

#include <time.h>
#include <sys/types.h>
#include <curl/curl.h>

#include "hashlib.h"

/* {{{ Memory abstraction */
/**
 * \defgroup memory_funcs Memory pool abstraction
 */
/*\@{*/

/**
 * \ingroup memory_funcs
 * This is the heart of the memory abstraction. It contains the memory area itself, the length
 * of the memory used and the length of the area itself
 */
typedef struct s_mem_pool {
  unsigned long len; /**< length of the memory used */
  unsigned long reserved; /**< length of the memory area itself */
  unsigned growth; /**< The growth factor */
  void *content; /**< The memory area itself */
} cf_mem_pool_t;

#define MEMORY_INITIALIZER { 0, 0, 0, NULL }

/**
 * \ingroup memory_funcs
 * This function initializes a memory pool
 * \param pool The memory pool object pointer
 */
void cf_mem_init(cf_mem_pool_t *pool);

/**
 * \ingroup memory_funcs
 * This function initializes a memory pool
 * \param pool The memory pool object pointer
 * \param growth The growth factor
 */
void cf_mem_init_growth(cf_mem_pool_t *pool,unsigned growth);

/**
 * \ingroup memory_funcs
 * This function cleans up a memory pool.
 * \param pool The memory pool object pointer
 */
void cf_mem_cleanup(cf_mem_pool_t *pool);

/**
 * \ingroup memory_funcs
 * This function sets a memory pool content.
 * \param pool The memory pool object pointer
 * \param src The source pointer
 * \param length The length of the region to copy
 * \return Number of copied bytes
 */
size_t cf_mem_set(cf_mem_pool_t *pool,const void *src,size_t length);

/**
 * \ingroup memory_funcs
 * This function appends a memory region to the region in the
 * pool.
 * \param pool The memory pool
 * \param src The source pointer
 * \param len The length of the region
 * \return Number of bytes copied
 */
void *cf_mem_append(cf_mem_pool_t *pool,const void *src,size_t len);
/*\@}*/

/* }}} */

/* {{{ String abstraction */
/**
 * \defgroup string_funcs String abstraction
 */
/*\@{*/

/**
 * \ingroup string_funcs
 * This is the heart of the string abstraction. It contains the string itself, the length
 * of the string and the size of the reserved memory for the string.
 */
typedef struct s_string {
  unsigned long len; /**< length of the memory used */
  unsigned long reserved; /**< length of the memory area itself */
  unsigned growth;
  u_char *content; /**< The memory area itself */
} cf_string_t;

#define CF_STRING_INITIALIZER { 0, 0, CF_BUFSIZ, NULL }

/**
 * \ingroup string_funcs
 * This function initializes a string structure with specified growth factor.
 * \param str A reference to a string structure
 * \param growth The growth factor
 */
void cf_str_init_growth(cf_string_t *str,unsigned growth);

/**
 * \ingroup string_funcs
 * This function initializes a string structure. It just sets everything to 0
 * \param str A reference to a string structure
 */
void cf_str_init(cf_string_t *str);

/**
 * \ingroup string_funcs
 * This function frees the reserved memory in a string structure and sets everything to NULL
 * \param str A reference to the string structure
 */
void cf_str_cleanup(cf_string_t *str);

/**
 * \ingroup string_funcs
 * This function appends a character to the string
 * \param str A reference to the string structure
 * \param content The character to append
 * \return Number of characters appended on success or 0 on failure
 */
size_t cf_str_char_append(cf_string_t *str,const u_char content);

/**
 * \ingroup string_funcs
 * This function appends a u_char array to the string in the string structure.
 * \param str A reference to the string structure
 * \param content The u_char array to append
 * \param length The length of the u_char array
 * \return The number of characters appended on success or 0 on failure
 */
size_t cf_str_chars_append(cf_string_t *str,const u_char *content,size_t length);

/**
 * \ingroup string_funcs
 * This function appends a string structure to a string structure. It's just a wrapper for
 * the cf_str_chars_append() function.
 * \param str A reference to the string structure to append to
 * \param content A reference to the string structure to append
 */
size_t cf_str_str_append(cf_string_t *str,cf_string_t *content);

/**
 * \ingroup string_funcs
 * This function appends a C-like null terminated character array to a string structure.
 * It's just a wrapper for the cf_str_chars_append() function.
 * \param str A reference to the string structure to append to
 * \param content The u_char array to append
 * \return The number of characters appended on success or 0 on failure
 */
size_t cf_str_cstr_append(cf_string_t *str,const u_char *content);

/**
 * \ingroup string_funcs
 * This function sets the value of an string structure to a given u_char array. The old string contained
 * in the structure will be lost. It gets the length of the u_char array by strlen().
 * \param str A reference to the string to append to
 * \param content The string to set
 * \return The number of characters set on success or 0 on failure
 */
int cf_str_cstr_set(cf_string_t *str,const u_char *content);

/**
 * \ingroup string_funcs
 * This function sets the value of an string structure to a given u_char array. The old string contained
 * in the structure will be lost.
 * \param str A reference to the string to append to
 * \param content The string to set
 * \param length The length of the string to set
 * \return The number of characters set on success or 0 on failure
 */
size_t cf_str_char_set(cf_string_t *str,const u_char *content,size_t length);

/**
 * \ingroup string_funcs
 * This function sets the value of a string structure to the value of another string structure. The old
 * string contained in the target structure will be lost.
 * \param str A reference to the structure to set to
 * \param content A reference to the structure to set
 * \return The number of characters set on success or 0 on failure
 */
size_t cf_str_str_set(cf_string_t *str,cf_string_t *content);

/**
 * \ingroup string_funcs
 * This function tests if two strings (cf_string_t) are equal
 * \param str1 string 1
 * \param str2 string 2
 * \return TRUE if both equal, FALSE otherwise
 */
int cf_str_equal_string(const cf_string_t *str1,const cf_string_t *str2);

/**
 * \ingroup string_funcs
 * This function tests if two strings (cf_string_t, u_char *) are equal
 * \param str1 string 1
 * \param str2 string 2
 * \param len Length of string str2
 * \return TRUE if both equal, FALSE otherwise
 */
int cf_str_equal_chars(const cf_string_t *str1,const u_char *str2, size_t len);

/*\@}*/
/* }}} */

/* {{{ Utility functions */

/**
 * This function tries to remove a directory recursively
 * \param path The path to remove
 * \return 0 on success, -1 on error
 */
int cf_remove_recursive(const u_char *path);

/**
 * This function tries to create the whole path from the beginning of the
 * parameter given, e.g. cf_make_path("/abc/def/",0755) tries to create /abc
 * and after that /abc/def
 * \param path The path to create
 * \param mode The mode to create with (think about umask!)
 * \return 0 on success, -1 on error
 */
int cf_make_path(const u_char *path,mode_t mode);

/**
 * Splits a string into a list of strings and returns the length of the list. The string big will be cut at
 * every match of the string small.
 * \param big The string to cf_split
 * \param small The string to search
 * \param ulist A reference to a u_char ** pointer. In this u_char ** pointer will the list be stored.
 * \return The length of the list.
 * \attention You HAVE to free every element of the list and the list itself!
 */
size_t cf_split(const u_char *big,const u_char *small,u_char ***ulist);

/**
 * Splits a string into a list of maximal max elements
 * \param bug The string to cf_split
 * \param small The string to search
 * \param ulist A reference to a u_char ** pointer
 * \return The length of the list
 * \attention You have to free every element of the list _and_ the list itself!
 */
size_t cf_nsplit(const u_char *big,const u_char *small,u_char ***ulist,size_t max);

/**
 * This function tries to convert a date string in the form of "1.1.1982 12:30:40" into a unix timestamp.
 * The time in the date is optional.
 * \param datestr The date string
 * \return The timestamp on success or -1 on failure.
 */
time_t cf_transform_date(const u_char *datestr);

/**
 * This function generates a (hopefully) unique id from the REMOTE_ADDR environment variable and a little
 * bit random.
 * \param buff The buffer to store the id in
 * \param maxlen The maximal length of the id
 * \return The length of the id on success or 0 on failure
 */
int cf_gen_unid(u_char *buff,int maxlen);

/**
 * This function safely allocates new memory. If the memory could not be allocated, exit() will be called.
 * \param ptr The old pointer (for CF_ALLOC_REALLOC)
 * \param nmemb The number of objects to allocate
 * \param size The size of one object
 * \param type The type of the allocation
 * \return The pointer to the new memory segment or NULL, if you gave a wrong type.
 */
void *cf_alloc(void *ptr,size_t nmemb,size_t size,int type);

#define CF_ALLOC_MALLOC  0 /**< Just allocate Memory */
#define CF_ALLOC_CALLOC  1 /**< Allocate memory and initialize it with 0 bytes */
#define CF_ALLOC_REALLOC 2 /**< Re-allocate a memory block with a different size */

#ifdef NOSTRDUP
/**
 * Duplicates a C string and returns the new allocated buffer
 * \param str The string to copy
 * \return The new allocated buffer
 * \attention You have to free() the returned buffer!
 */
u_char *strdup(const u_char *str);
#endif

#ifdef NOSTRNDUP
/**
 * Duplicates a C string of length len and returns the new allocated buffer
 * \param str The string to copy
 * \param len The length of the string
 * \return The new allocated buffer
 * \attention You have to free() the returned buffer!
 */
u_char *strndup(const u_char *str,size_t len);
#endif

#ifdef HAS_NO_GETLINE
/**
 * This function reads a complete line from FILE *stream.
 * \param lineptr The line pointer. If NULL, a new line pointer will be allocated. If not NULL, the buffer will be resized to the right size.
 * \param n The size of the buffer. If lineptr is not NULL, it has to contain the size of lineptr.
 * \param stream The stream pointer
 * \return -1 on failure, the number of bytes read on success
 */
ssize_t getline(char **lineptr,size_t *n,FILE *stream);
#endif

#ifdef HAS_NO_GETDELIM
/**
 * This function reads characters from FILE *stream until int delim has been found.
 * \param lineptr The line pointer. If NULL, a new line pointer will be allocated. If not NULL, the buffer will be resized to the right size.
 * \param n The size of the buffer. If lineptr is not NULL, it has to contain the size of lineptr.
 * \param delim The delimiter character
 * \param stream The file stream pointer
 * \return -1 on failure, the number of bytes read on success
 */
ssize_t getdelim(char **lineptr,size_t *n,int delim,FILE *stream);
#endif

/**
 * This function duplicates a memory segment area
 * \param inptr Pointer to the memory area segment to duplicate
 * \param size The size of the segment
 * \return A pointer to the newly created segment
 * \attention You have to free() the newly created segment yourself!
 */
void *cf_memdup(void *inptr,size_t size);
/* }}} */

/* {{{ Array abstraction */
/**
 * \defgroup cf_array_funcs Array abstraction
 */
/*\@{*/

/**
 * \ingroup cf_array_funcs
 * Array "class".
 * This struct contains all necessary information about the array
 */
typedef struct s_array {
  size_t elements, /**< Number of elements in this array instance */
         reserved, /**< Amount of memory reserved */
         element_size; /**< Size of one element */

  void (*cf_array_destroy)(void *); /**< Function pointer to element destructor */
  void *array; /**< Array pointer */
} cf_array_t;

#define CF_ARRAY_INITIALIZER(size,destroy) { 0, 0, (size), (destroy), NULL }

/**
 * \ingroup cf_array_funcs
 * This function initializes an array structure (it could be the constructor)
 * \param ary The array structure pointer
 * \param element_size The size of one element in the array
 * \param cf_array_destroy The destroy function for an array element. This function will be called whenever an element in the array has to be deleted. Could be NULL if a function like this is not needed and a simple free() call is enough.
 */
void cf_array_init(cf_array_t *ary,size_t element_size,void (*cf_array_destroy)(void *));

/**
 * \ingroup cf_array_funcs
 * This function pushes an element to the end of the array. The element
 * is being copied via a cf_memdup() function, which only is a malloc() with
 * a memcpy().
 * \param ary The array structure pointer
 * \param element The element to push to the end
 */
void cf_array_push(cf_array_t *ary,const void *element);

/**
 * \ingroup cf_array_funcs
 * This function deletes the last element in the array.
 * \param ary The array structure pointer
 * \return A copy of the 'popped' element
 * \attention Because this function returns a copy of this element, the destroy function will not be called!
 */
void *cf_array_pop(cf_array_t *ary);

/**
 * \ingroup cf_array_funcs
 * This function deletes the first element in the array
 * \param ary The array structure pointer
 * \return A copy of the 'shifted' element
 * \attention Because this function returns a copy of this element, the destroy function will not be called!
 */
void *cf_array_shift(cf_array_t *ary);

/**
 * \ingroup cf_array_funcs
 * This function inserts an element at the beginning of the array.
 * \param ary The array structure pointer
 * \param element The pointer to the element
 */
void cf_array_unshift(cf_array_t *ary,const void *element);

/**
 * \ingroup cf_array_funcs
 * This function sorts an array via the quick sort algorithm
 * \param ary The array structure pointer
 * \param compar The comparing function. See qsort(3) for informations of the return values and arguments of this coparing function.
 */
void cf_array_sort(cf_array_t *ary,int(*compar)(const void *,const void *));

/**
 * \ingroup cf_array_funcs
 * This function does a binary search on the array. Has to be sorted first!
 * \param ary The array structure pointer
 + \param key The key to search for
 * \param compar THe comparing function. See bsearch(3) for information of the return values and the arguments of this comparing function.
 * \return Returns NULL if element not found or element if found
 */
void *cf_array_bsearch(cf_array_t *ary,const void *key,int (*compar)(const void *, const void *));

/**
 * \ingroup cf_array_funcs
 * This function returns an element at a specified position.
 * \param ary The array structure pointer
 * \param index The index of the wanted element
 * \return The element at the specified position or NULL on failure
 */
void *cf_array_element_at(cf_array_t *ary,size_t index);

/**
 * \ingroup cf_array_funcs
 * This function destroys an array. It calls the destroy function specified to cf_array_init()
 * for each argument and then free()s the array itself.
 * \param ary The array structure pointer
 */
void cf_array_destroy(cf_array_t *ary);

/*\@}*/

/* }}} */

/* {{{ Tree abstraction */
/**
 * \defgroup tree_funcs Binary tree abstraction (AVL tree)
 */
/*\@{*/

/**
 * \ingroup tree_funcs
 * dataset structure. Used to store data in a tree node
 */
typedef struct s_cf_dataset {
  void *data; /**< Data member */
  void *key; /**< Key member */
} cf_tree_dataset_t;

/**
 * \ingroup tree_funcs
 * balancing types. NONE means balanced, LEFT means
 * left subtree is overbalanced and right means right
 * subtree is overbalanced.
 */
enum e_cf_tree_balance {
  CF_TREE_NONE, /**< balanced */
  CF_TREE_LEFT, /**< Left overbalanced */
  CF_TREE_RIGHT /**< Right overbalanced */
};

/**
 * \ingroup tree_funcs
 *  AVL tree node structure
 */
typedef struct s_cf_tree_node {
  struct s_cf_tree_node *left,  /**< left subtree */
                        *right; /**< right subtree */

  cf_tree_dataset_t *d; /**< Dataset member */

  enum e_cf_tree_balance bal; /**< balance */
} cf_tree_node_t;

/**
 * \ingroup tree_funcs
 * AVL tree structure
 */
typedef struct s_cf_tree {
  /**
   * Comparing function. Has to return -1 if first dataset is smaller
   * than the second, +1 if first dataset is greater than the second
   * and 0 if both are equal.
   */
  int (*compare)(cf_tree_dataset_t *,cf_tree_dataset_t *);

  /**
   * 'Destructor' function for a tree dataset
   */
  void (*destroy)(cf_tree_dataset_t *);

  cf_tree_node_t *root; /**< root node */
} cf_tree_t;

/**
 * \ingroup tree_funcs
 * This function initializes a new tree object
 * \param tree The tree object structure
 * \param compare The comparing function
 * \param destroy The destructor function for tree elements
 */
void cf_tree_init(cf_tree_t *tree,int (*compare)(cf_tree_dataset_t *,cf_tree_dataset_t *),void (*destroy)(cf_tree_dataset_t *));

/**
 * \ingroup tree_funcs
 * This function is used to destroy a tree
 * \param tree The tree object
 */
void cf_tree_destroy(cf_tree_t *tree);

/**
 * \ingroup tree_funcs
 * This function inserts a tree node into a tree with the data of dataset d (d will be copied)
 * \param tree The tree object structure
 * \param n Set always to NULL
 * \param d The dataset structure. Will be copied
 * \return CF_TREE_NONE, CF_TREE_LEFT or CF_TREE_RIGHT
 */
int cf_tree_insert(cf_tree_t *tree,cf_tree_node_t **n, cf_tree_dataset_t *d);

/**
 * \ingroup tree_funcs
 * Removes a node from a tree (expensive!)
 * \param tree The tree object structure
 * \param n Set always to NULL
 * \param key A dataset structure with a valid key set
 * \return CF_TREE_NONE, CF_TREE_LEFT or CF_TREE_RIGHT
 */
int cf_tree_remove(cf_tree_t *tree,cf_tree_node_t **n, cf_tree_dataset_t *key);

/**
 * \ingroup tree_funcs
 * Looks up a dataset
 * \param tree The tree object structure
 * \param n Set to tree->root
 * \param key A dataset structure with a valid key
 * \return NULL if element could not be found, element reference if element has been found
 */
const cf_tree_dataset_t *cf_tree_find(cf_tree_t *tree,cf_tree_node_t *n, cf_tree_dataset_t *key);

/*\@}*/

/* }}} */

/* {{{ list abstraction methods */
/**
 * \defgroup list_funcs List abstraction
 */
/*\@{*/


/**
 * This structure defines an element of the doubly linked list
 * \ingroup list_funcs
 */
typedef struct s_list_element {
  void *data; /**< saved data */
  size_t size; /**< size of the data field */
  int type;

  struct s_list_element *prev; /**< pointer to the previous element */
  struct s_list_element *next; /**< pointer to the next element */
} cf_list_element_t;

/**
 * This structure defines the header for a doubly linked list
 * \ingroup list_funcs
 */
typedef struct s_lishead_t {
  cf_list_element_t *elements; /**< list elements pointer */
  cf_list_element_t *last; /**< pointer to the last element */
} cf_list_head_t;


/**
 * This function initializes a list head
 * \param head The list header variable
 */
void cf_list_init(cf_list_head_t *head);

/**
 * This function appends an element to a list
 * \param head The list header variable for this list
 * \param data The data to append
 * \param size The size of the data
 */
void cf_list_append(cf_list_head_t *head,void *data,size_t size);

/**
 * This function appends an element to a list _and_does_not_copy_it_ but
 * safes the data argument as a reference
 * \param head The list header variable for this list
 * \param data The data to append
 * \param size The size of the data
 */

void cf_list_append_static(cf_list_head_t *head,void *data,size_t size);

/**
 * This function prepends an element to a list
 * \param head The list header variable for this list
 * \param data The data to prepend
 * \param size The size of the data
 */
void cf_list_prepend(cf_list_head_t *head,void *data,size_t size);

/**
 * This function prepends an element to a list _and_does_not_copy_it
_
 * but safes the data argument as a reference
 * \param head The list header variable for this list
 * \param data The data to prepend
 * \param size The size of the data
 */

void cf_list_prepend_static(cf_list_head_t *head,void *data,size_t size);

/**
 * This function inserts an element in a list after the given element
 * \param head The list header variable for this list
 * \param prev The list element variable to insert after
 * \param data The data to insert
 * \param size The size of the data
 */
void cf_list_insert(cf_list_head_t *head,cf_list_element_t *prev,void *data,size_t size);

/**
 * This function searches an element in a list
 * \param head The list header variable for this list
 * \param data The data to search
 * \param compare The comparing function
 * \return Returns the data of the element if found or NULL if not found
 */
void *cf_list_search(cf_list_head_t *head,void *data,int (*compare)(const void *data1,const void *data2));

/**
 * This function deletes an element from a list
 * \param head The list header variable for this list
 * \param elem The list element to delete
 */
void cf_list_delete(cf_list_head_t *head,cf_list_element_t *elem);

/**
 * This function destroys a list
 * \param head The list header variable for this list
 * \param destroy A destroying function for the list elements (NULL if not needed)
 */
void cf_list_destroy(cf_list_head_t *head,void (*destroy)(void *data));

/*\@}*/

/* }}} */

/* {{{ IPC functions */
/**
 * \defgroup ipc_funcs IPC functions
 */
/*\@{*/

/**
 * This function openes two pipes and executes a program
 * \param filename The filename of the program to execute
 * \param argv Its parameter list
 * \param envp Its environment
 * \param result A pointer to the resulting int[2] structure where the
 *               file descriptors are to be stored
 * \param res_pid A pointer where the resulting pid is to be stored or NULL
 * \return 0 on success, -1 on error
 */
int cf_ipc_dpopen(const char *filename,char *const argv[],char *const envp[],int *result,pid_t *res_pid);

/**
 * This function closes two pipes opened by ipc_dpopen() and waits for the process
 * to terminate
 * \param pipes A pointer to the int[2] structure where the
 *              file descriptors are stored or NULL
 * \param pid   A pointer where the pid is stored or NULL
 * \return 0 on success, -1 on error
 */
int cf_ipc_dpclose(int *pipes,pid_t *pid);

/*\@}*/

/* }}} */

/* {{{ String comparison functions */
/**
 * Fast implementation of strcmp()
 * \param str1 First string
 * \param str2 Second string
 * \return 0 if strings are equal, 1 if strings are not equal
 */
int cf_strcmp(const u_char *str1,const u_char *str2);

/**
 * Fast implementation of strncmp()
 * \param str1 First string
 * \param str2 Second string
 * \param n Number of chars to compare
 * \return 0 if substrings are equal, 1 if not equal
 */
int cf_strncmp(const u_char *str1,const u_char *str2,size_t n);

/**
 * Fast implementation of strcasecmp()
 * \param str1 First string
 * \param str2 Second string
 * \return 0 if strings are equal (case insensitive), 1 if not equal
 */
int cf_strcasecmp(const u_char *str1,const u_char *str2);

/**
 * Fast implementation of strncasecmp()
 * \param str1 First string
 * \param str2 Second string
 * \param n Number of chars to compare
 * \return 0 if substrings are equal, 1 if not equal
 */
int cf_strncasecmp(const u_char *str1,const u_char *str2,size_t n);

/**
 * Counts characters (not bytes!!) in an utf8 string
 * \param str The string to count
 * \param rlen The real length of the memory area
 * \return -1 on failure (e.g. EILSEQ), length on success
 */
size_t cf_strlen_utf8(const u_char *str,size_t rlen);

/**
 * Counts characters (not bytes!!) in an utf8 string but without spaces
 * \param str The string to count
 * \param rlen The real length of the memory area
 * \return -1 on failure (e.g. EILSEQ), length on success
 */
size_t cf_strlen_utf8_wo_space(const u_char *str,size_t rlen);

/**
 * Checks if the character the unicode number given by argument refers to is a space character
 * \param num The unicode number of the character
 * \return 1 if it is a space character, 0 if it is not
 */
int cf_isspace(u_int32_t num);

/* }}} */

/* {{{ caching functions */

/**
 * used for giving the user a cache entry
 */
typedef struct {
  void *ptr;   /**< Pointer to the content */
  size_t size; /**< Size of the segment */
  int fd;      /**< file descriptor */
} cf_cache_entry_t;

/**
 * function to check if a cache entry is outdated
 * \param base The base directory
 * \param uri The URI of the entry
 * \param file The file to compare
 * \return -1 on failure (e.g. cache file is outdated), 0 on success
 */
int cf_cache_outdated(const u_char *base,const u_char *uri,const u_char *file);

/**
 * This function checks if a cache entry is outdated by a given date
 * \param base The base directory
 * \param uri The URI of the entry
 * \param date The outdated date
 * \return -1 on failure (e.g. cache file is outdated), 0 on success
 */
int cf_cache_outdated_date(const u_char *base,const u_char *uri,time_t date);

/**
 * Function creates or updates a cache entry
 * \param base The base directory
 * \param uri The URI of the entry
 * \param content The content of the entry
 * \param len The length of the content segment
 * \param gzip Should the cache be compressed with gzip? If > 0, it'll be compressed with this compression level
 * \return 0 on success, -1 on error
 */
int cf_cache(const u_char *base,const u_char *uri,const u_char *content,size_t len,int gzip);

/**
 * Function used to get a cache entry
 * \param base The base directory
 * \param uri The URI of the entry
 * \param gzip If > 0, it expects the content to be encoded with gzip
 * \return NULL on error, a cache entry on success
 */
cf_cache_entry_t *cf_get_cache(u_char *base,u_char *uri,int gzip);

/**
 * Function cleaning up a cf_cache_entry_t struct
 * \param ent The entry to clean up
 */
void cf_cache_destroy(cf_cache_entry_t *ent);

/* }}} */

/* {{{ data utilities */
/**
 * This function converts an u_int64_t to a cf_string_t (using cf_str_char_append calls).
 * \param str The string to use
 * \param num The u_int64_t to convert
 */
void cf_uint64_to_str(cf_string_t *str, u_int64_t num);

/**
 * This function converts an u_int16_t to a cf_string_t (using cf_str_char_append calls).
 * \param str The string to use
 * \param num The u_int16_t to convert
 */
void cf_uint16_to_str(cf_string_t *str, u_int16_t num);

/**
 * This function converts an u_int32_t to a cf_string_t (using cf_str_char_append calls).
 * \param str The string to use
 * \param num The u_int32_t to convert
 */
void cf_uint32_to_str(cf_string_t *str, u_int32_t num);

/**
 * This function converts an u_char * to an u_int64_t
 * \param ptr The u_char * to convert
 * \return The converted number
 */
u_int64_t cf_str_to_uint64(register const u_char *ptr);


/* }}} */

/* {{{ HTTP utilities */
typedef size_t (*cf_http_request_callback_t)(void *,size_t,size_t,void *);

typedef struct {
  float version;
  int status;
  u_char *reason;

  cf_hash_t *headers;
  cf_string_t content;
} cf_http_response_t;

#define CF_HTTP_TYPE_GET  1
#define CF_HTTP_TYPE_POST 2
#define CF_HTTP_TYPE_HEAD 3
typedef struct {
  u_char *uri;
  int follow,type;
  long resume;

  u_char *user_pass,*proxy_user_pass,*proxy,*referer,*ua,*cookies,*custom_rq;
  void *rqdata;

  cf_string_t post_data;

  struct curl_slist *custom_headers;
  cf_http_response_t *rsp;

  CURL *handle;

  cf_http_request_callback_t data_callback,header_callback;

  int _donttouch;
} cf_http_request_t;

int cf_http_complex_request(cf_http_request_t *rq);
cf_http_response_t *cf_http_simple_head_uri(const u_char *uri);
cf_http_response_t *cf_http_simple_get_uri(const u_char *uri,time_t lm);
cf_http_response_t *cf_http_simple_post_uri(const u_char *uri,const u_char *postdata,size_t len);
void cf_http_destroy_response(cf_http_response_t *rsp);

void cf_http_redirect_with_nice_uri(const u_char *ruri,int perm);
/* }}} */

time_t cf_timegm(struct tm *tm);


#endif

/* eof */

