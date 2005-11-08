/**
 * \file configassembler.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This file contains the config assembler
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

/* {{{ cf_cfg_asm_line */
u_char *cf_cfg_asm_line(const u_char *input,u_char **pos) {
  register u_char *ptr;
  u_char *tmp;

  if(*pos && **pos == '\0') return NULL;

  for(ptr=*pos?*pos:(u_char *)input;*ptr;++ptr) {
    if(*ptr == '\n') {
      if(*pos) tmp = strndup(*pos,ptr-*pos);
      else tmp = strndup(input,ptr-input);

      *pos = ptr+1;
      return tmp;
    }
  }

  *pos = ptr;
  return strdup(input);
}
/* }}} */

/* {{{ cf_cfg_asm_readstr */
const u_char *cf_cfg_asm_readstr(const u_char *ptr,u_char **save) {
  cf_string_t str;

  cf_str_init_growth(&str,256);
  for(;*ptr && *ptr != '"';++ptr) {
    if(*ptr == '\\') {
      switch(*++ptr) {
        case '"':
          cf_str_char_append(&str,'"');
          break;
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
    else cf_str_char_append(&str,*ptr);
  }

  if(*ptr == '\0') {
    cf_str_cleanup(&str);
    return NULL;
  }

  *save = str.content;
  return ptr;
}
/* }}} */

/* {{{ cf_cfg_asm_tokenizer */
size_t cf_cfg_asm_tokenizer(const u_char *line,cf_cfg_asm_tok_t **tokens) {
  u_char *last;
  register u_char *ptr;
  size_t llen = 0;
  cf_cfg_asm_tok_t *list = NULL;

  /* get command */
  for(last=ptr=(u_char *)line;*ptr && !isspace(*ptr);++ptr);
  list = cf_alloc(list,++llen,sizeof(*list),CF_ALLOC_REALLOC);
  list[llen-1].val  = strndup(last,ptr-last);
  list[llen-1].type = CF_ASM_T_ATOM;

  if(*ptr == '\0' && *(ptr-1) == ':') { /* label */
    list[llen-1].val[strlen(list[llen-1].val)-1] = '\0';
    list[llen-1].type = CF_ASM_T_LBL;

    *tokens = list;
    return llen;
  }

  last = ptr;

  for(;*ptr;++ptr) {
    for(;*ptr && isspace(*ptr);++ptr);

    if(*ptr == '"') {
      list = cf_alloc(list,++llen,sizeof(*list),CF_ALLOC_REALLOC);
      ptr  = (u_char *)cf_cfg_asm_readstr(ptr+1,&list[llen-1].val);
      list[llen-1].type = CF_ASM_T_STR;
      last = ptr;
    }
    else if(*ptr) {
      if(*(ptr+1) == '"') continue;
      for(last=ptr++;*ptr && *ptr != ',';++ptr);
      if(*last == ',') ++last;

      if(*last) {
        list = cf_alloc(list,++llen,sizeof(*list),CF_ALLOC_REALLOC);
        list[llen-1].val  = strndup(last,ptr-last);
        list[llen-1].type = CF_ASM_T_ATOM;
        last = ptr+1;
      }

      --ptr;
    }
  }

  *tokens = list;
  return llen;
}
/* }}} */

/* {{{ cf_asm_register_replacement */
void cf_asm_register_replacement(cf_array_t *repl,const u_char *name,u_int32_t lbl_off,u_int32_t repl_off) {
  size_t j;
  cf_asm_replacements_t *r,r1;

  for(j=0;j<repl->elements;++j) {
    r = cf_array_element_at(repl,j);

    if(cf_strcmp(r->name,name) == 0) {
      if(lbl_off) r->lbl_off = lbl_off;
      if(repl_off) {
        r->repl_offs = cf_alloc(r->repl_offs,++r->rlen,sizeof(repl_off),CF_ALLOC_REALLOC);
        r->repl_offs[r->rlen-1] = repl_off;
      }

      return;
    }
  }

  r1.name      = strdup(name);
  r1.lbl_off   = lbl_off;
  r1.rlen      = 0;
  r1.repl_offs = NULL;

  if(repl_off) {
    r1.repl_offs = cf_alloc(r1.repl_offs,++r1.rlen,sizeof(repl_off),CF_ALLOC_REALLOC);
    r1.repl_offs[r1.rlen-1] = repl_off;
  }

  cf_array_push(repl,&r1);
}
/* }}} */

/* {{{ cf_cfg_assembler_destroy_replacement */
void cf_cfg_assembler_destroy_replacement(cf_asm_replacements_t *repl) {
  free(repl->name);
  if(repl->repl_offs) free(repl->repl_offs);
}
/* }}} */

/* {{{ cf_cfg_assembler */
int cf_cfg_assembler(cf_string_t *asmb,cf_string_t *bc) {
  u_char *line,*pos = NULL;
  cf_cfg_asm_tok_t *tokens;
  size_t tnum,i;
  int j;

  char reg;
  int32_t num;
  u_int32_t unum;

  u_int16_t argdesc;

  cf_string_t stringargs,itself,buff;
  cf_asm_replacements_t *repl;
  cf_array_t replacements;

  cf_array_init(&replacements,sizeof(*repl),(void (*)(void *))cf_cfg_assembler_destroy_replacement);

  cf_str_init(&stringargs);
  cf_str_init(&itself);

  num = 0;
  cf_str_char_append(&stringargs,CF_ASM_JMP);
  argdesc = 1;
  argdesc <<= 13;
  argdesc |= CF_ASM_ARG_NUM<<11;
  cf_str_char_append(&stringargs,argdesc>>8); /* higher 8 bit */
  cf_str_char_append(&stringargs,argdesc & ((1<<(8))-1)); /* lower 8 bit */
  cf_str_chars_append(&stringargs,(u_char *)&num,4);

  while((line = cf_cfg_asm_line(asmb->content,&pos)) != NULL) {
    tnum = cf_cfg_asm_tokenizer(line,&tokens);

    if(tokens[0].type != CF_ASM_T_ATOM && tokens[0].type != CF_ASM_T_LBL) {
      fprintf(stderr,"[%s:%d]: ERROR! first token is not an atom!\n",__FILE__,__LINE__);
      return CF_RETVAL_PARSEERROR;
    }

    /* {{{ encode directives */
    if(cf_strcmp(tokens[0].val,"MODULE") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for MODULE!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_MODULE);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"PUSH") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for PUSH!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_PUSH);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"UNSET") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for UNSET!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_UNSET);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"LOAD") == 0) {
      if(tnum != 4) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for LOAD!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_LOAD);
      argdesc = 3;
    }
    else if(cf_strcmp(tokens[0].val,"CPY") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for CPY!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_CPY);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"EQ") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for EQ!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_EQ);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"NE") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for NE!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_NE);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"LT") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for LT!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_LT);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"LTEQ") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for LTEQ!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_LTEQ);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"GT") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for GT!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_GT);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"GTEQ") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for GTEQ!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_GTEQ);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"ADD") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for ADD!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_ADD);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"SUB") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for SUB!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_SUB);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"DIV") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for DIV!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_DIV);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"MUL") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for MUL!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_MUL);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"JMP") == 0) {
      if(tnum != 2) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for JMP!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_CPY);
      argdesc = 1;

      cf_asm_register_replacement(&replacements,tokens[1].val,0,itself.len+2);
      free(tokens[1].val);
      tokens[1].val  = strdup("0");
      tokens[1].type = CF_ASM_T_ATOM;
    }
    else if(cf_strcmp(tokens[0].val,"JMPIF") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for JMPIF!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_JMPIF);
      argdesc = 2;

      cf_asm_register_replacement(&replacements,tokens[1].val,0,itself.len+2);
      free(tokens[1].val);
      tokens[1].val  = strdup("0");
      tokens[1].type = CF_ASM_T_ATOM;
    }
    else if(cf_strcmp(tokens[0].val,"JMPIFNOT") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for JMPIFNOT!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_JMPIFNOT);
      argdesc = 2;

      cf_asm_register_replacement(&replacements,tokens[1].val,0,itself.len+2);
      free(tokens[1].val);
      tokens[1].val  = strdup("0");
      tokens[1].type = CF_ASM_T_ATOM;
    }
    else if(cf_strcmp(tokens[0].val,"AND") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for AND!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_AND);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"OR") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for OR!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_OR);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"ARRAY") == 0) {
      if(tnum != 2) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for ARRAY!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_ARRAY);
      argdesc = 1;
    }
    else if(cf_strcmp(tokens[0].val,"ARRAYSUBS") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for ARRAYSUBS!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_ARRAYSUBS);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"ARRAYPUSH") == 0) {
      if(tnum != 3) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for ARRAYPUSH!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_ARRAYPUSH);
      argdesc = 2;
    }
    else if(cf_strcmp(tokens[0].val,"NEG") == 0) {
      if(tnum != 2) {
        fprintf(stderr,"[%s:%d]: ERROR! wrong argument count %zu for NEG!\n",__FILE__,__LINE__,tnum);
        return CF_RETVAL_PARSEERROR;
      }

      cf_str_char_append(&itself,CF_ASM_NEG);
      argdesc = 1;
    }
    else if(tokens[0].type == CF_ASM_T_LBL) {
      cf_asm_register_replacement(&replacements,tokens[0].val,itself.len,0);
      free(tokens[0].val);
      free(tokens);
      tokens = NULL;
      continue;
    }
    else {
      fprintf(stderr,"[%s:%d]: FATAL ERROR!! unknown directive %s\n",__FILE__,__LINE__,tokens[0].val);
      return -1;
    }
    /* }}} */

    /* {{{ encode argument description */
    argdesc <<= 13;

    for(i=1;i<tnum;++i) {
      if(tokens[i].type == CF_ASM_T_ATOM) {
        if(cf_strncmp(tokens[i].val,"reg",3) == 0) argdesc |= CF_ASM_ARG_REG<<(16-3-(i*2));
        else argdesc |= CF_ASM_ARG_NUM<<(16-3-(i*2));
      }
      else argdesc |= CF_ASM_ARG_STR<<(16-3-(i*2));
    }

    cf_str_char_append(&itself,argdesc>>8); /* higher 8 bit */
    cf_str_char_append(&itself,argdesc & ((1<<(8))-1)); /* lower 8 bit */
    /* }}} */

    for(i=1;i<tnum;++i) {
      if(tokens[i].type == CF_ASM_T_ATOM) {
        if(cf_strncmp(tokens[i].val,"reg",3) == 0) {
          reg = *(tokens[i].val + 3) - '0';
          cf_str_char_append(&itself,reg);
        }
        else {
          num = strtoll(tokens[i].val,NULL,10);
          cf_str_chars_append(&itself,(u_char *)&num,4);
        }
      }
      else {
        unum = stringargs.len;
        cf_str_chars_append(&itself,(u_char *)&unum,4);
        num = strlen(tokens[i].val);
        cf_str_chars_append(&stringargs,(u_char *)&num,4);
        cf_str_cstr_append(&stringargs,tokens[i].val);
      }

      free(tokens[i].val);
    }

    free(tokens[0].val);
    free(tokens);
    tokens = NULL;

    free(line);
  }

  /* second phase: replace offsets for JMP{IF,IFNOT} */
  cf_str_init_growth(&buff,10);
  for(i=0;i<replacements.elements;++i) {
    repl = cf_array_element_at(&replacements,i);
    buff.len = 0;
    unum     = repl->lbl_off + stringargs.len; /* label is offset in itself + length of stringargs */
    cf_str_chars_append(&buff,(u_char *)&unum,4);

    for(j=0;j<repl->rlen;++j) memcpy(itself.content+repl->repl_offs[j],buff.content,4);
  }

  cf_str_str_append(bc,&stringargs);
  cf_str_str_append(bc,&itself);

  itself.len = 0;
  unum = stringargs.len;
  cf_str_chars_append(&itself,(u_char *)&unum,4);

  memcpy(bc->content+3,itself.content,4);

  cf_str_cleanup(&buff);
  cf_str_cleanup(&itself);
  cf_str_cleanup(&stringargs);

  cf_array_destroy(&replacements);

  return 0;
}
/* }}} */

