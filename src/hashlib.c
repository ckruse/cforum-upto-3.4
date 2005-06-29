/**
 * \file hashlib.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief the implementation of the hashing library
 *
 * The hashing library was developed within the Classic Forum project, <http://cforum.teamone.de/>
 *
 * I use a hashing algorithm developed by Bob Jenkins, December 1996.
 * This is (directly after the SDBM algorithm) the fastest hashing
 * algorithm at this time, but the SDBM algorithm has more duplicates.
 * The Fowler/Noll/Vo algorithm has no duplicates, but it is also the
 * slowest of these three algorithms. So the Bob-Jenkins algorithm
 * should be a moderate compromiss.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stddef.h>

#include "hashlib.h"
/* }}} */

#ifndef DOXYGEN
/******************************* HASHING ALGORITHM, by Bob Jenkins *******************************/

/*
 * This code has been developed by Bob Jenkins.
 */

/* {{{ #define mix
 * mix -- mix 3 32-bit values reversibly.
 * For every delta with one or two bit set, and the deltas of all three
 * high bits or all three low bits, whether the original value of a,b,c
 * is almost all zero or is uniformly distributed,
 *
 * - If mix() is run forward or backward, at least 32 bits in a,b,c
 *   have at least 1/4 probability of changing.
 * - If mix() is run forward, every bit of c will change between 1/3 and
 *   2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *
 * mix() was built out of 36 single-cycle latency instructions in a 
 * structure that could supported 2x parallelism, like so:
 *     a -= b; 
 *     a -= c; x = (c>>13);
 *     b -= c; a ^= x;
 *     b -= a; x = (a<<8);
 *     c -= a; b ^= x;
 *     c -= b; x = (b>>13);
 *     ...
 * Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
 * of that parallelism.  They've also turned some of those single-cycle
 * latency instructions into multi-cycle latency instructions.  Still,
 * this is the fastest good hash I could find.  There were about 2^^68
 * to choose from.  I only looked at a billion or so.
 */
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}
/* }}} */
#endif

/* {{{ lookup()
 * lookup() -- hash a variable-length key into a 32-bit value
 *
 * k     : the key (the unaligned variable-length array of bytes)
 * len   : the length of the key, counting by bytes
 * level : can be any 4-byte value
 *
 * Returns a 32-bit value.  Every bit of the key affects every bit of
 * the return value.  Every 1-bit and 2-bit delta achieves avalanche.
 * About 6len+35 instructions.

 * The best hashtable sizes are powers of 2.  There is no need to do
 * mod a prime (mod is sooo slow!).  If you need less than 32 bits,
 * use a bitmask.  For example, if you need only 10 bits, do
 *
 * h = (h & hashmask(10));
 *
 * In which case, the hash table should have hashsize(10) elements.
 *
 * If you are hashing n strings (ub1 **)k, do it like this:
 *  for (i=0, h=0; i<n; ++i) h = lookup( k[i], len[i], h);
 *
 * By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *
 * See http://burtleburtle.net/bob/hash/evahash.html
 * Use for hash table lookup, or anything where one collision in 2^32 is
 * acceptable.  Do NOT use for cryptographic purposes.
 */
ub4 lookup(register ub1 *k,register ub4 length,register ub4 level) {
  register ub4 a,b,c,len;

  /* Set up the internal state */
  len = length;
  a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
  c = level;           /* the previous hash value */

  /*---------------------------------------- handle most of the key */
  while (len >= 12) {
    a += (k[0] +((ub4)k[1]<<8) +((ub4)k[2]<<16) +((ub4)k[3]<<24));
    b += (k[4] +((ub4)k[5]<<8) +((ub4)k[6]<<16) +((ub4)k[7]<<24));
    c += (k[8] +((ub4)k[9]<<8) +((ub4)k[10]<<16)+((ub4)k[11]<<24));
    mix(a,b,c);
    k += 12; len -= 12;
  }

  /*------------------------------------- handle the last 11 bytes */
  c += length;
  switch(len) {            /* all the case statements fall through */
    case 11: c+=((ub4)k[10]<<24);
    case 10: c+=((ub4)k[9]<<16);
    case 9 : c+=((ub4)k[8]<<8);
      /* the first byte of c is reserved for the length */
    case 8 : b+=((ub4)k[7]<<24);
    case 7 : b+=((ub4)k[6]<<16);
    case 6 : b+=((ub4)k[5]<<8);
    case 5 : b+=k[4];
    case 4 : a+=((ub4)k[3]<<24);
    case 3 : a+=((ub4)k[2]<<16);
    case 2 : a+=((ub4)k[1]<<8);
    case 1 : a+=k[0];
      /* case 0: nothing left to add */
  }
  mix(a,b,c);
  /*-------------------------------------------- report the result */
  return c;
}
/* }}} */

