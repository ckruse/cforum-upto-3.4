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
} flt_listhighlight_cfg = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

void flt_listhighlight_parse_list(u_char *vips,t_cf_hash *hash) {
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

u_char *flt_listhighlight_to_lower(const u_char *str,register int *len) {
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

int flt_listhighlight_execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  t_name_value *uname = NULL;
  int len = 0;
  u_char *tmp;
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);

  /*
   * Initialization
   */
  if(UserName) uname = cfg_get_first_value(vc,NULL,"Name");

  if(!flt_listhighlight_cfg.VIPList && !flt_listhighlight_cfg.WhiteList && !flt_listhighlight_cfg.HighlightCategories && !flt_listhighlight_cfg.HighlightOwnPostings) return FLT_DECLINE;

  if(flt_listhighlight_cfg.VIPList) {
    tmp = flt_listhighlight_to_lower(msg->author,&len);
    if(cf_hash_get(flt_listhighlight_cfg.VIPList,tmp,len)) {
      tpl_cf_setvar(&msg->tpl,"vip","1",1,0);
    }
    free(tmp);
  }

  if(flt_listhighlight_cfg.WhiteList) {
    tmp = flt_listhighlight_to_lower(msg->author,&len);
    if(cf_hash_get(flt_listhighlight_cfg.WhiteList,tmp,len)) {
      tpl_cf_setvar(&msg->tpl,"whitelist","1",1,0);
    }
    free(tmp);
  }

  if(flt_listhighlight_cfg.HighlightCategories && msg->category) {
    if(cf_hash_get(flt_listhighlight_cfg.HighlightCategories,msg->category,strlen(msg->category))) {
      tpl_cf_setvar(&msg->tpl,"cathigh","1",1,0);
    }
  }

  if(flt_listhighlight_cfg.HighlightOwnPostings && uname) {
    if(cf_strcasecmp(msg->author,uname->values[0]) == 0) {
      tpl_cf_setvar(&msg->tpl,"ownposting","1",1,0);
    }
  }

  return FLT_OK;
}

void flt_listhighlight_cleanup(void) {
  if(flt_listhighlight_cfg.OwnPostingsColorF)       free(flt_listhighlight_cfg.OwnPostingsColorF);
  if(flt_listhighlight_cfg.OwnPostingsColorB)       free(flt_listhighlight_cfg.OwnPostingsColorB);
  if(flt_listhighlight_cfg.WhiteList)               cf_hash_destroy(flt_listhighlight_cfg.WhiteList);
  if(flt_listhighlight_cfg.WhiteListColorF)         free(flt_listhighlight_cfg.WhiteListColorF);
  if(flt_listhighlight_cfg.WhiteListColorB)         free(flt_listhighlight_cfg.WhiteListColorB);
  if(flt_listhighlight_cfg.HighlightCategories)     cf_hash_destroy(flt_listhighlight_cfg.HighlightCategories);
  if(flt_listhighlight_cfg.CategoryHighlightColorF) free(flt_listhighlight_cfg.CategoryHighlightColorF);
  if(flt_listhighlight_cfg.CategoryHighlightColorB) free(flt_listhighlight_cfg.CategoryHighlightColorB);
  if(flt_listhighlight_cfg.VIPList)                 cf_hash_destroy(flt_listhighlight_cfg.VIPList);
  if(flt_listhighlight_cfg.VIPColorF)               free(flt_listhighlight_cfg.VIPColorF);
  if(flt_listhighlight_cfg.VIPColorB)               free(flt_listhighlight_cfg.VIPColorB);
}

