/**
 * \file utils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief utilities for the Classic Forum
 *
 * These utilities are written for the Classic Forum. Hope, they're useful.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ memdup
 * Returns:  A pointer to the duplicated memory area
 * Parameters:
 *   - void *inptr  The pointer to the original memory area
 *   - size_t size  The size of the memory area
 *
 * This function duplicates a memory area
 *
 */
void *memdup(void *inptr,size_t size) {
  void *outptr = fo_alloc(NULL,1,size,FO_ALLOC_MALLOC);
  memcpy(outptr,inptr,size);

  return outptr;
}
/* }}} */

/* {{{ fo_alloc
 * Returns:     NULL on false type attribute, the new pointer in any else case
 * Parameters:
 *   - void *ptr     The old pointer for realloc()
 *   - size_t nmemb  The number of objects to allocate
 *   - size_t size   The size of one object
 *   - int type      The type of the allocation (FO_ALLOC_MALLOC, FO_ALLOC_CALLOC or FO_ALLOC_REALLOC)
 *
 * Safely allocates new memory
 */
void *fo_alloc(void *ptr,size_t nmemb,size_t size,int type) {
  void *l_ptr = NULL;

  switch(type) {
  case FO_ALLOC_MALLOC:
    l_ptr = malloc(nmemb * size);
    break;
  case FO_ALLOC_CALLOC:
    l_ptr = calloc(nmemb,size);
    break;
  case FO_ALLOC_REALLOC:
    l_ptr = realloc(ptr,size * nmemb);
    break;
  }

  if(!l_ptr) {
    perror("error allocating memory");
    exit(EXIT_FAILURE);
  }

  return l_ptr;
}
/* }}} */

/* {{{ split
 * Returns: long       the length of the list
 * Parameters:
 *   - const u_char *big     the string to split
 *   - const u_char *small   the string where to split
 *   - u_char ***list        the list
 *
 * This function splits a string into pieces
 *
 */
size_t split(const u_char *big,const u_char *small,u_char ***ulist) {
  u_char **list  = fo_alloc(NULL,PRERESERVE,sizeof(*list),FO_ALLOC_MALLOC);
  u_char  *pos   = (u_char *)big,*pre = (u_char *)big;
  size_t len   = 0;
  size_t reser = PRERESERVE;
  size_t slen  = strlen(small);

  while((pos = strstr(pos,small)) != NULL) {
    *pos = '\0';

    list[len++] = strdup(pre);

    if(len >= reser) {
      reser += PRERESERVE;
      list = fo_alloc(list,reser,sizeof(*list),FO_ALLOC_REALLOC);
    }

    pre    = pos+slen;
    *pos++ = *small;
  }

  if(len >= reser) {
    list = fo_alloc(list,++reser,sizeof(*list),FO_ALLOC_REALLOC);
  }
  list[len++] = strdup(pre);

  list   = fo_alloc(list,len,sizeof(*list),FO_ALLOC_REALLOC);
  *ulist = list;

  return len;
}
/* }}} */

/* {{{ mem_init */
void mem_init(t_mem_pool *pool) {
  pool->len      = 0;
  pool->reserved = 0;
  pool->content  = NULL;
}
/* }}} */

/* {{{ str_init
 * Returns: void   nothing
 * Parameters:
 *   - t_string *str        the string to append on
 *
 * this function initializes a string structure
 *
 */
void str_init(t_string *str) {
  str->len      = 0;
  str->reserved = 0;
  str->content  = NULL;
}
/* }}} */

/* {{{ mem_cleanup */
void mem_cleanup(t_mem_pool *pool) {
  pool->len      = 0;
  pool->reserved = 0;

  if(pool->content) free(pool->content);

  pool->content  = NULL;
}
/* }}} */

/* {{{ str_cleanup
 * Returns: void   nothing
 * Parameters:
 *   - t_string *str        the string to append on
 *
 * this function frees mem
 *
 */
void str_cleanup(t_string *str) {
  str->len      = 0;
  str->reserved = 0;

  if(str->content) free(str->content);

  str->content  = NULL;
}
/* }}} */

/* {{{ str_char_append
 * Returns: size_t   the number of chars appended
 * Parameters:
 *   - t_string *str        the string to append on
 *   - const u_char content  the character to append
 *
 * this function appends a const u_char to a t_string
 *
 */