/******************************* HASHING ALGORITHM END *************************************/

/*
 * at this point the hashlib code begins ((c) by CK)
 */

/* {{{ cf_hash_new()
 * Returns:                the new hashtable object
 * Parameters:
 *   - t_cf_hash_cleanup   a destructor function for the entry data
 *
 * This function constructs a new hash. It expects a pointer to a
 * destructor function. This function will be called when a hash
 * entry will be destroyed. This can happen if you call
 * 'cf_hash_delete' or if you call 'cf_hash_destroy'. You MUST NOT
 * free the data object itself! This will be done internaly. You
 * only should cleanup the structure if it is a complex data structure
 * like a struct or an array of arrays.
 *
 * If the function pointer is NULL, no cleanup function will be called.
 *
 */
t_cf_hash *cf_hash_new(t_cf_hash_cleanup cl) {
  t_cf_hash *hsh = malloc(sizeof(t_cf_hash));
  ub4 elems = hashsize(CF_HASH_SIZE);

  if(!hsh) return NULL;

  hsh->elements = 0;

  /*
   * the tablesize is 2^10 (1024 entries) since we don't blame on double
   * values. This should be moderate for normal use. If not, increase
   * this value in the header file. The higher the tablesize, the lower the
   * possibility of a double hash value. But caution! This value will be powered
   * with the base of 2. So if you increase it to 11, you get a size of 2048.
   * If you increase it to 15, you get a size of 32768 and so on. We use 2 as
   * the base, because this hashing function works fastest with with exponents
   * of two. Modulo seems to be very slow...
   */
  hsh->tablesize = CF_HASH_SIZE;

  /*
   * this *sucks*. It costs a lot of time, but we need it because
   * we have to know if an element is set or not
   */
  hsh->table     = calloc(elems,sizeof(t_cf_hash_entry **));

  hsh->destroy   = cl;

  hsh->keys.elems = hsh->keys.last = NULL;

  return hsh;
}
/* }}} */

#ifndef DOXYGEN
/* {{{ _cf_hash_save()
 * Returns:                the data of the hash entry if found or NULL
 * Parameters:
 *   - t_cf_hash *hsh      the hash object
 *   - unsigned char *key  the key
 *   - size_t keylen       the length of the key
 *
 * This function looks up a hash entry in a hash table. If an entry with
 * the key has not been found NULL is returned.
 *
 * This function is private!
 */
t_cf_hash_entry *_cf_hash_save(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data,size_t datalen,ub4 hashval) {
  t_cf_hash_entry *ent = malloc(sizeof(t_cf_hash_entry));
  t_cf_hash_keylist *akt = NULL;

  if(!ent) return NULL;

  /*
   * we save the complete hashval because with this value
   * we can examine faster if a key exists in this hash
   * table
   */
  ent->hashval     = hashval;

  /*
   * the key of the entry has to be a 0 terminated unsigned char *
   */
  ent->key         = malloc((keylen + 1) * sizeof(unsigned char));
  if(!ent->key) return NULL;

  memcpy(ent->key,key,keylen);
  ent->key[keylen] = 0;
  ent->keylen      = keylen;

  /*
   * save the data
   */
  if(datalen) {
    ent->data        = malloc(datalen);
    ent->stat        = 0;
    if(!ent->data) return NULL;

    memcpy(ent->data,data,datalen);
  }
  else {
    ent->stat = 1;
    ent->data = data;
  }

  ent->prev        = NULL;
  ent->next        = NULL;

  if(hsh->keys.last == NULL) {
    if((akt = hsh->keys.last = hsh->keys.elems = malloc(sizeof(*hsh->keys.last))) == NULL) return NULL;
  }
  else {
    if((akt = hsh->keys.last->next = malloc(sizeof(*hsh->keys.last))) == NULL) return NULL;
  }

  akt->key  = ent->key;
  akt->next = NULL;

  /* are we at the first element? */
  if(hsh->keys.last != akt) {
    akt->prev = hsh->keys.last;
    hsh->keys.last = akt;
  }
  else akt->prev = NULL;

  ent->keyelem = akt;

  return ent;
}
/* }}} */

/* {{{ _cf_hash_split()
 * Returns:                nothing
 * Parameters:
 *   - t_cf_hash *hsh      the hash object
 *   - unsigned char *key  the key
 *   - size_t keylen       the length of the key
 *   - void *data          the data object to save
 *   - size_t datalen      the size of the data object to save
 *   - ub4 hval            the hash value of the key
 *
 * This function reallocates and restructures a hashtable. Very
 * expensive, but not called very often.
 *
 * This function is private!
 *
 */
