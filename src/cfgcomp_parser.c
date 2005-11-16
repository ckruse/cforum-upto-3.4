/**
 * \file configparser.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This file contains the config parser
 */

/* {{{ includes */
#include "cfconfig.h"
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

#define cf_cfg_parser_new_node(tr) { \
  tr->next = cf_alloc(NULL,1,sizeof(cf_cfg_trees_t),CF_ALLOC_CALLOC); \
  tr = tr->next; \
}

#define cf_cfg_parser_newtok() cf_alloc(NULL,1,sizeof(cf_cfg_token_t),CF_ALLOC_CALLOC)

/* {{{ type setters */
#define cf_cfg_parser_val_int(_ival,tok) { \
  tok->data = cf_alloc(NULL,1,sizeof(*tok->data),CF_ALLOC_CALLOC); \
  tok->data->type = CF_TYPE_INT; \
  tok->data->ival = _ival; \
}

#define cf_cfg_parser_val_id(id,tok) { \
  tok->data = cf_alloc(NULL,1,sizeof(*tok->data),CF_ALLOC_CALLOC); \
  tok->data->type = CF_TYPE_ID; \
  tok->data->sval = id; \
}

#define cf_cfg_parser_val_str(str,tok) { \
  tok->data = cf_alloc(NULL,1,sizeof(*tok->data),CF_ALLOC_CALLOC); \
  tok->data->type = CF_TYPE_STR; \
  tok->data->sval = str; \
}

#define cf_cfg_parser_val_ary(ar,vlen,tok) { \
  tok->data = cf_alloc(NULL,1,sizeof(*tok->data),CF_ALLOC_CALLOC); \
  tok->data->type = CF_TYPE_ARY; \
  tok->data->ary  = ar; \
  tok->data->elems= vlen; \
}
/* }}} */

/* {{{ cf_cfg_parser_insert_node */

cf_cfg_token_t *cf_cfg_parser_insert_node(cf_cfg_trees_t *tree,cf_cfg_token_t *cur,cf_cfg_token_t *tok) {
  #ifdef CF_CFG_TREE_DEBUG
  static const u_char cn[] = "cf_cfg_parser_insert_node";
  #endif

  #ifdef CF_CFG_TREE_DEBUG
  printf("%s[%s:%d]: tree pointer: %p\n",cn,__FILE__,__LINE__,tree);
  #endif

  /*
   * there does not exist a tree yet
   */
  if(!cur) {
    #ifdef CF_CFG_TREE_DEBUG
    printf("%s[%s:%d]: tree does not yet exist, tree is now: %s (prec: %d)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->type),tok->prec);
    #endif

    tree->tree = tok;
    return tok;
  }
  else {
    /*
     * the new node is a child of the current
     */
    #ifdef CF_CFG_TREE_DEBUG
    printf("%s[%s:%d]: I got a token to append: %s (prec %d)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->type),tok->prec);
    #endif

    /*
     * append the new node because precendence is smaller
     */
    if(cur->prec < tok->prec) {
      tok->parent = cur;

      if(cur->left) {
        cur->right = tok;
        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: appending right because cur (%s) is higher than tok (%s)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(cur->type),cf_dbg_get_token(tok->type));
        #endif
      }
      else {
        cur->left  = tok;
        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: appending left because cur (%s) is higher than tok (%s)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(cur->type),cf_dbg_get_token(tok->type));
        #endif
      }
    }
    else if(cur->prec == tok->prec) {
      if(cur->parent) {
        if(cur->parent->left == cur) cur->parent->left = tok;
        else cur->parent->right = tok;

        tok->parent = cur;
        cur->parent = tok;
        tok->left = cur;

        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: replaced cur (%s) through tok (%s), 'cause cur->prec == tok->prec\n",cn,__FILE__,__LINE__,cf_dbg_get_token(cur->type),cf_dbg_get_token(tok->type));
        #endif
      }
      else {
        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: new root is %s (old was %s)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->type),cf_dbg_get_token(tree->tree->type));
        #endif

        tok->left         = tree->tree;
        tree->tree        = tok;
        tok->left->parent = tok;
      }
    }
    else {
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s: searching right position...\n",cn);
      #endif

      /*
       * We have to search the right position.
       */
      while(cur && cur->prec > tok->prec) cur = cur->parent;

      /*
       * set new node as root node
       */
      if(!cur) {
        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: new root is %s (old was %s)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->type),cf_dbg_get_token(tree->tree->type));
        #endif

        tok->left         = tree->tree;
        tree->tree        = tok;
        tok->left->parent = tok;
      }
      else {
        tok->parent = cur;

        if(cur->right) {
          tok->left         = cur->right;
          tok->left->parent = tok;
          cur->right        = tok;

          #ifdef CF_CFG_TREE_DEBUG
          printf("%s[%s:%d]: replacing %s (right) through %s\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->left->type),cf_dbg_get_token(tok->type));
          #endif
        }
        else {
          tok->left         = cur->left;
          tok->left->parent = tok;
          cur->left         = tok;

          #ifdef CF_CFG_TREE_DEBUG
          printf("%s[%s:%d]: replacing %s (left) through %s\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tok->left->type),cf_dbg_get_token(tok->type));
          #endif
        }
      }
    }
  }

  return tok;
}