size_t str_char_append(t_string *str,const u_char content) {
  if(str->len + 1 >= str->reserved) {
    str->content   = fo_alloc(str->content,(size_t)(str->reserved + BUFSIZ),1,FO_ALLOC_REALLOC);
    str->reserved += BUFSIZ;
  }

  str->content[str->len] = content;
  str->len              += 1;
  str->content[str->len] = '\0';

  return 1;
}
/* }}} */

/* {{{ str_chars_append
 * Returns: size_t   the number of chars appended
 * Parameters:
 *   - t_string *str        the string to append on
 *   - const u_char *content the string to append
 *   - int length           the length of the string to append
 *
 * this function appends a const u_char * string to a t_string
 *
 */
size_t str_chars_append(t_string *str,const u_char *content,size_t length) {
  size_t len = BUFSIZ;

  if(str->len + length >= str->reserved) {
    if(length >= len) {
      len += length;
    }

    str->content   = fo_alloc(str->content,(size_t)(str->reserved + len),1,FO_ALLOC_REALLOC);
    str->reserved += len;
  }

  memcpy(&str->content[str->len],content,length);
  str->len += length;
  str->content[str->len] = '\0';

  return length;
}
/* }}} */

/* {{{ str_equal_string
 * Returns: TRUE if both are equal, FALSE otherwise
 * Parameters:
 *   - str1 string 1
 *   - str2 string 2
 *
 * This function tests if two strings (t_string) are equal
 */