void _cf_hash_split(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data,size_t datalen,ub4 hval) {
  ub4 elems,oelems,i,hval_short;
  t_cf_hash_entry *elem,*elem1;

  oelems = hashsize(hsh->tablesize);
  elems  = hashsize(++(hsh->tablesize));

  /*
   * first, we have to reallocate the hashtable. This is expensive enough...
   */
  hsh->table = realloc(hsh->table,elems * sizeof(t_cf_hash_entry **));

  /*
   * But this is not enough. We also have to set the second
   * half to NULL *grr* this is *very* expensive.
   */
  memset(&hsh->table[oelems],0,oelems * sizeof(t_cf_hash_entry **));

  /*
   * But as if this is not enough, we have to re-structure the complete
   * hashtable...
   */
  for(i=0;i<oelems;i++) {
    /*
     * empty elements are uninteresting
     */
    if(!hsh->table[i]) continue;

    /*
     * We need to check each element in an hash entry...
     */
    for(elem=hsh->table[i];elem;elem=elem1) {
      elem1 = elem->next;

      /*
       * Is the element at the right position? If yes, we can
       * continue with the next element.
       */
      if((elem->hashval & (elems - 1)) != i) {
        /*
         * Is this element the first entry? If no, we have to
         * knock out the actual element and if yes, we have to
         * set it to the next element (may even be NULL (aka end of list))
         */
        if(elem->prev) {
          elem->prev->next = elem->next;
        }
        else {
          hsh->table[i] = elem->next;
        }

        /*
         * has this element a next value? If yes, we have
         * to set the prev-pointer of the next element to
         * the prev-pointer of the actual element
         */
        if(elem->next) {
          elem->next->prev = elem->prev;
        }

        /*
         * And now: prepend the actual hash entry
         */
        elem->next = hsh->table[elem->hashval & (elems - 1)];
        elem->prev = NULL;
        hsh->table[elem->hashval & (elems - 1)] = elem;

        if(elem->next) elem->next->prev = elem;
      }
    }
  }

  /*
   * last, save the given element to the hash table. This time, we
   * don't check the CF_HASH_MAX_DOUBLES, because it would take a
   * *lot* of time to restructure the hash table
   */
  hval_short = hval & (elems - 1);
  if(hsh->table[hval_short]) {
    for(elem1=hsh->table[hval_short];elem1->next;elem1=elem1->next);
    elem1->next        = _cf_hash_save(hsh,key,keylen,data,datalen,hval);
    elem1->next->prev  = elem1;
  }
  else {
    hsh->table[hval_short] = _cf_hash_save(hsh,key,keylen,data,datalen,hval);
  }

}
/* }}} */

#endif

/* {{{ cf_hash_set */
int cf_hash_set(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data,size_t datalen) {
  ub4 hval,hval_short;
  t_cf_hash_entry *ent,*prev;

  /*
   * generate the hash value
   */
  hval       = lookup(key,keylen,0);

  /*
   * because we need no 32 bit hash values (a hashtable of
   * a size of 32 bit is definitely a *very* to big!)
   */
  hval_short = hval & hashmask(hsh->tablesize);

  ent = hsh->table[hval_short];

  if(ent) {
    for(prev=NULL;ent;ent=ent->next) {
      /*
       * We got a double value, so we have to free the actual value
       */
      if(ent->hashval == hval) {
        if(ent->keylen == keylen && memcmp(ent->key,key,keylen) == 0) {
          if(ent->stat == 0) {
            if(hsh->destroy) hsh->destroy(ent->data);
            free(ent->data);
          }

          ent->data = malloc(datalen);
          if(!ent->data) return 0;
          memcpy(ent->data,data,datalen);

          return 1;
        }
      }

      prev = ent;
    }

    /*
     * phew... I *really* hope this case happens not very often.
     */
    if(hsh->elements >= hashsize(hsh->tablesize)) _cf_hash_split(hsh,key,keylen,data,datalen,hval);
    else {
      prev->next       = _cf_hash_save(hsh,key,keylen,data,datalen,hval);
      prev->next->prev = prev;
      hsh->elements++;
    }

    return 1;
  }
  else {
    hsh->table[hval_short] = _cf_hash_save(hsh,key,keylen,data,datalen,hval);
    return 1;
  }

  return 0;
}
/* }}} */

