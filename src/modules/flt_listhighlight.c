/**
 * \file flt_listhighlight.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements the highlighting of VIP persons, etc
 */

/* {{{ Initial comments */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

struct {
  int HighlightOwnPostings;
  u_char *OwnPostingsColorF;
  u_char *OwnPostingsColorB;
  t_cf_hash *WhiteList;
  u_char *WhiteListColorF;
  u_char *WhiteListColorB;
  t_cf_hash *HighlightCategories;
  u_char *CategoryHighlightColorF;
  u_char *CategoryHighlightColorB;
  t_cf_hash *VIPList;
  u_char *VIPColorF;
  u_char *VIPColorB;
} Cfg = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* {{{ flt_lh_parse_list */
void flt_lh_parse_list(u_char *vips,t_cf_hash *hash) {
  if(vips) {
    u_char *ptr = vips;
    u_char *pos = ptr,*pre = ptr;

    while((pos = strstr(pos,",")) != NULL) {
      *pos = 0;

      cf_hash_set(hash,pre,pos-pre,"1",1);

      pre    = pos+1;
      *pos++ = ',';
    }

    cf_hash_set(hash,pre,strlen(pre),"1",1); /* argh! This sucks */
  }
}
/* }}} */

/* {{{ flt_lh_tolwer */
u_char *flt_lh_tolwer(const u_char *str,register int *len) {
  register u_char *ptr = (u_char *)str,*ptr1;
  u_char *result = strdup(str);

  *len = 0;
  ptr1 = result;

  while(*ptr) {
    *ptr1++ = tolower(*ptr++);
    *len += 1;
  }

  return result;
}
/* }}} */

/* {{{ flt_lh_execute_filter */
int flt_lh_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  t_name_value *uname = NULL;
  int len = 0;
  u_char *tmp;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  /*
   * Initialization
   */
  if(UserName) uname = cfg_get_first_value(vc,NULL,"Name");

  if(!Cfg.VIPList && !Cfg.WhiteList && !Cfg.HighlightCategories && !Cfg.HighlightOwnPostings) return FLT_DECLINE;

  if(Cfg.VIPList) {
    tmp = flt_lh_tolwer(msg->author.content,&len);
    if(cf_hash_get(Cfg.VIPList,tmp,len)) cf_tpl_setvalue(&msg->tpl,"vip",TPL_VARIABLE_STRING,"1",1);
    free(tmp);
  }

  if(Cfg.WhiteList) {
    tmp = flt_lh_tolwer(msg->author.content,&len);
    if(cf_hash_get(Cfg.WhiteList,tmp,len)) cf_tpl_setvalue(&msg->tpl,"whitelist",TPL_VARIABLE_STRING,"1",1);
    free(tmp);
  }

  if(Cfg.HighlightCategories && msg->category.len) {
    if(cf_hash_get(Cfg.HighlightCategories,msg->category.content,msg->category.len)) cf_tpl_setvalue(&msg->tpl,"cathigh",TPL_VARIABLE_STRING,"1",1);
  }

  if(Cfg.HighlightOwnPostings && uname) {
    if(cf_strcasecmp(msg->author.content,uname->values[0]) == 0) cf_tpl_setvalue(&msg->tpl,"ownposting",TPL_VARIABLE_STRING,"1",1);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_listhighlight_cleanup */
void flt_listhighlight_cleanup(void) {
  if(Cfg.OwnPostingsColorF)       free(Cfg.OwnPostingsColorF);
  if(Cfg.OwnPostingsColorB)       free(Cfg.OwnPostingsColorB);
  if(Cfg.WhiteList)               cf_hash_destroy(Cfg.WhiteList);
  if(Cfg.WhiteListColorF)         free(Cfg.WhiteListColorF);
  if(Cfg.WhiteListColorB)         free(Cfg.WhiteListColorB);
  if(Cfg.HighlightCategories)     cf_hash_destroy(Cfg.HighlightCategories);
  if(Cfg.CategoryHighlightColorF) free(Cfg.CategoryHighlightColorF);
  if(Cfg.CategoryHighlightColorB) free(Cfg.CategoryHighlightColorB);
  if(Cfg.VIPList)                 cf_hash_destroy(Cfg.VIPList);
  if(Cfg.VIPColorF)               free(Cfg.VIPColorF);
  if(Cfg.VIPColorB)               free(Cfg.VIPColorB);
}
/* }}} */

/* {{{ flt_lh_set_colors */
int flt_lh_set_colors(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  t_name_value *cs = cfg_get_first_value(dc,NULL,"ExternCharset");

  if(Cfg.VIPColorF || Cfg.VIPColorB) {
    cf_tpl_setvalue(begin,"vipcol",TPL_VARIABLE_STRING,"1",1);

    if(Cfg.VIPColorF && *Cfg.VIPColorF) cf_set_variable(begin,cs,"vipcolfg",Cfg.VIPColorF,strlen(Cfg.VIPColorF),1);
    if(Cfg.VIPColorB && *Cfg.VIPColorB) cf_set_variable(begin,cs,"vipcolbg",Cfg.VIPColorB,strlen(Cfg.VIPColorB),1);
  }

  if(Cfg.WhiteListColorF || Cfg.WhiteListColorB) {
    cf_tpl_setvalue(begin,"wlcol",TPL_VARIABLE_STRING,"1",1);

    if(Cfg.WhiteListColorF && *Cfg.WhiteListColorF) cf_set_variable(begin,cs,"wlcolfg",Cfg.WhiteListColorF,strlen(Cfg.WhiteListColorF),0);
    if(Cfg.WhiteListColorB && *Cfg.WhiteListColorB) cf_set_variable(begin,cs,"wlcolbg",Cfg.WhiteListColorB,strlen(Cfg.WhiteListColorB),0);
  }

  if(Cfg.OwnPostingsColorF || Cfg.OwnPostingsColorB) {
    cf_tpl_setvalue(begin,"colorown",TPL_VARIABLE_STRING,"1",1);

    if(Cfg.OwnPostingsColorF && *Cfg.OwnPostingsColorF) cf_set_variable(begin,cs,"colorownfg",Cfg.OwnPostingsColorF,strlen(Cfg.OwnPostingsColorF),1);
    if(Cfg.OwnPostingsColorB && *Cfg.OwnPostingsColorB) cf_set_variable(begin,cs,"colorownbg",Cfg.OwnPostingsColorB,strlen(Cfg.OwnPostingsColorB),1);
  }

  if(Cfg.CategoryHighlightColorF || Cfg.CategoryHighlightColorB) {
    cf_tpl_setvalue(begin,"cathighcolor",TPL_VARIABLE_STRING,"1",1);

    if(Cfg.CategoryHighlightColorF && *Cfg.CategoryHighlightColorF) cf_set_variable(begin,cs,"cathighcolorfg",Cfg.CategoryHighlightColorF,strlen(Cfg.CategoryHighlightColorF),1);
    if(Cfg.CategoryHighlightColorB && *Cfg.CategoryHighlightColorB) cf_set_variable(begin,cs,"cathighcolorbg",Cfg.CategoryHighlightColorB,strlen(Cfg.CategoryHighlightColorB),1);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_lh_set_cols_p */
int flt_lh_set_cols_p(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_lh_set_colors(head,dc,vc,tpl,NULL);
}
/* }}} */

/* {{{ flt_lh_handle_command */
int flt_lh_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  int len;
  u_char *list;

  if(cf_strcmp(opt->name,"HighlightOwnPostings") == 0) Cfg.HighlightOwnPostings = !cf_strcmp(args[0],"yes");
  else if(cf_strcmp(opt->name,"OwnPostingsColors") == 0) {
    if(Cfg.OwnPostingsColorF) free(Cfg.OwnPostingsColorF);
    if(Cfg.OwnPostingsColorB) free(Cfg.OwnPostingsColorB);
    Cfg.OwnPostingsColorF = strdup(args[0]);
    Cfg.OwnPostingsColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"WhiteList") == 0) {
    if(!Cfg.WhiteList) Cfg.WhiteList = cf_hash_new(NULL);
    list = flt_lh_tolwer(args[0],&len);
    flt_lh_parse_list(list,Cfg.WhiteList);
    free(list);
  }
  else if(cf_strcmp(opt->name,"WhiteListColors") == 0) {
    if(Cfg.WhiteListColorF) free(Cfg.WhiteListColorF);
    if(Cfg.WhiteListColorB) free(Cfg.WhiteListColorB);
    Cfg.WhiteListColorF = strdup(args[0]);
    Cfg.WhiteListColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"HighlightCategories") == 0) {
    if(!Cfg.HighlightCategories) Cfg.HighlightCategories = cf_hash_new(NULL);
    flt_lh_parse_list((u_char *)args[0],Cfg.HighlightCategories);
  }
  else if(cf_strcmp(opt->name,"CategoryHighlightColors") == 0) {
    if(Cfg.CategoryHighlightColorF) free(Cfg.CategoryHighlightColorF);
    if(Cfg.CategoryHighlightColorB) free(Cfg.CategoryHighlightColorB);
    Cfg.CategoryHighlightColorF = strdup(args[0]);
    Cfg.CategoryHighlightColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"VIPList") == 0) {
    if(!Cfg.VIPList) Cfg.VIPList = cf_hash_new(NULL);
    list = flt_lh_tolwer(args[0],&len);
    flt_lh_parse_list(list,Cfg.VIPList);
    free(list);
  }
  else if(cf_strcmp(opt->name,"VIPColors") == 0) {
    if(Cfg.VIPColorF) free(Cfg.VIPColorF);
    if(Cfg.VIPColorB) free(Cfg.VIPColorB);
    Cfg.VIPColorF = strdup(args[0]);
    Cfg.VIPColorB = strdup(args[1]);
  }

  return 0;
}
/* }}} */

t_conf_opt flt_listhighlight_config[] = {
  { "HighlightOwnPostings",    flt_lh_handle_command, CFG_OPT_USER,                NULL },
  { "OwnPostingsColors",       flt_lh_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "WhiteList",               flt_lh_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "WhiteListColors",         flt_lh_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "HighlightCategories",     flt_lh_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "CategoryHighlightColors", flt_lh_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "VIPList",                 flt_lh_handle_command, CFG_OPT_CONFIG,              NULL },
  { "VIPColors",               flt_lh_handle_command, CFG_OPT_CONFIG,              NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_listhighlight_handlers[] = {
  { VIEW_LIST_HANDLER, flt_lh_execute_filter    },
  { VIEW_INIT_HANDLER, flt_lh_set_colors },
  { POSTING_HANDLER,   flt_lh_set_cols_p },
  { 0, NULL }
};

t_module_config flt_listhighlight = {
  flt_listhighlight_config,
  flt_listhighlight_handlers,
  NULL,
  NULL,
  NULL,
  flt_listhighlight_cleanup
};

/* eof */