int str_equal_string(const t_string *str1,const t_string *str2) {
  register u_char *ptr1 = str1->content,*ptr2 = str2->content;
  register size_t i;

  if(str1->len != str2->len) {
    return 1;
  }

  for(i = 0; i < str1->len; ++i,++ptr1,++ptr2) {
    if(*ptr1 != *ptr2) {
      return 1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ str_equal_chars
 * Returns: TRUE if both are equal, FALSE otherwise
 * Parameters:
 *   - str1 string 1
 *   - str2 string 2
 *   - len length of the c_string to be compared
 *
 * This function tests if two strings (t_string) are equal
 */
int str_equal_chars(const t_string *str1,const u_char *str2, size_t len) {
  register size_t i = 0;
  register u_char *ptr1 = str1->content,*ptr2 = (u_char *)str2;

  if(str1->len != len) {
    return 1;
  }

  for(i = 0; i < len; ++i,++ptr1,++ptr2) {
    if(*ptr1 != *ptr2) {
      return 1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ mem_append */
void *mem_append(t_mem_pool *pool,const void *src,size_t length) {
  size_t len = BUFSIZ;

  if(pool->len + length >= pool->reserved) {
    if(length >= len) {
      len += length;
    }

    pool->content   = fo_alloc(pool->content,(size_t)(pool->reserved + len),1,FO_ALLOC_REALLOC);
    pool->reserved += len;
  }

  memcpy(pool->content + pool->len,src,length);
  pool->len += length;

  return pool->content + pool->len - length;
}
/* }}} */

/* {{{ str_str_append
 * Returns: void   nothing
 * Parameters:
 *   - t_string *str        the string to append on
 *   - t_string *content    the string to append
 *
 * this function is a wrapper, it calls str_chars_append
 *
 */
size_t str_str_append(t_string *str,t_string *content) {
  return str_chars_append(str,content->content,content->len);
}
/* }}} */

/* {{{ mem_set */
size_t mem_set(t_mem_pool *pool,const void *src,size_t length) {
  size_t len = MAXLINE;

  if(pool->len + length >= pool->reserved) {
    if(length >= len) {
      len += length;
    }

    pool->content   = fo_alloc(pool->content,pool->reserved + len,1,FO_ALLOC_REALLOC);
    pool->reserved += len;
  }

  memcpy(pool->content,src,length);
  pool->len = length;

  return length;
}
/* }}} */

/* {{{ str_char_set
 * Returns: void   nothing
 * Parameters:
 *   - t_string *str        the string to append on
 *   - const u_char *content the string to append
 *   - int length           the length of the string to append
 *
 * this function copies a const u_char * to a t_string.
 *
 */
size_t str_char_set(t_string *str,const u_char *content,size_t length) {
  size_t len = BUFSIZ;

  if(str->len + length >= str->reserved) {
    if(length >= len) {
      len += length;
    }

    str->content   = fo_alloc(str->content,len,1,FO_ALLOC_REALLOC);
    str->reserved += BUFSIZ;
  }

  memcpy(str->content,content,length);
  str->len = length;
  str->content[length] = '\0';

  return length;
}
/* }}} */

/* {{{ str_str_set
 * Returns: void   nothing
 * Parameters:
 *   - t_string *str        the string to append on
 *   - t_string *content    the string to append
 *
 * this function is a wrapper. it calls str_char_set
 *
 */
size_t str_str_set(t_string *str,t_string *set) {
  return str_char_set(str,set->content,set->len);
}
/* }}} */

/* {{{ transform_date
 * Returns: time_t          the timestamp or (time_t)0
 * Parameters:
 *   - const u_char *datestr the date string
 *
 * This function tries to create a timestamp
 *
 */
time_t transform_date(const u_char *datestr) {
  struct tm t;
  u_char *ptr,*before;                     /* two pointers to work with */
  u_char *str = fo_alloc(NULL,strlen(datestr)+1,1,FO_ALLOC_MALLOC); /* we need a copy of the string (we cannot change a const u_char *) */

  (void)strcpy(str,datestr);
  ptr = before = str;

  memset(&t,0,sizeof(t));        /* initialize the struct tm (set everything to 0) */

  for(;*ptr && *ptr != '.';ptr++);       /* search the first . (for the day) */
  if(*ptr == '.') {                      /* if this is not a dot, we have no valid date */
    *ptr = '\0';                         /* setting this byte to 0, we can use atoi() whithout creating a substring */
    t.tm_mday = atoi(before);            /* fill the mont day (1-31) */
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)-1;                    /* no valid date */
  }

  for(before= ++ptr;*ptr && *ptr != '.';ptr++);        /* search the next dot (for the month) */
  if(*ptr == '.') {
    *ptr = '\0';
    t.tm_mon = atoi(before)-1;                         /* tm_mon contains the mont - 1 (0-11) */
    *ptr = '.';
  }
  else {
    free(str);
    return (time_t)-1;                                  /* no valid date */
  }

  for(before= ++ptr;*ptr && !isspace(*ptr);ptr++);     /* search the '\0' or a whitespace; if a whitespace
                                                        * follows, there are also hours and mins and perhaps seconds */
  if(isspace(*ptr) || *ptr == '\0') {                  /* Is this a a valid entry? */
    t.tm_year = atoi(before) - 1900;                   /* tm_year contains the year - 1900 */
  }
  else {
    free(str);
    return (time_t)-1;                                  /* not a valid entry */
  }

  if(*ptr == ' ') {                                    /* follows an hour and a minute? */
    for(;*ptr && *ptr == ' ';ptr++);                   /* skip trailing whitespaces */

    if(*ptr) {                                         /* have we got a string like "1.1.2001 "? */
      for(before= ++ptr;*ptr && *ptr != ':';ptr++);    /* search the next colon (Hours are seperated from minutes by
                    * colons
              */
      if(*ptr == ':') {
        *ptr = '\0';
        t.tm_hour = atoi(before);                      /* get the hour */
        *ptr = ':';
      }
      else {
        free(str);
        return (time_t)-1;
      }

      for(before= ++ptr;*ptr && *ptr != ':';ptr++);    /* search for the end of the string or another colon */
      if(*ptr == ':' || *ptr == '\0') {
        if(*ptr == ':') {
          *ptr = '\0';
          t.tm_min = atoi(before);                       /* get the minutes */
          *ptr = ':';
        }
        else {
          t.tm_min = atoi(before);
        }
      }
      else {
        free(str);
        return (time_t)-1;
      }

      if(*ptr == ':') {                                /* seconds following */
        before= ptr + 1;                               /* after the seconds, there can only follow the end of
                                                        * the string
                                                        */
        if(!*ptr) return (time_t)0;
        t.tm_sec = atoi(before);
      }
    }
  }

  free(str);
  return mktime(&t);                                   /* finally, try to generate the timestamp */
}
/* }}} */

/* {{{ gen_unid
 * Returns: int             the length of the generated uid or 0
 * Parameters:
 *   - u_char *buff          a pointer to a buffer
 *   - int maxlen           the maximum length of the id
 *
 * This function generates a new unique id
 *
 */
int gen_unid(u_char *buff,int maxlen) {
  int i,l;
  u_char *remaddr = getenv("REMOTE_ADDR");
  static const u_char chars[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789_-";
  struct timeval  tp;

  if(!remaddr) {
    return 0;
  }

  if(gettimeofday(&tp,NULL) != 0) {
    return 0;
  }

  srand(tp.tv_usec);
  l = strlen(remaddr);

  if(maxlen > l) {
    maxlen = l;
  }
  if(maxlen <= l) {
    l = maxlen - 1;
  }

  for(i=0;i<l;i++) {
    buff[i] = chars[remaddr[i] % 59 + (unsigned int)(rand() % 3)];
  }

  return i;
}
/* }}} */

#ifdef HAS_NO_GETLINE
/* {{{ getline
 * Returns: ssize_t         The number of bytes read or -1 on failure
 * Parameters:
 *   - u_char **lineptr      The line pointer
 *   - size_t *n            The number of bytes allocated
 *   - FILE *stream         The stream pointer
 *
 * This function reads a complete line from FILE *stream.
 *
 */
ssize_t getline(char **lineptr,size_t *n,FILE *stream) {
  return getdelim(lineptr,n,'\n',stream);
}
/* }}} */
#endif

#ifdef HAS_NO_GETDELIM
/* {{{ getdelim
 * Returns: ssize_t         The number of bytes read or -1 on failure
 * Parameters:
 *   - u_char **lineptr      The line pointer
 *   - size_t *n            The number of bytes allocated
 *   - int delim            The delimiter character
 *   - FILE *stream         The stream pointer
 *
 * This function reads until 'delim' has been found
 *
 */
ssize_t getdelim(char **lineptr,size_t *n,int delim,FILE *stream) {
  t_string buf;
  register u_char c;

  str_init(&buf);

  while(!feof(stream) && !ferror(stream)) {
    c = fgetc(stream);
    str_char_append(&buf,c);

    if(c == delim) break;
  }

  if(*lineptr) free(*lineptr);

  *lineptr = buf.content;
  *n       = buf.len;

  if(feof(stream) || ferror(stream)) return -1;

  return buf.len;
}
/* }}} */
#endif

#ifdef NOSTRDUP
/* {{{ strdup */
u_char *strdup(const u_char *str) {
  size_t len = strlen(str);
  u_char *buff = fo_alloc(NULL,1,len+1,FO_ALLOC_MALLOC);

  memcpy(buff,str,len+1);

  return buff;
}
/* }}} */
#endif

#ifdef NOSTRNDUP
/* {{{ strndup */
u_char *strndup(const u_char *str,size_t len) {
  u_char *buff = fo_alloc(NULL,1,len+1,FO_ALLOC_MALLOC);

  memcpy(buff,str,len);
  buff[len-1] = '\0';

  return buff;
}
/* }}} */
#endif

/* {{{ Array abstraction */

void array_init(t_array *ary,size_t element_size,void (*array_destroy)(void *)) {
  ary->reserved      = 0;
  ary->elements      = 0;
  ary->element_size  = element_size;
  ary->array         = NULL;
  ary->array_destroy = array_destroy;
}

void array_push(t_array *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = fo_alloc(ary->array,ary->element_size,ary->reserved+1,FO_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memcpy(ary->array + (ary->elements * ary->element_size),element,ary->element_size);
  ary->elements += 1;
}

void *array_pop(t_array *ary) {
  ary->elements -= 1;
  return memdup((void *)(ary->array + ((ary->elements + 1) * ary->element_size)),ary->element_size);
}

void *array_shift(t_array *ary) {
  void *elem = memdup(ary->array,ary->element_size);

  memmove(ary->array,ary->array+ary->element_size,(ary->elements - 1) * ary->element_size);
  ary->elements -= 1;
  return elem;
}

void array_unshift(t_array *ary,const void *element) {
  if(ary->elements + 1 >= ary->reserved) {
    ary->array     = fo_alloc(ary->array,ary->element_size,ary->reserved+1,FO_ALLOC_REALLOC);
    ary->reserved += 1;
  }

  memmove(ary->array+ary->element_size,ary->array,ary->elements  * ary->element_size);
  memcpy(ary->array,element,ary->element_size);
  ary->elements += 1;
}

void array_sort(t_array *ary,int(*compar)(const void *,const void *)) {
  qsort(ary->array,ary->elements,ary->element_size,compar);
}

void *array_bsearch(t_array *ary,const void *key,int (*compar)(const void *, const void *)) {
  return bsearch(key,ary->array,ary->elements,ary->element_size,compar);
}

void *array_element_at(t_array *ary,size_t index) {
  if(index < 0 || index > ary->elements) {
    errno = EINVAL;
    return NULL;
  }

  return ary->array + (index * ary->element_size);
}

void array_destroy(t_array *ary) {
 size_t i;

  if(ary->array_destroy) {
    for(i=0;i<ary->elements;i++) {
      ary->array_destroy(ary->array + (i * ary->element_size));
    }
  }

  free(ary->array);
  memset(ary,0,sizeof(*ary));
}

/* }}} */

/* {{{ Tree abstraction */
/**
 * private function to do left-rotation if tree is
 * unbalanced
 * \param n The subtree root node
 */
void cf_tree_rotate_left(t_cf_tree_node **n) {
  t_cf_tree_node *tmp = *n;

  *n = (*n)->right;
  tmp->right = (*n)->left;
  (*n)->left = tmp;
}

/**
 * private function to do right-rotation if tree
 * is unbalanced
 * \param n The subtree root node
 */
void cf_tree_rotate_right(t_cf_tree_node **n) {
  t_cf_tree_node *tmp = *n;

  *n = (*n)->left;
  tmp->left = (*n)->right;
  (*n)->right = tmp;
}

/**
 * Handle case if tree has grown on the left
 * side
 * \param n The subtree root node
 * \return Returns 0 or 1
 */
int cf_tree_leftgrown(t_cf_tree_node **n) {
  switch((*n)->bal) {
    case CF_TREE_LEFT:
      if((*n)->left->bal == CF_TREE_LEFT) {
        (*n)->bal = (*n)->left->bal = CF_TREE_NONE;

        cf_tree_rotate_right(n);
      }
      else {
        switch((*n)->left->right->bal) {
          case CF_TREE_LEFT:
            (*n)->bal = CF_TREE_RIGHT;
            (*n)->left->bal = CF_TREE_NONE;
            break;

          case CF_TREE_RIGHT:
            (*n)->bal       = CF_TREE_NONE;
            (*n)->left->bal = CF_TREE_LEFT;
            break;

          default:
            (*n)->bal = CF_TREE_NONE;
            (*n)->left->bal = CF_TREE_NONE;
        }

        (*n)->left->right->bal = CF_TREE_NONE;

        cf_tree_rotate_left(&(*n)->left);
        cf_tree_rotate_right(n);
      }

      return 0;

    case CF_TREE_RIGHT:
      (*n)->bal = CF_TREE_NONE;
      return 0;

    default:
      (*n)->bal = CF_TREE_LEFT;
      return 1;
  }
}

/**
 * private function to handle the case that the tree
 * has been grown to the right side
 * \param n Subtree root node
 * \return Returns 0 or 1
 */
int cf_tree_rightgrown(t_cf_tree_node **n) {
  switch((*n)->bal) {
    case CF_TREE_LEFT:
      (*n)->bal = CF_TREE_NONE;
      return 0;

    case CF_TREE_RIGHT:
      if((*n)->right->bal == CF_TREE_RIGHT) {
        (*n)->bal = (*n)->right->bal = CF_TREE_NONE;
        cf_tree_rotate_left(n);
      }
      else {
        switch((*n)->right->left->bal) {
          case CF_TREE_RIGHT:
            (*n)->bal = CF_TREE_LEFT;
            (*n)->right->bal = CF_TREE_NONE;
            break;

          case CF_TREE_LEFT:
            (*n)->bal = CF_TREE_NONE;
            (*n)->right->bal = CF_TREE_RIGHT;
            break;

          default:
            (*n)->bal = CF_TREE_NONE;
            (*n)->right->bal = CF_TREE_NONE;
        }

        (*n)->right->left->bal = CF_TREE_NONE;
        cf_tree_rotate_right(& (*n)->right);
        cf_tree_rotate_left(n);
      }

      return 0;

    default:
      (*n)->bal = CF_TREE_RIGHT;
      return 1;
  }
}


int cf_tree_insert(t_cf_tree *tree,t_cf_tree_node **n, t_cf_tree_dataset *d) {
  int tmp;

  if(!n) n = &tree->root;

  if(!(*n)) {
    *n = fo_alloc(NULL,1,sizeof(*tree->root),FO_ALLOC_CALLOC);

    (*n)->d   = memdup(d,sizeof(*d));
    (*n)->bal = 0;

    return 1;
  }

  if(tree->compare(d,(*n)->d) < 0) {
    if((tmp = cf_tree_insert(tree,&(*n)->left,d)) == 1) {
      return cf_tree_leftgrown(n);
    }

    return tmp;
  }
  else if(tree->compare(d,(*n)->d) > 0) {
    if((tmp = cf_tree_insert(tree,&(*n)->right,d)) == 1) {
      return cf_tree_rightgrown(n);
    }

    return tmp;
  }

  return -1;
}

/**
 * Handle the case that the tree has shrunk on the
 * left side
 * \param n The subtree root node
 * \return Returns 0 or 1
 */
int cf_tree_leftshrunk(t_cf_tree_node **n) {
  switch((*n)->bal) {
    case CF_TREE_LEFT:
      (*n)->bal = CF_TREE_NONE;

      return 1;

    case CF_TREE_RIGHT:
      if((*n)->right->bal == CF_TREE_RIGHT) {
        (*n)->bal = (*n)->right->bal = CF_TREE_NONE;
        cf_tree_rotate_left(n);

        return 1;
      }
      else if((*n)->right->bal == CF_TREE_NONE) {
        (*n)->bal = CF_TREE_RIGHT;
        (*n)->right->bal = CF_TREE_LEFT;
        cf_tree_rotate_left(n);

        return 0;
      }
      else {
        switch((*n)->right->left->bal) {
          case CF_TREE_LEFT:
            (*n)->bal = CF_TREE_NONE;
            (*n)->right->bal = CF_TREE_RIGHT;
            break;

          case CF_TREE_RIGHT:
            (*n)->bal = CF_TREE_LEFT;
            (*n)->right->bal = CF_TREE_NONE;
            break;

          default:
            (*n)->bal = CF_TREE_NONE;
            (*n)->right->bal = CF_TREE_NONE;
        }

        (*n)->right->left->bal = CF_TREE_NONE;
        cf_tree_rotate_right(&(*n)->right);
        cf_tree_rotate_left(n);
        return 1;
      }

    default:
      (*n)->bal = CF_TREE_RIGHT;
      return 0;
  }
}

/**
 * Handle the case that the tree has shrunk on the
 * right side
 * \param n The subtree root node
 * \return Returns 0 or 1
 */
int cf_tree_rightshrunk(t_cf_tree_node **n) {
  switch((*n)->bal) {
    case CF_TREE_RIGHT:
      (*n)->bal = CF_TREE_NONE;
      return 1;

    case CF_TREE_LEFT:
      if((*n)->left->bal == CF_TREE_LEFT) {
        (*n)->bal = (*n)->left->bal = CF_TREE_NONE;
        cf_tree_rotate_right(n);

        return 1;
      }
      else if((*n)->left->bal == CF_TREE_NONE) {
        (*n)->bal = CF_TREE_LEFT;
        (*n)->left->bal = CF_TREE_RIGHT;
        cf_tree_rotate_right(n);

        return 0;
      }
      else {
        switch((*n)->left->right->bal) {
          case CF_TREE_LEFT:
            (*n)->bal = CF_TREE_RIGHT;
            (*n)->left->bal = CF_TREE_NONE;
            break;

          case CF_TREE_RIGHT:
            (*n)->bal = CF_TREE_NONE;
            (*n)->left->bal = CF_TREE_LEFT;
            break;

          default:
            (*n)->bal = CF_TREE_NONE;
            (*n)->left->bal = CF_TREE_NONE;
        }

        (*n)->left->right->bal = CF_TREE_NONE;

        cf_tree_rotate_left(&(*n)->left);
        cf_tree_rotate_right(n);

        return 1;
      }

    default:
      (*n)->bal = CF_TREE_LEFT;
      return 0;
  }
}

/**
 * This function finds the highest subtree
 * \param target Target node
 * \param n The subtree root node
 * \param res Result of this operation (height of the tree)
 * \return Returns 0 or 1
 */
int cf_tree_findhighest(t_cf_tree_node *target,t_cf_tree_node **n,int *res) {
  t_cf_tree_node *tmp;

  *res = 1;
  if(!(*n)) {
    return 0;
  }

  if((*n)->right) {
    if(!cf_tree_findhighest(target,&(*n)->right,res)) {
      return 0;
    }
    if(*res == 1) {
      *res = cf_tree_rightshrunk(n);
    }

    return 1;
  }

  target->d = (*n)->d;
  tmp = *n;
  *n = (*n)->left;
  free(tmp);

  return 1;
}

/**
 * This function finds the lowest subtree
 * \param target The target node
 * \param n The subtree root node
 * \param res The result of this operation (height of the tree)
 * \return Returns 0 or 1
 */
int cf_tree_findlowest(t_cf_tree_node *target,t_cf_tree_node **n,int *res) {
  t_cf_tree_node *tmp;

  *res = 1;
  if(!(*n)) return 0;

  if((*n)->left) {
    if(!cf_tree_findlowest(target,&(*n)->left,res)) {
      return 0;
    }
    if(*res == 1) {
      *res = cf_tree_leftshrunk(n);
    }

    return 1;
  }

  target->d = (*n)->d;
  tmp = *n;
  *n = (*n)->right;
  free(tmp);

  return 1;
}

int cf_tree_remove(t_cf_tree *tree,t_cf_tree_node **n,t_cf_tree_dataset *key) {
  int tmp = 1;

  if(!n) n = &tree->root;

  if(!(*n)) return -1;

  if(tree->compare(key,(*n)->d) < 0) {
    if((tmp = cf_tree_remove(tree,&(*n)->left, key)) == 1) {
      return cf_tree_leftshrunk(n);
    }

    return tmp;
  }
  else if(tree->compare(key, (*n)->d) > 0) {
    if((tmp = cf_tree_remove(tree,&(*n)->right,key)) == 1) {
      return cf_tree_rightshrunk(n);
    }

    return tmp;
  }

  if((*n)->left) {
    if(cf_tree_findhighest(*n, &((*n)->left), &tmp)) {
      if(tmp == 1) {
        tmp = cf_tree_leftshrunk(n);
      }

      return tmp;
    }
  }
  if((*n)->right) {
    if(cf_tree_findlowest(*n, &((*n)->right), &tmp)) {
      if(tmp == 1) {
        tmp = cf_tree_rightshrunk(n);
      }
      return tmp;
    }
  }

  free(*n);
  *n = NULL;

  return 1;
}

const t_cf_tree_dataset *cf_tree_find(t_cf_tree *tree,t_cf_tree_node *n, t_cf_tree_dataset *key) {

  if(!n) return NULL;

  if(tree->compare(key,n->d) < 0) {
    return cf_tree_find(tree,n->left,key);
  }
  else if(tree->compare(key,n->d) > 0) {
    return cf_tree_find(tree,n->right,key);
  }

  return n->d;
}

void cf_tree_init(t_cf_tree *tree,int (*compare)(t_cf_tree_dataset *,t_cf_tree_dataset *),void (*destroy)(t_cf_tree_dataset *)) {
  tree->root    = NULL;
  tree->compare = compare;
  tree->destroy = destroy;
}

/**
 * Internal function used to destroy all tree nodes (works recursively)
 * \param tree The tree
 * \param n Actual node
 */
void cf_tree_destroy_nodes(t_cf_tree *tree,t_cf_tree_node *n) {
  if(n) {
    if(n->left) cf_tree_destroy_nodes(tree,n->left);
    if(n->right) cf_tree_destroy_nodes(tree,n->right);

    if(tree->destroy) {
      tree->destroy(n->d);
    }
    else {
      if(n->d->key)  free(n->d->key);
      if(n->d->data) free(n->d->data);
    }

    free(n->d);
    free(n);
  }
}

void cf_tree_destroy(t_cf_tree *tree) {
  cf_tree_destroy_nodes(tree,tree->root);
}

/* }}} */

/* {{{ list functions */
void cf_list_init(t_list_head *head) {
  memset(head,0,sizeof(*head));
}

void cf_list_append(t_list_head *head,void *data,size_t size) {
  t_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  if(head->last == NULL) {
    head->last = head->elements = elem;
  }
  else {
    head->last->next = elem;
    elem->prev       = head->last;
    head->last       = elem;
  }
}

void cf_list_prepend(t_list_head *head,void *data,size_t size) {
  t_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  if(head->elements) {
    elem->next = head->elements;
    head->elements->prev = elem;
    head->elements = elem;
  }
  else {
    head->last = head->elements = elem;
  }
}

void cf_list_insert(t_list_head *head,t_list_element *prev,void *data,size_t size) {
  t_list_element *elem = fo_alloc(NULL,1,sizeof(*elem),FO_ALLOC_CALLOC);

  elem->data = memdup(data,size);
  elem->size = size;

  elem->next = prev->next;
  elem->prev = prev;
  if(prev->next) prev->next->prev = elem;
  prev->next = elem;
}

void *cf_list_search(t_list_head *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  t_list_element *elem;

  for(elem=head->elements;elem;elem=elem->next) {
    if(compare(elem->data,data) == 0) return elem->data;
  }

  return NULL;
}

void cf_list_delete(t_list_head *head,t_list_element *elem) {
  if(elem->prev) elem->prev->next = elem->next;
  if(elem->next) elem->next->prev = elem->prev;
  
  if(head->elements == elem) head->elements = elem->next;
  if(head->last == elem) head->elements = elem->next;
}

void cf_list_destroy(t_list_head *head,void (*destroy)(void *data)) {
  t_list_element *elem,*elem1;

  for(elem=head->elements;elem;elem=elem1) {
    elem1 = elem->next;

    if(destroy) destroy(elem);
    free(elem->data);
    free(elem);
  }
}

/* }}} */

/* {{{ Faster implementation of the string comparing functions */

int cf_strcmp(const u_char *str1,const u_char *str2) {
  register u_char *ptr1 = (u_char *)str1;
  register u_char *ptr2 = (u_char *)str2;

  for(;*ptr1 && *ptr2 && *ptr1 == *ptr2;ptr1++,ptr2++);

  if(*ptr1 == *ptr2) return 0;

  return 1;
}

int cf_strcasecmp(const u_char *str1,const u_char *str2) {
  register u_char *ptr1 = (u_char *)str1;
  register u_char *ptr2 = (u_char *)str2;

  for(;*ptr1 && *ptr2 && toupper(*ptr1) == toupper(*ptr2);ptr1++,ptr2++);

  if(toupper(*ptr1) == toupper(*ptr2)) return 0;

  return 1;
}

int cf_strncmp(const u_char *str1,const u_char *str2,size_t n) {
  register u_char *ptr1 = (u_char *)str1;
  register u_char *ptr2 = (u_char *)str2;
  register size_t i;

  for(i=0;*ptr1 && *ptr2 && *ptr1 == *ptr2 && i < n;ptr1++,ptr2++,i++) {
    if(i == n - 1) return 0;
  }

  return 1;
}

int cf_strncasecmp(const u_char *str1,const u_char *str2,size_t n) {
  register u_char *ptr1 = (u_char *)str1;
  register u_char *ptr2 = (u_char *)str2;
  register size_t i;

  for(i=0;*ptr1 && *ptr2 && i < n && toupper(*ptr1) == toupper(*ptr2);ptr1++,ptr2++,i++) {
    if(i == n - 1) return 0;
  }

  return 1;
}

size_t cf_strlen_utf8(const u_char *str,size_t rlen) {
  register size_t len;
  register u_char *ptr;
  int bytes;
  u_int32_t num;

  for(ptr=(u_char *)str;*ptr;len++) {
    if((bytes = utf8_to_unicode(ptr,rlen,&num)) < 0) {
      return -1;
    }

    ptr  += bytes;
    rlen -= bytes;
  }

  return rlen;
}

/* }}} */

/* eof */
