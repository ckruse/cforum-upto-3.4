/**
 * \file configcodegenerator.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This file contains the config code generator
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

#define cf_cfg_gencode_register_set_used(vm,reg) (vm)->registers |= (1<<reg)
#define cf_cfg_gencode_register_set_unused(vm,reg) (vm)->registers &= ~(1<<reg)
#define cf_cfg_gencode_register_check_used(vm,reg) ((vm)->registers & (1<<reg))

#define cf_cfg_gencode_ng(state,reg) if((reg = cf_cfg_gencode_next_unused_register(state)) == -1) return -1;

/* {{{ cf_cfg_gencode_next_unused_register */
int cf_cfg_gencode_next_unused_register(cf_cfg_vmstate_t *state) {
  int i;

  for(i=0;i<(1<<sizeof(char) * 8) && cf_cfg_gencode_register_check_used(state,i) != 0;++i);

  if(cf_cfg_gencode_register_check_used(state,i) != 0) {
    fprintf(stderr,"%s[%d]: fatal error: all registers used!\n",__FILE__,__LINE__);
    return -1;
  }

  return i;
}
/* }}} */

/* {{{ cf_cfg_codegen_append_str */
void cf_cfg_codegen_append_str(cf_string_t *str,const u_char *ptr) {
  for(;*ptr;++ptr) {
    switch(*ptr) {
      case '"':
        cf_str_chars_append(str,"\\\"",2);
        break;
      case '\\':
        cf_str_chars_append(str,"\\\\",2);
        break;
      case '\012':
        cf_str_chars_append(str,"\\n",2);
        break;
      case '\015':
        cf_str_chars_append(str,"\\r",2);
        break;
      default:
        cf_str_char_append(str,*ptr);
    }
  }
}
/* }}} */