/* }}} */

/* {{{ cf_cfg_parser_isoperator */
int cf_cfg_parser_isoperator(cf_cfg_token_t *prev) {
  switch(prev->type) {
    case CF_TOK_DOT:
    case CF_TOK_MINUS:
    case CF_TOK_PLUS:
    case CF_TOK_DIV:
    case CF_TOK_MULT:
    case CF_TOK_PERC:
    case CF_TOK_NOTEQ:
    case CF_TOK_EQ:
    case CF_TOK_SET:
    case CF_TOK_NOT:
    case CF_TOK_GT:
    case CF_TOK_GTEQ:
    case CF_TOK_LT:
    case CF_TOK_LTEQ:
    case CF_TOK_AND:
    case CF_TOK_OR:
      return 1;
  }

  return 0;
}
/* }}} */

/* {{{ cf_cfg_parser */
int cf_cfg_parser(cf_cfg_stream_t *stream,cf_cfg_trees_t *exec,cf_cfg_token_t *cur,int level,int retat) {
  int ret = 0,ret1,rc = 0;

  cf_cfg_trees_t
    *tr  = exec,
    *t   = NULL;

  cf_cfg_token_t
    *tok = NULL,
    *prev = NULL;

  #ifdef CF_CFG_TREE_DEBUG
  static const u_char cn[] = "cf_cfg_parser";

  printf("%s[%s:%d]: level: %d, exec: %p\n",cn,__FILE__,__LINE__,level,exec);
  #endif

  if(exec == NULL) tr = stream->trees = cf_alloc(NULL,1,sizeof(*stream->trees),CF_ALLOC_CALLOC);

  do {
    ret = cf_cfg_lexer(stream,1);

    #ifdef CF_CFG_TREE_DEBUG
    printf("%s[%s:%d]: token type: %s (%d)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(ret),ret);
    #endif

    /* {{{ check when to return */
    if(ret == retat) {
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s[%s:%d]: returning\n",cn,__FILE__,__LINE__);
      #endif
      return ret;
    }
    else if(retat == CF_TOK_IFELSEELSEIF && (ret == CF_TOK_ELSE || ret == CF_TOK_ELSEIF || ret == CF_TOK_END)) {
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s[%s:%d]: returning\n",cn,__FILE__,__LINE__);
      #endif
      return ret;
    }
    else if(retat == CF_TOK_COMMARPAREN && (ret == CF_TOK_COMMA || ret == CF_TOK_RPAREN)) {
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s[%s:%d]: returning\n",cn,__FILE__,__LINE__);
      #endif
      return ret;
    }
    /* }}} */

    /* {{{ handle if */
    if(ret == CF_TOK_IF) {
      tr->type = CF_TOK_IF;

      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_LPAREN) {
        fprintf(stderr,"[%s:%d] left paren expected but not found!\n",__FILE__,__LINE__);
        return CF_RETVAL_PARSEERROR;
      }
      if((ret = cf_cfg_parser(stream,tr,NULL,level+1,CF_TOK_RPAREN)) != CF_TOK_RPAREN) {
        fprintf(stderr,"[%s:%d] right paren exptected but not found!\n",__FILE__,__LINE__);
        return CF_RETVAL_PARSEERROR;
      }

      /* we got our condition; start recursion for if body */
      rc   = 0;
      ret1 = CF_TOK_STMT;

      tr->arguments    = cf_alloc(NULL,1,sizeof(*tr->arguments),CF_ALLOC_CALLOC);
      tr->arguments[0] = cf_alloc(NULL,1,sizeof(**tr->arguments),CF_ALLOC_CALLOC);
      tr->arglen       = 1;
      if((ret = cf_cfg_parser(stream,tr->arguments[0],NULL,level+1,CF_TOK_IFELSEELSEIF)) != CF_TOK_ELSE && ret != CF_TOK_ELSEIF && ret != CF_TOK_END) {
        fprintf(stderr,"[%s:%d] unexpected token type found!\n",__FILE__,__LINE__);
        return CF_RETVAL_PARSEERROR;
      }

      do {
        if(ret == CF_TOK_END) break;

        t = cf_alloc(NULL,1,sizeof(*t),CF_ALLOC_CALLOC);

        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: if: token type: %s (%d)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(ret),ret);
        #endif

        t->type = ret1;

        tr->arguments = cf_alloc(tr->arguments,++tr->arglen,sizeof(*tr->arguments),CF_ALLOC_REALLOC);
        tr->arguments[tr->arglen-1] = t;

        if(ret == CF_TOK_ELSEIF) {
          t->type = CF_TOK_ELSEIF;

          if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_LPAREN) {
            fprintf(stderr,"[%s:%d] left paren exptected but not found!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          if((ret = cf_cfg_parser(stream,t,NULL,level+1,CF_TOK_RPAREN)) != CF_TOK_RPAREN) {
            fprintf(stderr,"[%s:%d] right paren exptected but not found!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }

          /* get body of elseif() */
          t->arguments = cf_alloc(NULL,1,sizeof(*t->arguments),CF_ALLOC_CALLOC);
          t->arguments[0] = cf_alloc(NULL,1,sizeof(**t->arguments),CF_ALLOC_CALLOC);
          if((ret = cf_cfg_parser(stream,t->arguments[0],NULL,level+1,CF_TOK_IFELSEELSEIF)) != CF_TOK_ELSE && ret != CF_TOK_ELSEIF && ret != CF_TOK_END) {
            fprintf(stderr,"[%s:%d] unexpected token: %s\n",__FILE__,__LINE__,cf_dbg_get_token(ret));
            return CF_RETVAL_PARSEERROR;
          }

          if(ret == CF_TOK_END) break;
        }
        else if(ret == CF_TOK_ELSE) {
          if(rc == 1) {
            fprintf(stderr,"[%s:%d] more than one else?!",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR; /* we don't want more than one else */
          }
          rc = 1;

          t->arguments = cf_alloc(NULL,1,sizeof(*t->arguments),CF_ALLOC_CALLOC);
          t->arguments[0] = cf_alloc(NULL,1,sizeof(**t->arguments),CF_ALLOC_CALLOC);
          if((ret = cf_cfg_parser(stream,t->arguments[0],NULL,level+1,CF_TOK_IFELSEELSEIF)) != CF_TOK_ELSE && ret != CF_TOK_ELSEIF && ret != CF_TOK_END) {
            fprintf(stderr,"[%s:%d] unexpected token: %s\n",__FILE__,__LINE__,cf_dbg_get_token(ret));
            return CF_RETVAL_PARSEERROR;
          }

          if(ret != CF_TOK_END) {
            fprintf(stderr,"[%s:%d] token after else is not end!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }

          t->type = CF_TOK_ELSE;
          break;
        }

        ret1 = ret;
      } while(ret != CF_TOK_END);

      /* we got an end, now we want a semicolon */
      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_SEMI) return CF_RETVAL_PARSEERROR;

      cf_cfg_parser_new_node(tr);
      cur = prev = NULL;
    }
    /* }}} */
    /* {{{ handle with() */
    else if(ret == CF_TOK_WITH) {
      tr->type = CF_TOK_WITH;

      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_LPAREN) return CF_RETVAL_PARSEERROR;
      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_IDENT) return CF_RETVAL_PARSEERROR;

      tr->data     = stream->stok;
      stream->stok = NULL;

      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_RPAREN) return CF_RETVAL_PARSEERROR;

      t = cf_alloc(NULL,1,sizeof(*t),CF_ALLOC_CALLOC);
      if((ret = cf_cfg_parser(stream,t,NULL,level+1,CF_TOK_RPAREN)) != CF_TOK_END) return CF_RETVAL_PARSEERROR;
      tr->arguments    = cf_alloc(NULL,1,sizeof(*tr->arguments),CF_ALLOC_MALLOC);
      tr->arglen       = 1;
      tr->arguments[0] = t;

      if((ret = cf_cfg_lexer(stream,1)) != CF_TOK_SEMI) return CF_RETVAL_PARSEERROR;

      cf_cfg_parser_new_node(tr);
      cur = prev = NULL;
    }
    /* }}} */
    /* {{{ handle semicolon */
    else if(ret == CF_TOK_SEMI) {
      if(tr->type == 0) tr->type = CF_TOK_STMT;
      cf_cfg_parser_new_node(tr);
      cur = prev = NULL;
    }
    /* }}} */
    /* {{{ handle end token */
    else if(ret == CF_TOK_END) {
      if(level == 0) return CF_RETVAL_PARSEERROR;
      return CF_TOK_END;
    }
    /* }}} */
    /* {{{ handle arrays */
    else if(ret == CF_TOK_LPAREN) { /* array begins */
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s[%s:%d]: got begin of an ary\n",cn,__FILE__,__LINE__);
      #endif

      tok = cf_cfg_parser_newtok();
      tok->type = CF_TOK_ARRAY;
      tok->prec = CF_PREC_ATOM;

      do {
        tok->arguments = cf_alloc(tok->arguments,++tok->arglen,sizeof(*tok->arguments),CF_ALLOC_REALLOC);
        memset(&tok->arguments[tok->arglen-1],0,sizeof(*tok->arguments));

        #ifdef CF_CFG_TREE_DEBUG
        printf("%s[%s:%d]: returned from array value: %s\n",cn,__FILE__,__LINE__,cf_dbg_get_token(ret));
        #endif

        ret = cf_cfg_parser(stream,&tok->arguments[tok->arglen-1],NULL,level+1,CF_TOK_COMMARPAREN);
      } while(ret == CF_TOK_COMMA);

      if(ret != CF_TOK_RPAREN) return CF_RETVAL_PARSEERROR;

      prev = tok;
      cur  = cf_cfg_parser_insert_node(tr,cur,tok);
    }
    /* }}} */
    /* {{{ handle module path */
    else if(ret == CF_TOK_IDENT && cf_strcmp(stream->stok,"ModulePath") == 0) {
      if(cf_cfg_lexer(stream,1) != CF_TOK_SET) return CF_RETVAL_PARSEERROR;
      if(cf_cfg_lexer(stream,1) != CF_TOK_STRING) return CF_RETVAL_PARSEERROR;

      if(stream->modpath) return CF_RETVAL_PARSEERROR;
      stream->modpath = stream->stok;

      if(cf_cfg_lexer(stream,1) != CF_TOK_SEMI) return CF_RETVAL_PARSEERROR;
    }
    /* }}} */
    /* {{{ handle load */
    else if(ret == CF_TOK_IDENT && cf_strcmp(stream->stok,"Load") == 0) {
      if(cf_cfg_lexer(stream,1) != CF_TOK_STRING) return CF_RETVAL_PARSEERROR;
      tr->type = CF_TOK_LOAD;
      tr->data = stream->stok;
      if(cf_cfg_lexer(stream,1) != CF_TOK_SEMI) return CF_RETVAL_PARSEERROR;

      cf_cfg_parser_new_node(tr);
      cur = prev = NULL;
    }
    /* }}} */
    /* {{{ lbracket (array subscripts) need extra attention */
    else if(ret == CF_TOK_LBRACKET) {
      tok = cf_cfg_parser_newtok();
      tok->type = ret;

      tok->prec = CF_PREC_BR;

      prev = tok;
      cur  = cf_cfg_parser_insert_node(tr,cur,tok);
      tok  = NULL;

      if((ret = cf_cfg_parser(stream,tr,cur,level+1,CF_TOK_RBRACKET)) != CF_TOK_RBRACKET) return CF_RETVAL_PARSEERROR;
    }
    /* }}} */
    /* {{{ handle operator tokens */
    else if(ret > 0 && ret != CF_TOK_EOF) {
      tok = cf_cfg_parser_newtok();
      tok->type = ret;

      switch(ret) {
        case CF_TOK_DOT:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected dot!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_PT;
          break;

        case CF_TOK_MINUS:
        case CF_TOK_PLUS:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected +/-!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_PLUSMINUS;
          break;

        case CF_TOK_DIV:
        case CF_TOK_MULT:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected div/mul!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_DIVMUL;
          break;

        case CF_TOK_PERC:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected modulo!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_DIVMUL;
          break;

        case CF_TOK_NOTEQ:
        case CF_TOK_EQ:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected operator!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_EQ;
          break;

        case CF_TOK_SET:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected set!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_SET;
          break;

        case CF_TOK_NOT:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected not!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_NOT;
          break;

        case CF_TOK_GT:
        case CF_TOK_GTEQ:
        case CF_TOK_LT:
        case CF_TOK_LTEQ:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected operator!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_LTGT;
          break;

        case CF_TOK_AND:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected and!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_AND;
          break;

        case CF_TOK_OR:
          if(prev && cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected or!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }
          tok->prec = CF_PREC_OR;
          break;

        case CF_TOK_NUM:
          if(prev && !cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected integer!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }

          tok->prec = CF_PREC_ATOM;
          cf_cfg_parser_val_int(stream->numtok,tok);
          break;
        case CF_TOK_IDENT:
          #ifdef CF_CFG_TREE_DEBUG
          printf("%s[%s:%d]: ident is %s\n",cn,__FILE__,__LINE__,stream->stok);
          #endif

          if(prev && !cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected string!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }

          if(cf_cfg_lexer(stream,0) == CF_TOK_DOT) {
            #ifdef CF_CFG_TREE_DEBUG
            printf("%s[%s:%d]: changing token type to CF_TOK_FID\n",cn,__FILE__,__LINE__);
            #endif
            tok->type = CF_TOK_FID;
          }

          tok->prec = CF_PREC_ATOM;
          cf_cfg_parser_val_id(stream->stok,tok);
          break;

        case CF_TOK_STRING:
          if(prev && !cf_cfg_parser_isoperator(prev)) {
            fprintf(stderr,"[%s:%d] FATAL ERROR!! unexpected string!\n",__FILE__,__LINE__);
            return CF_RETVAL_PARSEERROR;
          }

          tok->prec = CF_PREC_ATOM;
          cf_cfg_parser_val_str(stream->stok,tok);
          break;
        case CF_TOK_DOLLAR:
          break;
        default:
          fprintf(stderr,"unexpected token type: %s\n",cf_dbg_get_token(ret));
          return CF_RETVAL_PARSEERROR;
      }

      prev = tok;
      cur  = cf_cfg_parser_insert_node(tr,cur,tok);
      tok  = NULL;
    }
    /* }}} */


    if(retat == CF_TOK_NEXTTOK) {
      #ifdef CF_CFG_TREE_DEBUG
      printf("%s[%s:%d]: returning 0x%X\n",cn,__FILE__,__LINE__,ret);
      #endif
      return ret;
    }

  } while(ret > 0 && ret != CF_TOK_EOF);


  return 0;
}
/* }}} */

