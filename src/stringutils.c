/**
 * \file stringutils.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief string utilities for the Classic Forum
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
#include "cfconfig.h"
#include "defines.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ cf_str_init */
void cf_str_init(cf_string_t *str) {
  str->len      = 0;
  str->reserved = 0;
  str->growth   = CF_BUFSIZ;
  str->content  = NULL;
}
/* }}} */

/* {{{ cf_str_init_growth */
void cf_str_init_growth(cf_string_t *str,unsigned growth) {
  str->len      = 0;
  str->reserved = 0;
  str->growth   = growth;
  str->content  = NULL;
}
/* }}} */

/* {{{ cf_str_cleanup */
void cf_str_cleanup(cf_string_t *str) {
  str->len      = 0;
  str->reserved = 0;

  if(str->content) free(str->content);

  str->content  = NULL;
}
/* }}} */

/* {{{ cf_str_char_append */
size_t cf_str_char_append(cf_string_t *str,const u_char content) {
  if(str->growth == 0) str->growth = CF_BUFSIZ;

  if(str->len + 1 >= str->reserved) {
    str->reserved += str->growth;
    str->content   = cf_alloc(str->content,(size_t)str->reserved,1,CF_ALLOC_REALLOC);
  }

  str->content[str->len] = content;
  str->len              += 1;
  str->content[str->len] = '\0';

  return 1;
}
/* }}} */

/* {{{ cf_str_chars_append */
size_t cf_str_chars_append(cf_string_t *str,const u_char *content,size_t length) {
  size_t len;

  if(str->growth == 0) str->growth = CF_BUFSIZ;
  len = str->growth;

  if(str->len + length >= str->reserved) {
    if(length >= len) len += length;

    str->reserved += len;
    str->content   = cf_alloc(str->content,(size_t)str->reserved,1,CF_ALLOC_REALLOC);
  }

  memcpy(&str->content[str->len],content,length);
  str->len += length;
  str->content[str->len] = '\0';

  return length;
}
/* }}} */