int flt_listhighlight_set_colors(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  t_name_value *cs = cfg_get_first_value(dc,NULL,"ExternCharset");

  if(flt_listhighlight_cfg.VIPColorF || flt_listhighlight_cfg.VIPColorB) {
    tpl_cf_setvar(begin,"vipcol","1",1,0);

    if(flt_listhighlight_cfg.VIPColorF && *flt_listhighlight_cfg.VIPColorF) {
      cf_set_variable(begin,cs,"vipcolfg",flt_listhighlight_cfg.VIPColorF,strlen(flt_listhighlight_cfg.VIPColorF),1);
    }
    if(flt_listhighlight_cfg.VIPColorB && *flt_listhighlight_cfg.VIPColorB) {
      cf_set_variable(begin,cs,"vipcolbg",flt_listhighlight_cfg.VIPColorB,strlen(flt_listhighlight_cfg.VIPColorB),1);
    }
  }

  if(flt_listhighlight_cfg.WhiteListColorF || flt_listhighlight_cfg.WhiteListColorB) {
    tpl_cf_setvar(begin,"wlcol","1",1,0);

    if(flt_listhighlight_cfg.WhiteListColorF && *flt_listhighlight_cfg.WhiteListColorF) {
      cf_set_variable(begin,cs,"wlcolfg",flt_listhighlight_cfg.WhiteListColorF,strlen(flt_listhighlight_cfg.WhiteListColorF),0);
    }
    if(flt_listhighlight_cfg.WhiteListColorB && *flt_listhighlight_cfg.WhiteListColorB) {
      cf_set_variable(begin,cs,"wlcolbg",flt_listhighlight_cfg.WhiteListColorB,strlen(flt_listhighlight_cfg.WhiteListColorB),0);
    }
  }

  if(flt_listhighlight_cfg.OwnPostingsColorF || flt_listhighlight_cfg.OwnPostingsColorB) {
    tpl_cf_setvar(begin,"colorown","1",1,0);

    if(flt_listhighlight_cfg.OwnPostingsColorF && *flt_listhighlight_cfg.OwnPostingsColorF) {
      cf_set_variable(begin,cs,"colorownfg",flt_listhighlight_cfg.OwnPostingsColorF,strlen(flt_listhighlight_cfg.OwnPostingsColorF),1);
    }
    if(flt_listhighlight_cfg.OwnPostingsColorB && *flt_listhighlight_cfg.OwnPostingsColorB) {
      cf_set_variable(begin,cs,"colorownbg",flt_listhighlight_cfg.OwnPostingsColorB,strlen(flt_listhighlight_cfg.OwnPostingsColorB),1);
    }
  }

  if(flt_listhighlight_cfg.CategoryHighlightColorF || flt_listhighlight_cfg.CategoryHighlightColorB) {
    tpl_cf_setvar(begin,"cathighcolor","1",1,0);

    if(flt_listhighlight_cfg.CategoryHighlightColorF && *flt_listhighlight_cfg.CategoryHighlightColorF) {
      cf_set_variable(begin,cs,"cathighcolorfg",flt_listhighlight_cfg.CategoryHighlightColorF,strlen(flt_listhighlight_cfg.CategoryHighlightColorF),1);
    }
    if(flt_listhighlight_cfg.CategoryHighlightColorB && *flt_listhighlight_cfg.CategoryHighlightColorB) {
      cf_set_variable(begin,cs,"cathighcolorbg",flt_listhighlight_cfg.CategoryHighlightColorB,strlen(flt_listhighlight_cfg.CategoryHighlightColorB),1);
    }
  }

  return FLT_OK;
}

int flt_listhighlight_set_cols_p(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  return flt_listhighlight_set_colors(head,dc,vc,tpl,NULL);
}

