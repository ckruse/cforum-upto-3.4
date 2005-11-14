/**
 * \file stringutils.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
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

/* {{{ cf_str_to_uint64 */
u_int64_t cf_str_to_uint64(register const u_char *ptr) {
  u_int64_t retval = 0;

  for(;*ptr && isdigit(*ptr);++ptr) retval = retval * 10 + *ptr - '0';

  return retval;
}
/* }}} */

/* {{{ cf_uint64_to_str */
void cf_uint64_to_str(cf_string_t *str, u_int64_t num) {
  register u_char *ptr1,*ptr2,tmp;
  size_t i = 0;

  if(num) {
    while(num) {
      cf_str_char_append(str,'0' + (num % 10));
      num /= 10;
      ++i;
    }

    /* now we have to swap the bytes */
    for(ptr1=str->content+str->len-i,ptr2=str->content+str->len-1;ptr1<ptr2;ptr1++,ptr2--) {
      tmp = *ptr1;
      *ptr1 = *ptr2;
      *ptr2 = tmp;
    }
  }
  else cf_str_char_append(str,'0');
}
/* }}} */

/* {{{ cf_uint16_to_str */
void cf_uint16_to_str(cf_string_t *str, u_int16_t num) {
  register u_char *ptr1,*ptr2,tmp;
  size_t i = 0;

  if(num) {
    while(num) {
      cf_str_char_append(str,'0' + (num % 10));
      num /= 10;
      ++i;
    }

    /* now we have to swap the bytes */
    for(ptr1=str->content+str->len-i,ptr2=str->content+str->len-1;ptr1<ptr2;ptr1++,ptr2--) {
      tmp = *ptr1;
      *ptr1 = *ptr2;
      *ptr2 = tmp;
    }
  }
  else cf_str_char_append(str,'0');
}
/* }}} */

/* {{{ cf_uint32_to_str */
void cf_uint32_to_str(cf_string_t *str, u_int32_t num) {
  register u_char *ptr1,*ptr2,tmp;
  size_t i = 0;

  if(num) {
    while(num) {
      cf_str_char_append(str,'0' + (num % 10));
      num /= 10;
      ++i;
    }

    /* now we have to swap the bytes */
    for(ptr1=str->content+str->len-i,ptr2=str->content+str->len-1;ptr1<ptr2;ptr1++,ptr2--) {
      tmp = *ptr1;
      *ptr1 = *ptr2;
      *ptr2 = tmp;
    }
  }
  else cf_str_char_append(str,'0');
}
/* }}} */

/* eof */
