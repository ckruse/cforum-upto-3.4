/**
 * \file cfgcomp_vm.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This file contains the config vm
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

/* {{{ cf_cfg_vm_destroy_cmd */
void cf_cfg_vm_destroy_cmd(cf_cfg_vm_command_t *cmd) {
  size_t i;

  for(i=0;i<cmd->argcount;++i) {
    if(cmd->args[i].type == CF_ASM_ARG_STR && cmd->args[i].cval) free(cmd->args[i].cval);
  }

  free(cmd->args);
}
/* }}} */

/* {{{ cf_cfg_vm_fetch */
int cf_cfg_vm_fetch(cf_cfg_vm_t *me,cf_cfg_vm_command_t *cmd) {
  u_int16_t argdesc;
  u_char *end = me->content + me->len;
  size_t i;
  int32_t i32;

  /* first byte: the instruction itself */
  cmd->instruction = *me->pos;

  if(++me->pos == end || me->pos + 1 == end) {
    fprintf(stderr,"unexpected EOF!\n");
    return -1;
  }

  /* following: two bytes, describing count and type of arguments */
  argdesc = *me->pos << 8 | *++me->pos;
  ++me->pos;

  cmd->argcount = argdesc >> 13;

  cmd->args = cf_alloc(NULL,cmd->argcount,sizeof(*cmd->args),CF_ALLOC_CALLOC);

  for(i=0;i<cmd->argcount;++i) {
    cmd->args[i].type = (argdesc >> (13 - (i+1) * 2)) & 0x3;

    switch(cmd->args[i].type) {
      case CF_ASM_ARG_REG:
        cmd->args[i].bval = *me->pos;
        ++me->pos;
        break;
      case CF_ASM_ARG_NUM:
        cmd->args[i].i32val = *((int32_t *)me->pos);
        me->pos += 4;
        break;
      case CF_ASM_ARG_STR:
        i32 = *((int32_t *)me->pos);
        cmd->args[i].i32val = *((int32_t *)(me->content + i32));
        cmd->args[i].cval   = strndup(me->content + i32 + 4,cmd->args[i].i32val);
        me->pos += 4;
        break;
    }
  }

  return 0;
}
/* }}} */

/* {{{ cf_cleanup_register */
void cf_cleanup_register(cf_cfg_vm_val_t *reg) {
  if(reg->cval) free(reg->cval);
  memset(reg,0,sizeof(*reg));
}
/* }}} */

/* {{{ cf_cfg_vm_cpy_val_to_cfg */
void cf_cfg_vm_cpy_val_to_cfg(cf_cfg_config_value_t *cfg,cf_cfg_vm_val_t *val) {
  size_t i;

  switch(val->type) {
    case CF_ASM_ARG_REG:
      fprintf(stderr,"invalid cf_cfg_vm_cpy_val_to_cfg call!\n");
      exit(0);
    case CF_ASM_ARG_STR:
      cfg->sval = strdup(val->cval);
      break;
    case CF_ASM_ARG_NUM:
      cfg->ival = val->i32val;
      break;
    case CF_ASM_ARG_CFG:
      memcpy(cfg,val->cfgval,sizeof(*val->cfgval));
      break;
    case CF_ASM_ARG_ARY:
      cfg->avals = cf_alloc(NULL,1,sizeof(*cfg->avals),CF_ALLOC_CALLOC);
      for(i=0;i<val->alen;++i) cf_cfg_vm_cpy_val_to_cfg(&cfg->avals[i],&val->ary[i]);
      break;
  }
}
/* }}} */

/* {{{ cf_cfg_vm_copy_cfgval_to_vmval */
void cf_cfg_vm_cpy_cfgval_to_vmval(cf_cfg_config_value_t *cfg,cf_cfg_vm_val_t *val) {
  size_t i;

  memset(val,0,sizeof(*val));

  switch(cfg->type) {
    case CF_ASM_ARG_STR:
      val->cval = strdup(cfg->sval);
      break;
    case CF_ASM_ARG_NUM:
      val->i32val = cfg->ival;
      break;
    case CF_ASM_ARG_ARY:
      val->ary = cf_alloc(NULL,1,sizeof(*val->ary),CF_ALLOC_MALLOC);
      for(i=0;i<cfg->alen;++i) cf_cfg_vm_cpy_cfgval_to_vmval(&cfg->avals[i],&val->ary[i]);
      break;
  }
}
/* }}} */