/* {{{ cf_cfg_parser_destroy_tokens */
void cf_cfg_parser_destroy_tokens(cf_cfg_token_t *tok) {
  size_t i;

  if(!tok) return;

  if(tok->left) cf_cfg_parser_destroy_tokens(tok->left);
  if(tok->right) cf_cfg_parser_destroy_tokens(tok->right);

  for(i=0;i<tok->arglen;++i) cf_cfg_parser_destroy_tokens(tok->arguments[i].tree);

  free(tok->arguments);

  if(tok->data) {
    if(tok->data->type == CF_TYPE_STR || tok->data->type == CF_TYPE_ID) free(tok->data->sval);
    free(tok->data);
  }

  free(tok);
}
/* }}} */

/* {{{ cf_cfg_parser_destroy_stream */
void cf_cfg_parser_destroy_stream(cf_cfg_stream_t *stream) {
  if(stream->modpath) free(stream->modpath);
  cf_cfg_parser_destroy_trees(stream->trees);
  free(stream->content);
}
/* }}} */

/* {{{ cf_cfg_parser_destroy_trees */
void cf_cfg_parser_destroy_trees(cf_cfg_trees_t *tree) {
  cf_cfg_trees_t *tr,*tr1;
  int i;

  for(tr=tree;tr;tr=tr1) {
    cf_cfg_parser_destroy_tokens(tr->tree);

    for(i=0;i<tr->arglen;++i) cf_cfg_parser_destroy_trees(tr->arguments[i]);
    free(tr->arguments);

    if(tr->data) free(tr->data);

    tr1 = tr->next;
    free(tr);
  }

}
/* }}} */

/* eof */
