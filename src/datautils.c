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

#include "charconvert.h"
#include "utils.h"
/* }}} */

/* {{{ str_to_u_int64 */
u_int64_t str_to_u_int64(register const u_char *ptr) {
  u_int64_t retval = 0;

  for(;*ptr;++ptr) {
    retval = retval * 10 + *ptr - '0';
  }

  return retval;
}
/* }}} */

/* {{{ u_int64_to_str */
void u_int64_to_str(t_string *str, u_int64_t num) {
  while(num) {
    str_char_append(str,num % 10 + '0');
    num /= 10;
  }
}
/* }}} */

/* eof */