/* {{{ cf_cfg_vm_copy_vmval_to_vmval */
void cf_cfg_vm_copy_vmval_to_vmval(cf_cfg_vm_val_t *val1,cf_cfg_vm_val_t *val2) {
  memcpy(val2,val1,sizeof(*val1));
}
/* }}} */

/* {{{ cf_cfg_vm_cfg_is_true */
int cf_cfg_vm_cfg_is_true(cf_cfg_config_value_t *cfg) {
  switch(cfg->type) {
    case CF_ASM_ARG_STR:
      return strlen(cfg->sval);
    case CF_ASM_ARG_NUM:
      return cfg->ival != 0;
    case CF_ASM_ARG_ARY:
      return cfg->alen != 0;
  }

  return 0;
}
/* }}} */

/* {{{ cf_cfg_vm_is_true */
int cf_cfg_vm_is_true(cf_cfg_vm_val_t *val) {
  switch(val->type) {
    case CF_ASM_ARG_STR:
      return strlen(val->cval);
    case CF_ASM_ARG_NUM:
      return val->i32val != 0;
      break;
    case CF_ASM_ARG_CFG:
      return cf_cfg_vm_cfg_is_true(val->cfgval);
    case CF_ASM_ARG_ARY:
      return val->alen > 0;
  }

  return 0;
}
/* }}} */

/* {{{ cf_cfg_vm_cmp_reg */
int cf_cfg_vm_cmp_reg(cf_cfg_vm_val_t *a,cf_cfg_vm_val_t *b) {
  u_char buff[512];
  int32_t i32;

  switch(a->type) {
    case CF_ASM_ARG_STR:
      if(b->type == CF_ASM_ARG_NUM) {
        snprintf(buff,512,"%"PRId32,b->i32val);
        return strcmp(a->cval,buff);
      }
      else if(b->type == CF_ASM_ARG_CFG) {
        if(b->cfgval->type == CF_ASM_ARG_STR) return strcmp(a->cval,b->cfgval->sval);
        else if(b->cfgval->type == CF_ASM_ARG_NUM) {
          snprintf(buff,512,"%"PRId32,b->cfgval->ival);
          return strcmp(a->cval,buff);
        }
        else return -2; /* failure */
      }
      else if(b->type == CF_ASM_ARG_STR) return strcmp(a->cval,b->cval);
      return -2; /* failure */

    case CF_ASM_ARG_NUM:
      if(b->type == CF_ASM_ARG_STR) {
        i32 = strtoll(b->cval,NULL,10);

        if(a->i32val == i32) return 0;
        else if(a->i32val < i32) return -1;
        else return 1;
      }
      else if(b->type == CF_ASM_ARG_NUM) {
        if(a->i32val == b->i32val) return 0;
        else if(a->i32val < b->i32val) return -1;
        else return 1;
      }
      else if(b->type == CF_ASM_ARG_CFG) {
        if(b->cfgval->type == CF_ASM_ARG_STR) {
          i32 = strtoll(b->cfgval->sval,NULL,10);

          if(a->i32val == i32) return 0;
          else if(a->i32val < i32) return -1;
          else return 1;
        }
        else if(b->cfgval->type == CF_ASM_ARG_NUM) {
          if(a->i32val == b->cfgval->ival) return 0;
          else if(a->i32val < b->cfgval->ival) return -1;
          else return 1;
        }
        else return -2; /* failure */
      }

      return -2; /* failure */

    case CF_ASM_ARG_CFG:
      switch(a->cfgval->type) {
        case CF_ASM_ARG_NUM:
          if(b->type == CF_ASM_ARG_NUM) {
            if(a->cfgval->ival == b->i32val) return 0;
            else if(a->cfgval->ival < b->i32val) return -1;
            else return 1;
          }
          else if(b->type == CF_ASM_ARG_STR) {
            i32 = strtoll(b->cval,NULL,10);

            if(a->cfgval->ival == i32) return 0;
            else if(a->cfgval->ival < i32) return -1;
            else return 1;
          }
          else if(b->type == CF_ASM_ARG_CFG) {
            if(b->cfgval->type == CF_ASM_ARG_NUM) {
              if(a->cfgval->ival == b->cfgval->ival) return 0;
              else if(a->cfgval->ival < b->cfgval->ival) return -1;
              else return 1;
            }
            else if(b->cfgval->type == CF_ASM_ARG_STR) {
              i32 = strtoll(b->cfgval->sval,NULL,10);

              if(a->cfgval->ival == i32) return 0;
              else if(a->cfgval->ival < i32) return -1;
              else return 1;
            }
            else return -2;
          }

          return -2;

        case CF_ASM_ARG_STR:
          if(b->type == CF_ASM_ARG_NUM) {
            snprintf(buff,512,"%"PRId32,b->i32val);
            return strcmp(a->cfgval->sval,buff);
          }
          else if(b->type == CF_ASM_ARG_STR) return strcmp(a->cfgval->sval,b->cval);
          else if(b->type == CF_ASM_ARG_CFG) {
            if(b->cfgval->type == CF_ASM_ARG_NUM) {
              snprintf(buff,512,"%"PRId32,b->cfgval->ival);
              return strcmp(a->cfgval->sval,buff);
            }
            else if(b->cfgval->type == CF_ASM_ARG_STR) return strcmp(a->cfgval->sval,b->cfgval->sval);
            else return -2;
          }

          return -2;

        default:
          return -2;
      }

      return -1;

    default:
      return -1;
  }
}
/* }}} */