int flt_listhighlight_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  int len;
  u_char *list;

  if(cf_strcmp(opt->name,"HighlightOwnPostings") == 0) {
    flt_listhighlight_cfg.HighlightOwnPostings = !cf_strcmp(args[0],"yes");
  }
  else if(cf_strcmp(opt->name,"OwnPostingsColors") == 0) {
    if(flt_listhighlight_cfg.OwnPostingsColorF) free(flt_listhighlight_cfg.OwnPostingsColorF);
    if(flt_listhighlight_cfg.OwnPostingsColorB) free(flt_listhighlight_cfg.OwnPostingsColorB);
    flt_listhighlight_cfg.OwnPostingsColorF = strdup(args[0]);
    flt_listhighlight_cfg.OwnPostingsColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"WhiteList") == 0) {
    if(!flt_listhighlight_cfg.WhiteList) flt_listhighlight_cfg.WhiteList = cf_hash_new(NULL);
    list = flt_listhighlight_to_lower(args[0],&len);
    flt_listhighlight_parse_list(list,flt_listhighlight_cfg.WhiteList);
    free(list);
  }
  else if(cf_strcmp(opt->name,"WhiteListColors") == 0) {
    if(flt_listhighlight_cfg.WhiteListColorF) free(flt_listhighlight_cfg.WhiteListColorF);
    if(flt_listhighlight_cfg.WhiteListColorB) free(flt_listhighlight_cfg.WhiteListColorB);
    flt_listhighlight_cfg.WhiteListColorF = strdup(args[0]);
    flt_listhighlight_cfg.WhiteListColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"HighlightCategories") == 0) {
    if(!flt_listhighlight_cfg.HighlightCategories) flt_listhighlight_cfg.HighlightCategories = cf_hash_new(NULL);
    flt_listhighlight_parse_list((u_char *)args[0],flt_listhighlight_cfg.HighlightCategories);
  }
  else if(cf_strcmp(opt->name,"CategoryHighlightColors") == 0) {
    if(flt_listhighlight_cfg.CategoryHighlightColorF) free(flt_listhighlight_cfg.CategoryHighlightColorF);
    if(flt_listhighlight_cfg.CategoryHighlightColorB) free(flt_listhighlight_cfg.CategoryHighlightColorB);
    flt_listhighlight_cfg.CategoryHighlightColorF = strdup(args[0]);
    flt_listhighlight_cfg.CategoryHighlightColorB = strdup(args[1]);
  }
  else if(cf_strcmp(opt->name,"VIPList") == 0) {
    if(!flt_listhighlight_cfg.VIPList) flt_listhighlight_cfg.VIPList = cf_hash_new(NULL);
    list = flt_listhighlight_to_lower(args[0],&len);
    flt_listhighlight_parse_list(list,flt_listhighlight_cfg.VIPList);
    free(list);
  }
  else if(cf_strcmp(opt->name,"VIPColors") == 0) {
    if(flt_listhighlight_cfg.VIPColorF) free(flt_listhighlight_cfg.VIPColorF);
    if(flt_listhighlight_cfg.VIPColorB) free(flt_listhighlight_cfg.VIPColorB);
    flt_listhighlight_cfg.VIPColorF = strdup(args[0]);
    flt_listhighlight_cfg.VIPColorB = strdup(args[1]);
  }

  return 0;
}

t_conf_opt flt_listhighlight_config[] = {
  { "HighlightOwnPostings",    flt_listhighlight_handle_command, CFG_OPT_USER,                NULL },
  { "OwnPostingsColors",       flt_listhighlight_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "WhiteList",               flt_listhighlight_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "WhiteListColors",         flt_listhighlight_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "HighlightCategories",     flt_listhighlight_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "CategoryHighlightColors", flt_listhighlight_handle_command, CFG_OPT_CONFIG|CFG_OPT_USER, NULL },
  { "VIPList",                 flt_listhighlight_handle_command, CFG_OPT_CONFIG,              NULL },
  { "VIPColors",               flt_listhighlight_handle_command, CFG_OPT_CONFIG,              NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_listhighlight_handlers[] = {
  { VIEW_LIST_HANDLER, flt_listhighlight_execute_filter },
  { VIEW_INIT_HANDLER, flt_listhighlight_set_colors },
  { POSTING_HANDLER,   flt_listhighlight_set_cols_p },
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
