/**
 * \file hashlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief the header definitions for the hashing library
 *
 * The original hashing algorithm has been developed by Bob Jenkins. I made a hash table library around
 * it.
 * A hash table is something like an array, but its keys are strings, not numbers. I create an ordanary array
 * which is the hashtable. If you save an entry in it, a hash sum will be calculated and this hash sum
 * says in which array element this entry has to be saved.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __HASHLIB_H
#define __HASHLIB_H

typedef           u_int32_t  ub4;   /**< unsigned 4-byte quantities */
typedef           u_char     ub1;   /**< unified character format */

/**
 * Is the starting hash table size (2^9)
 */
#define CF_HASH_SIZE 9

/**
 * If we get more than CF_HASH_MAX_DOUBLES double hash sums, we increase the
 * size of the hash sums and the hash table. But this is really
 * expensive...
 */
#define CF_HASH_MAX_DOUBLES 5

/**
 * This is a destructor function type. This function type is used to destroy hash elements.
 */
typedef void (*t_cf_hash_cleanup)(void *);

/**
 * this struct is used to store a hashtable entry.
 */
typedef struct s_cf_hashentry {
  unsigned char *key; /**< the hashtable key */
  size_t keylen; /**< the length of the key */
  int stat; /**< shall we call a destructor for this entry? */

  void *data; /**< the data */

  ub4 hashval; /**< the index in the hashtable (not shortened) */

  /**
   * This is the pointer to the next value. Because we cannot avoid double hash values, we have a
   * linked list of entries.
   */
  struct s_cf_hashentry *next;

  /**
   * This is the pointer to the previous value. Because we cannot avoid double hash values, we have a
   * linked list of entries.
   */
  struct s_cf_hashentry *prev;
} t_cf_hash_entry;

/**
 *
 * this struct is used to store the infos about a hashtable instance.
 */
typedef struct s_cf_hash {
  /**
   * The higher the tablesize, the smaller the possibility of
   * a double hash value. This goes on the cost of memory... a pointer to
   * a t_cf_hashentry type has 4 byte, so 512 entries have 2048 bytes (2kb!).
   * This is very big... so be patiant!
   */
  ub4 tablesize;

  /**
   * For element cleanups we need a callback function. This callback function
   * has to cleanup the structure we saved.
   */
  t_cf_hash_cleanup destroy;

  /**
   * We save only pointers because in each element we have a
   * linked list of elements
   */
  t_cf_hash_entry **table;
} t_cf_hash;

/**
 * this macro is used to make shorten indexes easier. It calculates the number of entries at the given table size
 */
#define hashsize(n) ((ub4)1<<(n))

/**
 * this macro is used to calculate the number of bytes used for keys in the actual table size.
 */
#define hashmask(n) (hashsize(n)-1)

/**
 *  This function is used to generate a hash sum from a given key.
 * \param k      The key to calculate the hashsum from
 * \param length The length of the key
 * \param level  The entropy level. It is used to mix a little bit random into the keys. Not used by this hash table library.
 * \return the hash sum
 */
ub4 lookup(register ub1 *k, register ub4 length, register ub4 level);

/**
 * This function constructs a new hash. It expects a pointer to a
 * destructor function. This function will be called when a hash
 * entry will be destroyed. This can happen if you call
 * 'cf_hash_entry_delete' or if you call 'cf_hash_destroy'. You MUST NOT
 * free the data object itself! This will be done internaly. You
 * only should cleanup the structure if it is a complex data structure
 * like a struct or an array of arrays.
 * \param cl The destructor for a data entry of the hash table. If the function pointer is NULL, no cleanup function will be called.
 * \return It returns the new created hash table object on success or NULL on failure
 */
t_cf_hash *cf_hash_new(t_cf_hash_cleanup cl);

/**
 * This function saves a hash entry with the given key in the hash
 * table. It can happen that the table has to be resized, but this
 * should not happen very often (there's one double hashvalue in a
 * 32 bit keylen and we accept 5 double hashvalues per entry). But
 * *when* it happens, this call is very expensive...
 * \param hsh     The hash table
 * \param key     The key of the entry
 * \param keylen  The length of the key
 * \param data    The data of the entry
 * \param datalen The size of the data
 * \return 0 on failure, 1 if successful
 */
int cf_hash_set(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data,size_t datalen);

/**
 * This function saves a hash entry with the given key in the hash
 * table. It can happen that the table has to be resized, but this
 * should not happen very often (there's one double hashvalue in a
 * 32 bit keylen and we accept 5 double hashvalues per entry). But
 * *when* it happens, this call is very expensive...
 * The difference between cf_hash_set() is, that cf_hash_set() makes
 * a copy from the data and cf_hash_set_static() assumes static data
 * which needs not to be copied and needs no free() call
 * \param hsh     The hash table
 * \param key     The key of the entry
 * \param keylen  The length of the key
 * \param data    The data of the entry
 * \return 0 on failure, 1 if successful
 */
int cf_hash_set_static(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data);

/**
 * This function looks up a hash entry in a hash table. If an entry with
 * the key could not be found, NULL is returned.
 * \param hsh The hash table
 * \param key The key of the entry
 * \param keylen The length of the key
 * \return The data of the entry if it has been found or NULL if it could not be found.
 */
void *cf_hash_get(t_cf_hash *hsh,unsigned char *key,size_t keylen);

/**
 * This function deletes a hash entry in a hashtable.
 * \param hsh The hash table
 * \param key The key of the entry
 * \param keylen The length of the key
 * \return It returns 0 if the entry could not be found and 1 if the entry has been deleted successfully.
 */
int cf_hash_entry_delete(t_cf_hash *hsh,unsigned char *key,size_t keylen);

/**
 * This function destroys a hash and frees all of its values.
 * After this function call you have to create a new hash with
 * cf_hash_new()!
 * \param hsh The hash table to destroy.
 */
void cf_hash_destroy(t_cf_hash *hsh);

#endif