/* {{{ cf_cfg_assemble */
int cf_cfg_assemble(const u_char *filename,cf_string_t *str) {
  FILE *fd;
  struct stat st;
  cf_cfg_stream_t stream;
  cf_cfg_token_t tree;
  int retval;
  cf_cfg_vmstate_t state;
  size_t r;
  cf_string_t str1;

  memset(&stream,0,sizeof(stream));
  memset(&tree,0,sizeof(tree));
  memset(&state,0,sizeof(state));

  cf_str_init(str);
  cf_str_init(&str1);

  if(stat(filename,&st) == -1) return CF_RETVAL_NOSUCHFILE;

  if((fd = fopen(filename,"r")) == NULL) return CF_RETVAL_FILENOTREADABLE;

  stream.content = cf_alloc(NULL,1,st.st_size+1,CF_ALLOC_MALLOC);
  r = fread(stream.content,1,st.st_size,fd);
  fclose(fd);

  stream.content[r] = '\0';

  if((retval = cf_cfg_parser(&stream,NULL,NULL,0,0)) != 0) {
    cf_cfg_parser_destroy_stream(&stream);
    return retval;
  }

  if((retval = cf_cfg_codegenerator(&stream,stream.trees,&state,&str1,-1)) != 0) {
    cf_cfg_parser_destroy_stream(&stream);
    cf_str_cleanup(&str1);
    return retval;
  }

  cf_cfg_parser_destroy_stream(&stream);

  if(str1.content == NULL) return -1;

  if((retval = cf_cfg_assembler(&str1,str)) != 0) {
    cf_str_cleanup(&str1);
    cf_str_cleanup(str);
    return retval;
  }

  cf_str_cleanup(&str1);

  return CF_RETVAL_OK;
}
/* }}} */

int main(int argc,char *argv[]) {
  cf_string_t str;

  if(argc < 2) {
    fprintf(stderr,"usage: %s <file>\n",argv[0]);
    return EXIT_FAILURE;
  }

  memset(&str,0,sizeof(str));
  fprintf(stderr,"retval: %d\n",cf_cfg_assemble(argv[1],&str));
  fwrite(str.content,1,str.len,stdout);
  cf_str_cleanup(&str);

  return EXIT_SUCCESS;
}

/* eof */