/* {{{ cf_str_equal_string */
int cf_str_equal_string(const cf_string_t *str1,const cf_string_t *str2) {
  register u_char *ptr1 = str1->content,*ptr2 = str2->content;
  register size_t i;

  if(str1->len != str2->len) return 1;

  for(i = 0; i < str1->len; ++i,++ptr1,++ptr2) {
    if(*ptr1 != *ptr2) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ cf_str_equal_chars */
int cf_str_equal_chars(const cf_string_t *str1,const u_char *str2, size_t len) {
  register size_t i = 0;
  register u_char *ptr1 = str1->content,*ptr2 = (u_char *)str2;

  if(str1->len != len) return 1;

  for(i = 0; i < len; ++i,++ptr1,++ptr2) {
    if(*ptr1 != *ptr2) return 1;
  }

  return 0;
}
/* }}} */

/* {{{ cf_str_str_append */
size_t cf_str_str_append(cf_string_t *str,cf_string_t *content) {
  return cf_str_chars_append(str,content->content,content->len);
}
/* }}} */

/* {{{ cf_str_cstr_append */
size_t cf_str_cstr_append(cf_string_t *str,const u_char *content) {
  return cf_str_chars_append(str,content,strlen(content));
}
/* }}} */

/* {{{ cf_str_cstr_set */
int cf_str_cstr_set(cf_string_t *str,const u_char *content) {
  return cf_str_char_set(str,content,strlen(content));
}
/* }}} */

/* {{{ cf_str_char_set */
size_t cf_str_char_set(cf_string_t *str,const u_char *content,size_t length) {
  size_t len;

  if(str->growth == 0) str->growth = CF_BUFSIZ;
  len = str->growth;

  if(str->len + length >= str->reserved) {
    if(length >= len) len += length;

    str->reserved  = len;
    str->content   = cf_alloc(str->content,len,1,CF_ALLOC_REALLOC);
  }

  memcpy(str->content,content,length);
  str->len = length;
  str->content[length] = '\0';

  return length;
}
/* }}} */

/* {{{ cf_str_str_set */
size_t cf_str_str_set(cf_string_t *str,cf_string_t *set) {
  return cf_str_char_set(str,set->content,set->len);
}
/* }}} */

#ifdef NOSTRDUP
/* {{{ strdup */
u_char *strdup(const u_char *str) {
  size_t len = strlen(str);
  u_char *buff = cf_alloc(NULL,1,len+1,CF_ALLOC_MALLOC);

  memcpy(buff,str,len+1);

  return buff;
}
/* }}} */
#endif

#ifdef NOSTRNDUP
/* {{{ strndup */
u_char *strndup(const u_char *str,size_t len) {
  u_char *buff = cf_alloc(NULL,1,len+1,CF_ALLOC_MALLOC);

  memcpy(buff,str,len);
  buff[len-1] = '\0';

  return buff;
}
/* }}} */
#endif

/* {{{ cf_strdup */
size_t cf_strdup(u_char **dst,const u_char *str) {
  size_t len = strlen(str);
  *dst = cf_alloc(NULL,1,len+1,CF_ALLOC_MALLOC);

  memcpy(*dst,str,len+1);

  return len;
}
/* }}} */

/* {{{ cf_strcmp */
int cf_strcmp(register const u_char *str1,register const u_char *str2) {
  for(;*str1 && *str2 && *str1 == *str2;str1++,str2++);

  if(*str1 == *str2) return 0;

  return 1;
}
/* }}} */

/* {{{ cf_strcasecmp */
int cf_strcasecmp(register const u_char *str1,register const u_char *str2) {
  for(;*str1 && *str2 && toupper(*str1) == toupper(*str2);str1++,str2++);

  if(toupper(*str1) == toupper(*str2)) return 0;

  return 1;
}
/* }}} */

/* {{{ cf_strncmp */
int cf_strncmp(register const u_char *str1,register const u_char *str2,size_t n) {
  register size_t i;

  for(i=0;*str1 && *str2 && *str1 == *str2 && i < n;str1++,str2++,i++) {
    if(i == n - 1) return 0;
  }

  if(*str1 == *str2) return 0;

  return 1;
}
/* }}} */

/* {{{ cf_strncasecmp */
int cf_strncasecmp(register const u_char *str1,register const u_char *str2,size_t n) {
  register size_t i;

  for(i=0;*str1 && *str2 && i < n && toupper(*str1) == toupper(*str2);str1++,str2++,i++) {
    if(i == n - 1) return 0;
  }

  if(*str1 == *str2) return 0;

  return 1;
}
/* }}} */

/* {{{ cf_strlen_utf8_wo_space */
size_t cf_strlen_utf8_wo_space(const u_char *str,size_t rlen) {
  register size_t len = 0;
  register u_char *ptr;
  u_char *end;
  int bytes;
  u_int32_t num;

  for(ptr=(u_char *)str;*ptr && isspace(*ptr);++ptr);
  for(end=(u_char *)(str+rlen-1);*end && isspace(*end) && end > str;--end);

  for(;*ptr && ptr <= end;++len) {
    if((bytes = utf8_to_unicode(ptr,rlen,&num)) < 0) return -1;

    ptr  += bytes;
    rlen -= bytes;
  }

  return len;
}
/* }}} */

/* {{{ cf_strlen_utf8 */
size_t cf_strlen_utf8(const u_char *str,size_t rlen) {
  register size_t len = 0;
  register u_char *ptr;
  int bytes;
  u_int32_t num;

  for(ptr=(u_char *)str;*ptr;++len) {
    if((bytes = utf8_to_unicode(ptr,rlen,&num)) < 0) return -1;

    ptr  += bytes;
    rlen -= bytes;
  }

  return len;
}
/* }}} */

/* {{{ cf_isspace */
int cf_isspace(u_int32_t num) {
  if(num == 0x20 || num == 0xA0 || (num >= 0x2000 && num <= 0x200B) || (num >= 0x2028 && num <= 0x202F)) return 1;

  return 0;
}
/* }}} */

/* {{{ cf_split
 * Returns: long       the length of the list
 * Parameters:
 *   - const u_char *big     the string to cf_split
 *   - const u_char *small   the string where to cf_split
 *   - u_char ***list        the list
 *
 * This function splits a string into pieces
 *
 */
size_t cf_split(const u_char *big,const u_char *small,u_char ***ulist) {
  u_char **list  = cf_alloc(NULL,PRERESERVE,sizeof(*list),CF_ALLOC_MALLOC);
  u_char  *pos   = (u_char *)big,*pre = (u_char *)big;
  size_t len   = 0;
  size_t reser = PRERESERVE;
  size_t slen  = strlen(small);

  while((pos = strstr(pos,small)) != NULL) {
    *pos = '\0';

    list[len++] = strdup(pre);

    if(len >= reser) {
      reser += PRERESERVE;
      list = cf_alloc(list,reser,sizeof(*list),CF_ALLOC_REALLOC);
    }

    pre    = pos+slen;
    *pos++ = *small;
  }

  if(len >= reser) list = cf_alloc(list,++reser,sizeof(*list),CF_ALLOC_REALLOC);
  list[len++] = strdup(pre);

  *ulist = list;

  return len;
}
/* }}} */

/* {{{ cf_nsplit */
size_t cf_nsplit(const u_char *big,const u_char *small,u_char ***ulist,size_t max) {
  u_char **list  = cf_alloc(NULL,PRERESERVE,sizeof(*list),CF_ALLOC_MALLOC);
  u_char  *pos   = (u_char *)big,*pre = (u_char *)big;
  size_t len   = 0;
  size_t reser = PRERESERVE;
  size_t slen  = strlen(small);

  while((pos = strstr(pos,small)) != NULL) {
    *pos = '\0';

    list[len++] = strdup(pre);

    if(len >= reser) {
      reser += PRERESERVE;
      list = cf_alloc(list,reser,sizeof(*list),CF_ALLOC_REALLOC);
    }

    pre    = pos+slen;
    *pos++ = *small;
    if(len + 1 == max) break;
  }

  if(len + 1 <= max) {
    if(len >= reser) list = cf_alloc(list,++reser,sizeof(*list),CF_ALLOC_REALLOC);
    list[len++] = strdup(pre);
  }

  *ulist = list;

  return len;
}
/* }}} */


/* eof */
