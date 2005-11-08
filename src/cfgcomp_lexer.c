/**
 * \file configlexer.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This file contains the config lexer
 */

/* {{{ includes */
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dlfcn.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <inttypes.h>

#include <pwd.h>

#include "utils.h"
#include "cfgcomp.h"
/* }}} */


/* {{{ cf_cfg_lex_is_k12_token, recognizes k=1 and k=2 symbols */
int cf_cfg_lex_is_k12_token(u_char *ptr,cf_cfg_stream_t *stream,int change_state) {
  switch(*ptr) {
    case '"':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_DQ;
    case '\'':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_SQ;

    case '(':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_LPAREN;
    case ')':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_RPAREN;

    case '[':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_LBRACKET;

    case ']':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_RBRACKET;

    case '.':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_DOT;

    case ',':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_COMMA;

    case '+':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_PLUS;

    case '-':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_MINUS;

    case '/':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_DIV;

    case '*':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_MULT;

    case ';':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_SEMI;

    case '%':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_PERC;

    case '$':
      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_DOLLAR;

    /* now we need a k=2 lexer (not parser!) since the symbols will not be deterministic if k=1 */
    case '=':
      if(*(ptr+1) == '=') {
        if(change_state) stream->pos = ptr + 2;
        return CF_TOK_EQ;
      }

      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_SET;

    case '!':
      if(*(ptr+1) == '=') {
        if(change_state) stream->pos = ptr + 2;
        return CF_TOK_NOTEQ;
      }

      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_NOT;

    case '<':
      if(*(ptr+1) == '=') {
        if(change_state) stream->pos = ptr + 2;
        return CF_TOK_LTEQ;
      }

      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_LT;

    case '>':
      if(*(ptr+1) == '=') {
        if(change_state) stream->pos = ptr + 2;
        return CF_TOK_GTEQ;
      }

      if(change_state) stream->pos = ptr + 1;
      return CF_TOK_GT;
  }

  return CF_NO_TOK;
}
/* }}} */

/* {{{ cf_cfg_lex_isspace */
int cf_cfg_lex_isspace(u_char c) {
  if(isspace((int)c) || c == '\015' || c == '\012') return 1;
  return 0;
}
/* }}} */

/* {{{ cf_cfg_lex_readstr */
int cf_cfg_lex_readstr(cf_cfg_stream_t *stream,u_char *ptr,int do_seq,u_char delim,int changestate) {
  cf_string_t str;

  cf_str_init_growth(&str,256);
  for(;*ptr && *ptr != delim;++ptr) {
    if(do_seq) {
      if(*ptr == '\\') {
        if(*++ptr == delim) cf_str_char_append(&str,delim);
        else {
          switch(*ptr) {
            case 'n':
              cf_str_char_append(&str,'\n');
              break;
            case 't':
              cf_str_char_append(&str,'\t');
              break;
            case 'r':
              cf_str_char_append(&str,'\r');
              break;
            default:
              cf_str_char_append(&str,*ptr);
          }
        }
      }
      else cf_str_char_append(&str,*ptr);
    }
    else {
      if(*ptr == '\\' && *(ptr+1) == delim) {
        cf_str_char_append(&str,delim);
        ++ptr;
      }
      else cf_str_char_append(&str,*ptr);
    }
  }

  if(*ptr == '\0') {
    cf_str_cleanup(&str);
    return -1;
  }

  stream->stok = str.content;
  if(changestate) stream->pos = ptr + 1;

  return 0;
}
/* }}} */

