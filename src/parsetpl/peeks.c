/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "utils.h"
#include "hashlib.h"
#include "parsetpl.h"

/* {{{ peek_next_nws_type */
int peek_next_nws_type(cf_array_t *data) {
  size_t i;
  int t = -1;
  token_t *tok;
  
  for (i = 0; i < data->elements; i++) {
    tok = (token_t *)cf_array_element_at(data,i);
    if(tok->type == PARSETPL_TOK_WHITESPACE) continue;

    t = tok->type;
    break;
  }

  return t;
}
/* }}} */

/* {{{ peek_for_hash */
// returns: 0 - no hash, 1 - hash, -1 invalid tag
int peek_for_hash(cf_array_t *data) {
  size_t i;
  token_t *tok;
  int had_str = 0;
  
  for (i = 0; i < data->elements; i++) {
    tok = (token_t *)cf_array_element_at(data,i);
    if(tok->type == PARSETPL_TOK_WHITESPACE) continue;

    if(tok->type == PARSETPL_TOK_STRING) {
      if(had_str) return -1;

      had_str = 1;
      continue;
    }

    if(tok->type == PARSETPL_TOK_HASHASSIGNMENT) {
      if(had_str) return 1;
      return -1;
    }

    // no whitespace, no string, no hash assignment => no hash
    break;
  }

  return 0;
}
/* }}} */

/* eof */