int cf_cfg_vm_start(cf_cfg_vm_t *me,cf_cfg_config_t *cfg) {
  int ret,ret1;
  u_char *sval1,*sval2;
  cf_cfg_vm_command_t cmd;
  cf_tree_dataset_t dt,*dtp;
  cf_cfg_vm_val_t *dstreg,*srcreg,*srcreg1;
  int32_t i32;

  size_t i;
  cf_cfg_config_t *cfgns; /* namespace */

  me->pos = me->content;

  memset(me->registers,0,sizeof(*me->registers) * 256);

  while(me->pos < me->content + me->len) {
    memset(&cmd,0,sizeof(cmd));
    ret = cf_cfg_vm_fetch(me,&cmd);

    switch(cmd.instruction) {
      case CF_ASM_MODULE:
        break;

      case CF_ASM_SET:
        /* {{{ set config value in config */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"got SET with %zu arguments\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[1].type != CF_ASM_ARG_STR && cmd.args[1].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"got SET with invalid second argument!\n");
          return -1;
        }

        memset(&dt,0,sizeof(dt));
        dtp   = NULL;
        sval1 = NULL;

        if(cmd.argcount == 3) {
          if(cmd.args[2].type == CF_ASM_ARG_REG) sval1 = me->registers[cmd.args[2].bval].cval;
          else sval1 = cmd.args[2].cval;
        }

        if(cmd.args[1].type == CF_ASM_ARG_STR) {
          dt.key = cmd.args[1].cval;

          if(sval1 && cf_strcmp(sval1,"global") != 0) { /* we got a namespace */
            for(i=0;i<cfg->nmspcs.elements;++i) {
              cfgns = cf_array_element_at(&cfg->nmspcs,i);
              if(cf_strcmp(cfgns->name,sval1) == 0) {
                dtp = (cf_tree_dataset_t *)cf_tree_find(&cfgns->args,cfgns->args.root,&dt);
                break;
              }
            }
          }
          else dtp = (cf_tree_dataset_t *)cf_tree_find(&cfg->args,cfg->args.root,&dt);

          if(dtp) cf_cfg_vm_cpy_val_to_cfg((cf_cfg_config_value_t *)dtp->data,&cmd.args[0]);
          else { /* create new config value */
            dt.key  = strdup(cmd.args[1].cval);
            dt.data = cf_alloc(NULL,1,sizeof(cf_cfg_config_value_t),CF_ALLOC_CALLOC);
            cf_cfg_vm_cpy_val_to_cfg((cf_cfg_config_value_t *)dtp->data,&cmd.args[0]);

            if(sval1 && cf_strcmp(sval1,"global") != 0) {
              for(ret = 0,i=0;i<cfg->nmspcs.elements;++i) {
                cfgns = cf_array_element_at(&cfg->nmspcs,i);
                if(cf_strcmp(cfgns->name,sval1) == 0) {
                  ret = 1;
                  cf_tree_insert(&cfgns->args,NULL,&dt);
                  break;
                }
              }

              if(ret == 0) {
                cfgns = cf_alloc(NULL,1,sizeof(*cfgns),CF_ALLOC_CALLOC);
                cfgns->name = strdup(sval1);
                cf_tree_init(&cfgns->args,cf_cfg_cmp,NULL);
                cf_array_init(&cfgns->nmspcs,sizeof(*cfgns),(void (*)(void *))cf_cfg_config_destroy);
                cf_tree_insert(&cfgns->args,NULL,&dt);
              }
            }
            else cf_tree_insert(&cfg->args,NULL,&dt);
          }
        }
        else {
          if(cmd.argcount == 3) {
            fprintf(stderr,"we got SET reg,reg with 3 arguments!\n");
            return -1;
          }

          if(me->registers[cmd.args[1].bval].type != CF_ASM_ARG_CFG) {
            fprintf(stderr,"we got SET reg1,reg2 and reg2 is not a config value!\n");
            return -1;
          }

          dstreg = &me->registers[cmd.args[1].bval];

          memset(&dt,0,sizeof(dt));
          dt.key = dstreg->cfgval->name;

          if(cmd.args[0].type != CF_ASM_ARG_REG) cf_cfg_vm_cpy_val_to_cfg(dstreg->cfgval,&cmd.args[0]);
          else cf_cfg_vm_cpy_val_to_cfg(dstreg->cfgval,&me->registers[cmd.args[0].bval]);

          if(dstreg->cval && cf_strcmp(dstreg->cval,"global") != 0) {
            for(ret=0,i=0;i<cfg->nmspcs.elements;++i) {
              cfgns = cf_array_element_at(&cfg->nmspcs,i);
              if(cf_strcmp(cfgns->name,dstreg->cval) == 0) {
                ret = 1;
                if(cf_tree_find(&cfgns->args,cfgns->args.root,&dt) != NULL) ret = 2;
                break;
              }
            }

            if(ret == 0) {
              cfgns = cf_alloc(NULL,1,sizeof(*cfgns),CF_ALLOC_CALLOC);
              cfgns->name = strdup(dstreg->cval);
              cf_tree_init(&cfgns->args,cf_cfg_cmp,NULL);
              cf_array_init(&cfgns->nmspcs,sizeof(*cfgns),(void (*)(void *))cf_cfg_config_destroy);
            }

            if(ret == 1) {
              dt.key = strdup(dt.key);
              dt.data = dstreg->cfgval;
              cf_tree_insert(&cfgns->args,NULL,&dt);
            }
          }
          else {
            if(cf_tree_find(&cfg->args,cfg->args.root,&dt) == NULL) {
              dt.key = strdup(dt.key);
              dt.data = dstreg->cfgval;
              cf_tree_insert(&cfg->args,NULL,&dt);
            }
          }
        }
        /* }}} */
        break;

      case CF_ASM_PUSHDIR:
        break;

      case CF_ASM_UNSET:
        break;

      case CF_ASM_LOAD:
        /* {{{ load config value to register */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"got LOAD with %zu arguments\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) sval1 = me->registers[cmd.args[0].bval].cval;
        else sval1 = cmd.args[0].cval;

        if(cmd.args[1].type == CF_ASM_ARG_REG) sval2 = strdup(me->registers[cmd.args[1].bval].cval);
        else {
          if(cmd.argcount != 3) {
            fprintf(stderr,"Got LOAD str,str! Not allowed!\n");
            return -1;
          }

          sval2 = strdup(cmd.args[1].cval);
        }

        if(cmd.argcount == 3) dstreg = &me->registers[cmd.args[2].bval];
        else dstreg = &me->registers[cmd.args[1].bval];

        memset(&dt,0,sizeof(dt));

        dtp     = NULL;
        dt.key  = sval1;

        dstreg->type = CF_ASM_ARG_CFG;
        if(cf_strcmp(sval2,"global") != 0) {
          for(i=0;i<cfg->nmspcs.elements;++i) {
            cfgns = cf_array_element_at(&cfg->nmspcs,i);
            if(cf_strcmp(cfgns->name,sval2) == 0) {
              dtp = (cf_tree_dataset_t *)cf_tree_find(&cfgns->args,cfgns->args.root,&dt);
              break;
            }
          }
        }
        else dtp = (cf_tree_dataset_t *)cf_tree_find(&cfg->args,cfg->args.root,&dt);

        cf_cleanup_register(dstreg);
        dstreg->type = CF_ASM_ARG_CFG;

        if(!dtp) {
          if(cf_strcmp(sval2,"global") != 0) dstreg->cval = strdup(sval2);
          dstreg->cfgval = cf_alloc(NULL,1,sizeof(*dstreg->cfgval),CF_ALLOC_CALLOC);
          dstreg->cfgval->name = strdup(sval1);
        }
        else {
          if(cf_strcmp(sval2,"global") != 0) dstreg->cval = strdup(sval2);
          dstreg->cfgval = dtp->data;
        }

        free(sval2);
        /* }}} */
        break;

      case CF_ASM_CPY:
        /* {{{ copy value from src (register|memory) to register */
        if(cmd.argcount != 2) {
          fprintf(stderr,"got CPY with %zu arguments\n",cmd.argcount);
          return -1;
        }
        if(cmd.args[1].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"got CPY with second argument not an register!\n");
          return -1;
        }

        dstreg = &me->registers[cmd.args[1].bval];
        cf_cleanup_register(dstreg);

        if(cmd.args[0].type == CF_ASM_ARG_REG) {
          srcreg = &me->registers[cmd.args[0].bval];
          if(srcreg->type == CF_ASM_ARG_NUM) dstreg->i32val = srcreg->i32val;
          else if(srcreg->type == CF_ASM_ARG_STR) dstreg->cval = strdup(srcreg->cval);
          else if(srcreg->type == CF_ASM_ARG_CFG) {
            if(srcreg->cval) dstreg->cval = strdup(srcreg->cval);
            dstreg->cfgval = srcreg->cfgval;
          }
        }
        else if(cmd.args[0].type == CF_ASM_ARG_NUM) dstreg->i32val = cmd.args[0].i32val;
        else if(cmd.args[0].type == CF_ASM_ARG_STR) dstreg->cval   = strdup(cmd.args[0].cval);
        /* }}} */
        break;

      case CF_ASM_EQ:
        /* {{{ check if two values are uneqal */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"EQ with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"EQ with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"EQ with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret == 0;
        /* }}} */
        break;

      case CF_ASM_NE:
        /* {{{ check if two values are unequal */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"NE with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"NE with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"NE with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret != 0;
        /* }}} */
        break;

      case CF_ASM_LT:
        /* {{{ check if value a is lighter value b */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"LT with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"LT with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"LT with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret < 0;
        /* }}} */
        break;

      case CF_ASM_LTEQ:
        /* {{{ check if value a is lighter or equal to value b */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"LTEQ with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"LTEQ with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"LTEQ with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret <= 0;
        /* }}} */
        break;

      case CF_ASM_GT:
        /* {{{ check if value a is greater value b */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"GT with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"GT with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"GT with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret > 0;
        /* }}} */
        break;

      case CF_ASM_GTEQ:
        /* {{{ check if value a is lighter or equal to value b */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"GTEQ with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"GTEQ with two arguments and second argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"GTEQ with two arguments and third argument not a register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[0].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret = cf_cfg_vm_cmp_reg(srcreg,srcreg1);

        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret >= 0;
        /* }}} */
        break;

      case CF_ASM_ADD:
        break;
      case CF_ASM_SUB:
        break;
      case CF_ASM_DIV:
        break;
      case CF_ASM_MUL:
        break;
      case CF_ASM_JMP:
        /* {{{ jump to address */
        if(cmd.argcount != 1) {
          fprintf(stderr,"JMP with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        me->pos = me->content + cmd.args[0].i32val;
        /* }}} */
        break;

      case CF_ASM_JMPIF:
        /* {{{ jump to adress if reg is true */
        if(cmd.argcount != 2) {
          fprintf(stderr,"JMPIF with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[0].type != CF_ASM_ARG_NUM || cmd.args[1].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"JMPIF arguments have to be lbl,reg!\n");
          return -1;
        }

        if(cf_cfg_vm_is_true(&me->registers[cmd.args[1].bval])) me->pos = me->content + cmd.args[0].i32val;
        /* }}} */
        break;

      case CF_ASM_JMPIFNOT:
        /* {{{ jump to address if reg is false */
        if(cmd.argcount != 2) {
          fprintf(stderr,"JMPIFNOT with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[0].type != CF_ASM_ARG_NUM || cmd.args[1].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"JMPIFNOT arguments have to be lbl,reg!\n");
          return -1;
        }

        if(!cf_cfg_vm_is_true(&me->registers[cmd.args[1].bval])) me->pos = me->content + cmd.args[0].i32val;
        /* }}} */
        break;

      case CF_ASM_AND:
        /* {{{ AND-operation on two arguments */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"AND with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"AND with two arguments and second argument NOT an register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"AND with three arguments and third argument NOT an register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[1].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[1].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret  = cf_cfg_vm_is_true(srcreg);
        ret1 = cf_cfg_vm_is_true(srcreg1);

        cf_cleanup_register(dstreg);
        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret && ret1;
        /* }}} */
        break;

      case CF_ASM_OR:
        /* {{{ OR-operation on two arguments */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"OR with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"OR with two arguments and second argument NOT an register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg1 = dstreg;
        }
        else {
          if(cmd.args[2].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"OR with three arguments and third argument NOT an register!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
          if(cmd.args[1].type == CF_ASM_ARG_REG) srcreg1 = &me->registers[cmd.args[1].bval];
          else srcreg1 = &cmd.args[1];
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        ret  = cf_cfg_vm_is_true(srcreg);
        ret1 = cf_cfg_vm_is_true(srcreg1);

        cf_cleanup_register(dstreg);
        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = ret || ret1;
        /* }}} */
        break;

      case CF_ASM_NEG:
        /* {{{ Negates an argument */
        if(cmd.argcount < 1 || cmd.argcount > 2) {
          fprintf(stderr,"NEG with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.argcount == 1) {
          if(cmd.args[0].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"NEG with one argument and first argument NOT an register!\n");
            return -1;
          }

          dstreg = &me->registers[cmd.args[0].bval];
          srcreg = dstreg;
        }
        else {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"NEG with two arguments and second argument NOT an register!\n");
            return -1;
          }

          dstreg = &me->registers[cmd.args[1].bval];
          if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
          else srcreg = &cmd.args[0];
        }

        ret  = cf_cfg_vm_is_true(srcreg);
        cf_cleanup_register(dstreg);

        dstreg->type   = CF_ASM_ARG_NUM;
        dstreg->i32val = !ret;
        /* }}} */
        break;

      case CF_ASM_ARRAY:
        /* {{{ create array in register */
        if(cmd.argcount < 1 || cmd.argcount > 2) {
          fprintf(stderr,"ARRAY with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        dstreg = &me->registers[cmd.args[0].bval];
        cf_cleanup_register(dstreg);
        memset(dstreg,0,sizeof(*dstreg));

        dstreg->type = CF_ASM_ARG_ARY;
        if(cmd.argcount == 2) {
          dstreg->alen = cmd.args[1].i32val;
          dstreg->ary = cf_alloc(NULL,dstreg->alen,sizeof(*dstreg->ary),CF_ALLOC_CALLOC);
        }
        /* }}} */
        break;

      case CF_ASM_ARRAYSUBS:
        /* {{{ substitute array */
        if(cmd.argcount < 2 || cmd.argcount > 3) {
          fprintf(stderr,"ARRAYSUBS with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[0].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"ARRAYSUBS with first argument NOT a register!\n");
          return -1;
        }

        if(cmd.argcount == 2) {
          if(cmd.args[1].type != CF_ASM_ARG_REG) {
            fprintf(stderr,"ARRAYSUBS with two arguments and second argument NOT register!\n");
            return -1;
          }

          if(me->registers[cmd.args[1].bval].type != CF_ASM_ARG_NUM) {
            fprintf(stderr,"ARRAYSUBS with index not nummeric!\n");
            return -1;
          }

          dstreg  = &me->registers[cmd.args[1].bval];
          srcreg  = &me->registers[cmd.args[0].bval];

          i32 = dstreg->i32val;
        }
        else {
          srcreg  = &me->registers[cmd.args[0].bval];

          if(cmd.args[1].type == CF_ASM_ARG_REG) {
            if(me->registers[cmd.args[2].bval].type != CF_ASM_ARG_NUM) {
              fprintf(stderr,"ARRAYSUBS with index not nummeric!\n");
              return -1;
            }

            i32 = me->registers[cmd.args[2].bval].i32val;
          }
          else {
            if(cmd.args[1].type != CF_ASM_ARG_NUM) {
              fprintf(stderr,"ARRAYSUBS with index not nummeric!\n");
              return -1;
            }

            i32 = cmd.args[1].i32val;
          }

          dstreg  = &me->registers[cmd.args[2].bval];
        }

        if(srcreg->type != CF_ASM_ARG_CFG && srcreg->type != CF_ASM_ARG_ARY) {
          fprintf(stderr,"ARRAYSUBS and source arg is not an array!\n");
          return -1;
        }

        if(srcreg->cfgval->type == CF_ASM_ARG_CFG) {
          if(srcreg->alen <= (size_t)i32) {
            fprintf(stderr,"array index out of bounds!\n");
            return -1;
          }

          cf_cfg_vm_cpy_cfgval_to_vmval(&srcreg->cfgval->avals[i32],dstreg);
        }
        else {
          if(srcreg->alen <= (size_t)i32) {
            fprintf(stderr,"array index out of bounds!\n");
            return -1;
          }

          cf_cfg_vm_copy_vmval_to_vmval(&srcreg->ary[i32],dstreg);
        }
        /* }}} */
        break;

      case CF_ASM_ARRAYPUSH:
        /* {{{ push value to the end of an array */
        if(cmd.argcount != 2) {
          fprintf(stderr,"ARRAYPUSH with %zu arguments!\n",cmd.argcount);
          return -1;
        }

        if(cmd.args[1].type != CF_ASM_ARG_REG) {
          fprintf(stderr,"ARRAYPUSH with first argument not an register!\n");
          return -1;
        }

        dstreg = &me->registers[cmd.args[1].bval];
        if(dstreg->type != CF_ASM_ARG_ARY) {
          fprintf(stderr,"ARRAYPUSH with first argument not an array!\n");
          return -1;
        }

        if(cmd.args[0].type == CF_ASM_ARG_REG) srcreg = &me->registers[cmd.args[0].bval];
        else srcreg = &cmd.args[0];

        if(dstreg->pos >= dstreg->alen) dstreg->ary = cf_alloc(dstreg->ary,++dstreg->alen,sizeof(*dstreg->ary),CF_ALLOC_REALLOC);
        memcpy(&dstreg->ary[dstreg->pos++],srcreg,0);
        memset(&cmd.args[0],0,sizeof(cmd.args[0]));
        /* }}} */
        break;
    }

    cf_cfg_vm_destroy_cmd(&cmd);
  }

  return 0;
}


/* eof */