/* {{{ cf_cfg_lexer */
int cf_cfg_lexer(cf_cfg_stream_t *stream,int changestate) {
  register u_char *ptr = NULL;
  u_char *ptr1,*tname;
  int ttype;

  if(!stream->pos) {
    stream->pos = stream->content;
    if(cf_strncmp(stream->content,"\357\273\277",3) == 0) stream->pos += 3; /* ignore BOM */
  }

  ptr = stream->pos;

 cf_cfg_lex_comment_state:
  if(*ptr == '#') { /* comments */
    for(;*ptr && *ptr != '\012' && *ptr != '\015';++ptr);
  }

  /* ignore whitspaces */
  for(;cf_cfg_lex_isspace(*ptr);++ptr);

  if(*ptr == '#') goto cf_cfg_lex_comment_state;

  if((ttype = cf_cfg_lex_is_k12_token(ptr,stream,changestate)) != CF_NO_TOK) {
    if(ttype == CF_TOK_DQ) {
      if(cf_cfg_lex_readstr(stream,ptr+1,1,'"',changestate) != 0) return CF_ERR;
      return CF_TOK_STRING;
    }
    else if(ttype == CF_TOK_SQ) {
      if(cf_cfg_lex_readstr(stream,ptr+1,0,'\'',changestate) != 0) return CF_ERR;
      return CF_TOK_STRING;
    }

    return ttype;
  }

  /* {{{ digit tokens */
  if(isdigit((int)*ptr)) {
    if(*ptr == '0') {
      if(*(ptr+1) == 'x') {
        stream->numtok = strtol(ptr,(char **)&ptr1,16);
        if(changestate) stream->pos = ptr1;
        return CF_TOK_NUM;
      }
      else if(!isdigit(*(ptr+1))) { /* single 0 */
        stream->numtok = 0;
        if(changestate) stream->pos = ptr+1;
        return CF_TOK_NUM;
      }
      else {
        stream->numtok = strtol(ptr,(char **)&ptr1,8);
        if(changestate) stream->pos = ptr1;
        return CF_TOK_NUM;
      }
    }
    else {
      stream->numtok = strtol(ptr,(char **)&ptr1,10);
      if(changestate) stream->pos = ptr1;
      return CF_TOK_NUM;
    }
  }
  /* }}} */
  /* {{{ get identifier/keyword token */
  else if(isalpha((int)*ptr)) {
    for(ptr1=ptr;*ptr && !cf_cfg_lex_isspace(*ptr) && !cf_cfg_lex_is_k12_token(ptr,stream,0);++ptr);

    tname = strndup(ptr1,ptr-ptr1);
    if(cf_strcmp(tname,"if") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_IF;
    }
    else if(cf_strcmp(tname,"elseif") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_ELSEIF;
    }
    else if(cf_strcmp(tname,"else") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_ELSE;
    }
    else if(cf_strcmp(tname,"with") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_WITH;
    }
    else if(cf_strcmp(tname,"end") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_END;
    }
    else if(cf_strcmp(tname,"and") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_AND;
    }
    else if(cf_strcmp(tname,"or") == 0) {
      free(tname);
      if(changestate) stream->pos = ptr;
      return CF_TOK_OR;
    }
    else {
      stream->stok = tname;
      if(changestate) stream->pos = ptr;
      return CF_TOK_IDENT;
    }

  }
  /* }}} */

  return CF_TOK_EOF;
}
/* }}} */

/* {{{ cf_dbg_get_token */
u_char *cf_dbg_get_token(int ttype) {
  switch(ttype) {
  case CF_TOK_DOT:
    return "CF_TOK_DOT";
  case CF_TOK_PLUS:
    return "CF_TOK_PLUS";
  case CF_TOK_MINUS:
    return "CF_TOK_MINUS";
  case CF_TOK_DIV:
    return "CF_TOK_DIV";
  case CF_TOK_MULT:
    return "CF_TOK_MULT";
  case CF_TOK_SEMI:
    return "CF_TOK_SEMI";
  case CF_TOK_PERC:
    return "CF_TOK_PERC";
  case CF_TOK_EQ:
    return "CF_TOK_EQ";
  case CF_TOK_SET:
    return "CF_TOK_SET";
  case CF_TOK_NOTEQ:
    return "CF_TOK_NOTEQ";
  case CF_TOK_NOT:
    return "CF_TOK_NOT";
  case CF_TOK_LTEQ:
    return "CF_TOK_LTEQ";
  case CF_TOK_LT:
    return "CF_TOK_LT";
  case CF_TOK_GTEQ:
    return "CF_TOK_GTEQ";
  case CF_TOK_GT:
    return "CF_TOK_GT";
  case CF_TOK_NUM:
    return "CF_TOK_NUM";
  case CF_TOK_IF:
    return "CF_TOK_IF";
  case CF_TOK_ELSEIF:
    return "CF_TOK_ELSEIF";
  case CF_TOK_ELSE:
    return "CF_TOK_ELSE";
  case CF_TOK_WITH:
    return "CF_TOK_WITH";
  case CF_TOK_END:
    return "CF_TOK_END";
  case CF_TOK_AND:
    return "CF_TOK_AND";
  case CF_TOK_OR:
    return "CF_TOK_OR";
  case CF_TOK_IDENT:
    return "CF_TOK_IDENT";
  case CF_TOK_DOLLAR:
    return "CF_TOK_DOLLAR";
  case CF_TOK_EOF:
    return "CF_TOK_EOF";
  case CF_TOK_STRING:
    return "CF_TOK_STRING";
  case CF_TOK_COMMA:
    return "CF_TOK_COMMA";
  case CF_TOK_LPAREN:
    return "CF_TOK_LPAREN";
  case CF_TOK_RPAREN:
    return "CF_TOK_RPAREN";
  case CF_TOK_ARRAY:
    return "CF_TOK_ARRAY";
  case CF_TOK_STMT:
    return "CF_TOK_STMT";
  case CF_TOK_LOAD:
    return "CF_TOK_LOAD";
  case CF_TOK_LBRACKET:
    return "CF_TOK_LBRACKET";
  case CF_TOK_RBRACKET:
    return "CF_TOK_RBRACKET";
  case CF_TOK_FID:
    return "CF_TOK_FID";
  }

  printf("%s:%d: unknown: 0x%X\n",__FILE__,__LINE__,ttype);
  return "(UNKNOWN)";
}
/* }}} */

/* eof */
