/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "config.h"
#include "defines.h"

#include "utils.h"
#include "hashlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "parsetpl.h"

/* {{{ peek_next_nws_type */
int peek_next_nws_type(t_array *data) {
  size_t i;
  int t = -1;
  t_token *tok;
  
  for (i = 0; i < data->elements; i++) {
    tok = (t_token *)array_element_at(data,i);
    if(tok->type == PARSETPL_TOK_WHITESPACE) continue;

    t = tok->type;
    break;
  }

  return t;
}
/* }}} */

/* {{{ peek_for_hash */
// returns: 0 - no hash, 1 - hash, -1 invalid tag
int peek_for_hash(t_array *data) {
  size_t i;
  t_token *tok;
  int had_str = 0;
  
  for (i = 0; i < data->elements; i++) {
    tok = (t_token *)array_element_at(data,i);
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