/* {{{ cf_cfg_codegenerator_tokens */
int cf_cfg_codegenerator_tokens(cf_cfg_stream_t *stream,cf_cfg_token_t *tok,cf_cfg_vmstate_t *state,cf_string_t *str,int regarg) {
  int reg,reg1;
  char buff[512];
  size_t len,i;

  #ifdef CF_CODEGEN_DEBUG
  static const u_char cn[] = "cf_cfg_codegenerator_tokens";

  printf("%s[%s:%d]: called (0x%X)!\n",cn,__FILE__,__LINE__,tok->type);
  #endif


  switch(tok->type) {
    case CF_TOK_DOT:
      reg = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);

      if(tok->right->type != CF_TOK_IDENT) {
        fprintf(stderr,"[%s:%d]: dot-notation but right child is NOT an identifier!! This is a real error!\n",__FILE__,__LINE__);
        return -1;
      }

      cf_str_cstr_append(str,"LOAD \"");
      cf_cfg_codegen_append_str(str,tok->right->data->sval);
      cf_str_cstr_append(str,"\",");
      cf_str_cstr_append(str,"reg");
      cf_uint16_to_str(str,reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,reg);
      cf_str_char_append(str,'\n');

      return reg;

    case CF_TOK_MINUS:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"SUB reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_PLUS:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"ADD reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_DIV:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"DIV reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_MULT:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"MUL reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_PERC:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"MOD reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_NOTEQ:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"NE reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_EQ:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"EQ reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_SET:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"SET reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      cf_cfg_gencode_register_set_unused(state,reg1);
      break;

    case CF_TOK_NOT:
      reg = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      cf_str_cstr_append(str,"NEG reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      return reg;

    case CF_TOK_GT:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"GT reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_GTEQ:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"GTE reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_LT:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"LT reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_LTEQ:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"LTE reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_AND:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"AND reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_OR:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      if(reg < 0 || reg1 < 0) return -1;

      cf_str_cstr_append(str,"OR reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);
      return reg1;

    case CF_TOK_NUM:
      cf_cfg_gencode_ng(state,reg);
      len = snprintf(buff,512,"%"PRId32,tok->data->ival);

      cf_str_cstr_append(str,"CPY ");
      cf_str_chars_append(str,buff,len);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_used(state,reg);
      return reg;

    case CF_TOK_IDENT:
      cf_cfg_gencode_ng(state,reg);
      cf_cfg_gencode_register_set_used(state,reg);

      cf_str_cstr_append(str,"LOAD \"");
      cf_cfg_codegen_append_str(str,tok->data->sval);
      cf_str_cstr_append(str,"\",");

      if(regarg != -1) {
        cf_str_cstr_append(str,"reg");
        cf_uint16_to_str(str,(u_int16_t)regarg);
      }
      else cf_str_cstr_append(str,"\"global\"");
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,reg);
      cf_str_char_append(str,'\n');

      return reg;

    case CF_TOK_ARRAY:
      cf_cfg_gencode_ng(state,reg);
      cf_cfg_gencode_register_set_used(state,reg);

      cf_str_cstr_append(str,"ARRAY reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,',');
      cf_uint32_to_str(str,(u_int32_t)tok->arglen);
      cf_str_char_append(str,'\n');

      for(i=0;i<tok->arglen;++i) {
        if((reg1 = cf_cfg_codegenerator_tokens(stream,tok->arguments[i].tree,state,str,-1)) < 0) return -1;

        cf_str_cstr_append(str,"ARRAYPUSH reg");
        cf_uint16_to_str(str,(u_int16_t)reg1);
        cf_str_cstr_append(str,",reg");
        cf_uint16_to_str(str,(u_int16_t)reg);
        cf_str_char_append(str,'\n');

        cf_cfg_gencode_register_set_unused(state,reg1);
      }

      return reg;

    case CF_TOK_LBRACKET:
      reg  = cf_cfg_codegenerator_tokens(stream,tok->left,state,str,regarg);
      reg1 = cf_cfg_codegenerator_tokens(stream,tok->right,state,str,regarg);

      cf_str_cstr_append(str,"ARRAYSUBS reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_cstr_append(str,",reg");
      cf_uint16_to_str(str,(u_int16_t)reg1);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_unused(state,reg);

      return reg1;

    case CF_TOK_FID:
    case CF_TOK_STRING:
      cf_cfg_gencode_ng(state,reg);

      cf_str_cstr_append(str,"CPY \"");
      cf_cfg_codegen_append_str(str,tok->data->sval);
      cf_str_cstr_append(str,"\",reg");
      cf_uint16_to_str(str,(u_int16_t)reg);
      cf_str_char_append(str,'\n');

      cf_cfg_gencode_register_set_used(state,reg);
      return reg;

    case CF_TOK_DOLLAR:
      break;

    default:
      fprintf(stderr,"unexpected token type: %s\n",cf_dbg_get_token(tok->type));
      return CF_RETVAL_PARSEERROR;
  }

  return 0;
}

/* }}} */

/* {{{ cf_cfg_codegenerator */
int cf_cfg_codegenerator(cf_cfg_stream_t *stream,cf_cfg_trees_t *trees,cf_cfg_vmstate_t *state,cf_string_t *str,int regarg) {
  cf_cfg_trees_t *tr;
  int lbl,lbl1,lbl2,i,j,had_elif;

  #ifdef CF_CODEGEN_DEBUG
  static const u_char cn[] = "cf_cfg_codegenerator";
  #endif

  #ifdef CF_CODEGEN_DEBUG
  printf("%s[%s:%d]: called (0x%X)!\n",cn,__FILE__,__LINE__,trees->type);
  #endif

  for(tr=trees;tr && tr->type;tr=tr->next) {
    #ifdef CF_CODEGEN_DEBUG
    printf("%s[%s:%d]: token type: %s (0x%X)\n",cn,__FILE__,__LINE__,cf_dbg_get_token(tr->type),tr->type);
    #endif

    if(tr->tree == NULL && tr->arguments == NULL && tr->data == NULL) continue;

    switch(tr->type) {
      case CF_TOK_IF:
        lbl = state->lbls_used++;
        lbl1 = state->lbls_used++;
        lbl2 = state->lbls_used++;
        had_elif = 0;

        if((i = cf_cfg_codegenerator_tokens(stream,tr->tree,state,str,regarg)) < 0) return i;

        cf_str_cstr_append(str,"JMPIFNOT label");
        if(tr->arglen == 2) cf_uint32_to_str(str,(u_int32_t)lbl1);
        else cf_uint32_to_str(str,(u_int32_t)lbl);
        cf_str_cstr_append(str,",reg");
        cf_uint16_to_str(str,(u_int16_t)i);
        cf_str_char_append(str,'\n');

        cf_cfg_gencode_register_set_unused(state,i); /* we no longer need this register */

        /* handle body */
        if(cf_cfg_codegenerator(stream,tr->arguments[0],state,str,regarg) != 0) return -1;

        if(tr->arguments[tr->arglen-1]->type == CF_TOK_ELSE) { /* we have got an else */
          cf_str_cstr_append(str,"JMP label");
          cf_uint32_to_str(str,(u_int32_t)(tr->arglen>2?lbl2:lbl));
          cf_str_char_append(str,'\n');
        }

        for(i=1;i<tr->arglen;++i) {
          if(tr->arguments[i]->type != CF_TOK_ELSE) {
            had_elif = 1;
            cf_str_cstr_append(str,"label");
            cf_uint32_to_str(str,(u_int32_t)lbl);
            cf_str_chars_append(str,":\n",2);

            lbl = state->lbls_used++;
            if((j = cf_cfg_codegenerator_tokens(stream,tr->arguments[i]->tree,state,str,regarg)) < 0) return j;

            cf_str_cstr_append(str,"JMPIFNOT label");
            if(tr->arglen-1 > i+1 || tr->arguments[tr->arglen-1]->type != CF_TOK_ELSE) cf_uint32_to_str(str,(u_int32_t)lbl);
            else cf_uint32_to_str(str,(u_int32_t)lbl1);
            cf_str_cstr_append(str,",reg");
            cf_uint16_to_str(str,(u_int16_t)j);
            cf_str_char_append(str,'\n');

            cf_cfg_gencode_register_set_unused(state,j); /* we no longer need this register */

            /* handle body */
            if(cf_cfg_codegenerator(stream,tr->arguments[i]->arguments[0],state,str,regarg) != 0) return -1;

            cf_str_cstr_append(str,"JMP label");
            cf_uint32_to_str(str,(u_int32_t)lbl2);
            cf_str_char_append(str,'\n');

            cf_cfg_gencode_register_set_unused(state,i); /* we no longer need this register */
          }
        }

        if(tr->arguments[tr->arglen-1]->type == CF_TOK_ELSE) {
          cf_str_chars_append(str,"label",5);
          cf_uint32_to_str(str,(u_int32_t)lbl1);
          cf_str_chars_append(str,":\n",2);
          /* else body */
          if(cf_cfg_codegenerator(stream,tr->arguments[tr->arglen-1]->arguments[0],state,str,regarg) != 0) return -1;
        }

        cf_str_chars_append(str,"label",5);
        cf_uint32_to_str(str,(u_int32_t)(had_elif?lbl2:lbl));
        cf_str_chars_append(str,":\n",2);
        break;

      case CF_TOK_WITH:
        if((i = cf_cfg_gencode_next_unused_register(state)) == -1) return -1;

        cf_str_cstr_append(str,"CPY \"");
        cf_cfg_codegen_append_str(str,tr->data);
        cf_str_chars_append(str,"\",reg",5);
        cf_uint16_to_str(str,(u_int16_t)i);
        cf_str_char_append(str,'\n');

        cf_cfg_gencode_register_set_used(state,i);

        /* gen body */
        #ifdef CF_CODEGEN_DEBUG
        printf("%s[%s:%d]: calling code generator (0x%p)!\n",cn,__FILE__,__LINE__,tr->arguments[0]);
        #endif
        if(cf_cfg_codegenerator(stream,tr->arguments[0],state,str,i) != 0) return -1;

        cf_cfg_gencode_register_set_unused(state,i);
        break;

      case CF_TOK_LOAD:
        if(!stream->modpath) {
          fprintf(stderr,"[%s:%d]: FATAL ERRROR! module path not set!\n\n",__FILE__,__LINE__);
          return -1;
        }

        cf_str_cstr_append(str,"MODULE \"");
        cf_cfg_codegen_append_str(str,stream->modpath);
        if(stream->modpath[strlen(stream->modpath)-1] != '/') cf_str_char_append(str,'/');
        cf_cfg_codegen_append_str(str,tr->data);
        cf_str_cstr_append(str,"\",\"");
        cf_cfg_codegen_append_str(str,tr->data);
        cf_str_cstr_append(str,"\"\n");
        break;

      case CF_TOK_ELSE:
      case CF_TOK_ELSEIF:
      case CF_TOK_STMT:
        if((i = cf_cfg_codegenerator_tokens(stream,tr->tree,state,str,regarg)) == -1) return -1;
        break;

      default:
        fprintf(stderr,"[%s:%d]: FATAL ERROR!! unexpected token type! (0x%X)\n",__FILE__,__LINE__,tr->type);
    }
  }

  return 0;
}

/* }}} */


/* eof */