/* {{{ cf_hash_set_static */
int cf_hash_set_static(t_cf_hash *hsh,unsigned char *key,size_t keylen,void *data) {
  ub4 hval,hval_short;
  t_cf_hash_entry *ent,*prev;

  /*
   * generate the hash value
   */
  hval       = lookup(key,keylen,0);

  /*
   * because we need no 32 bit hash values (a hashtable of
   * a size of 32 bit is definitely a *very* to big!)
   */
  hval_short = hval & hashmask(hsh->tablesize);

  ent = hsh->table[hval_short];

  if(ent) {
    for(prev=NULL;ent;ent=ent->next) {
      /*
       * We got a double value, so we have to free the actual value
       */
      if(ent->hashval == hval) {
        if(ent->keylen == keylen && memcmp(ent->key,key,keylen) == 0) {
          if(ent->stat == 0) {
            if(hsh->destroy) hsh->destroy(ent->data);
            free(ent->data);
          }

          ent->data = data;
          ent->stat = 1;

          return 1;
        }
      }

      prev = ent;
    }

    /*
     * phew... I *really* hope this case happens not very often.
     */
    if(hsh->elements >= hashsize(hsh->tablesize)) _cf_hash_split(hsh,key,keylen,data,0,hval);
    else {
      prev->next       = _cf_hash_save(hsh,key,keylen,data,0,hval);
      prev->next->prev = prev;
      hsh->elements++;
    }

    return 1;
  }
  else {
    hsh->table[hval_short] = _cf_hash_save(hsh,key,keylen,data,0,hval);
    return 1;
  }

  return 0;
}
/* }}} */

/* {{{ cf_hash_get */
void *cf_hash_get(t_cf_hash *hsh,unsigned char *key,size_t keylen) {
  ub4 hval,hval_short;
  t_cf_hash_entry *ent;

  hval       = lookup(key,keylen,0);
  hval_short = hval & hashmask(hsh->tablesize);

  if(hsh->table[hval_short]) {
    for(ent = hsh->table[hval_short];ent && (ent->hashval != hval || ent->keylen != keylen || memcmp(ent->key,key,keylen) != 0);ent=ent->next);

    if(ent) return ent->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_hash_entry_delete */
int cf_hash_entry_delete(t_cf_hash *hsh,unsigned char *key,size_t keylen) {
  ub4 hval,hval_short;
  t_cf_hash_entry *ent;

  hval       = lookup(key,keylen,0);
  hval_short = hval & hashmask(hsh->tablesize);

  if(hsh->table[hval_short]) {
    for(ent = hsh->table[hval_short];ent && (ent->hashval != hval || ent->keylen != keylen || memcmp(ent->key,key,keylen) != 0);ent=ent->next);

    if(ent) {
      if(ent->stat == 0) {
        if(hsh->destroy) hsh->destroy(ent->data);
        free(ent->data);
      }
      free(ent->key);

      if(ent->prev) ent->prev->next = ent->next;
      else hsh->table[hval_short] = ent->next;

      if(ent->next) ent->next->prev = ent->prev;

      if(ent->keyelem->next) ent->keyelem->next->prev = ent->keyelem->prev;
      if(ent->keyelem->prev) ent->keyelem->prev->next = ent->keyelem->next;

      free(ent->keyelem);

      free(ent);
      hsh->elements--;

      return 1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ cf_hash_destroy() */
void cf_hash_destroy(t_cf_hash *hsh) {
  t_cf_hash_keylist *key,*key1;
  ub4 hval,hval_short;
  t_cf_hash_entry *ent;
  size_t keylen;

  for(key=hsh->keys.elems;key;key=key1) {
    keylen     = strlen(key->key);
    hval       = lookup(key->key,keylen,0);
    hval_short = hval & hashmask(hsh->tablesize);

    key1 = key->next;

    if(hsh->table[hval_short]) {
      for(ent = hsh->table[hval_short];ent && (ent->hashval != hval || ent->keylen != keylen || memcmp(ent->key,key->key,keylen) != 0);ent=ent->next);

      if(ent) {
        if(ent->stat == 0) {
          if(hsh->destroy) hsh->destroy(ent->data);
          free(ent->data);
        }
        free(ent->key);

        if(ent->prev) ent->prev->next = ent->next;
        else hsh->table[hval_short] = ent->next;

        if(ent->next) ent->next->prev = ent->prev;

        if(ent->keyelem->next) ent->keyelem->next->prev = ent->keyelem->prev;
        if(ent->keyelem->prev) ent->keyelem->prev->next = ent->keyelem->next;

        free(ent->keyelem);

        free(ent);
        hsh->elements--;
      }
    }
  }

  free(hsh->table);
  free(hsh);
}
/* }}} */

/*
 * this is a small example
 *
 *

int main(void) {
  t_cf_hash *hsh = cf_hash_new(NULL);
  int i = 0,len;
  unsigned char buff[50];

  for(i=0;i<34000;i++) {
    len = snprintf(buff,50,"key%d",i);
    cf_hash_set(hsh,buff,len,buff,len+1);
  }

  cf_hash_entry_delete(hsh,"key50",5);
  cf_hash_entry_delete(hsh,"key51",5);

  for(i=0;i<34000;i++) {
    len = snprintf(buff,50,"key%d",i);
    cf_hash_get(hsh,buff,len);
  }

  cf_hash_destroy(hsh);

  return 0;
}
*/

/* eof */
