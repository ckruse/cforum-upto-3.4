/**
 * \file flt_frameset.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements the forum view in a frameset
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

int ShallFrameset = 0;
u_char *TplFrameset;
u_char *TplBlank;

void gen_tpl_name(u_char buff[256],u_char *tpl) {
  t_name_value *vn = cfg_get_first_value(&fo_default_conf,"TemplateMode");

  if(vn) {
    snprintf(buff,256,tpl,vn->values[0]);
  }
  else {
    snprintf(buff,256,tpl,"");
  }
}

/*
 * Returns: int   one of the FLT_* values
 * Parameters:
 *   - t_cf_hash *head          a pointer to the cgi paramter list
 *   - t_configuration *dc      a pointer to the default configuration
 *   - t_configuration *vc      a pointer to the fo_view configuration
 *   - t_configuration *uc      a pointer to the user config
 *
 * This function is the api function for the frameset plugin. It is a
 * t_filter_begin function
 *
 */
int execute_filter(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  u_char buff[256];
  u_char *UserName = cf_hash_get(GlobalValues,"UserName",8);
  t_name_value *cs = cfg_get_first_value(dc,"ExternCharset");

  if(!ShallFrameset) {
    return FLT_DECLINE;
  }

  if(!head) {
    gen_tpl_name(buff,TplFrameset);

    if(buff) {
      t_name_value *x = cfg_get_first_value(dc,UserName?"UBaseURL":"BaseURL");
      t_cf_template tpl;

      printf("Content-Type: text/html; charset=%s\n\n",cs->values[0]);

      tpl_cf_init(&tpl,buff);
      tpl_cf_setvar(&tpl,"script",x->values[0],strlen(x->values[0]),0);
      tpl_cf_setvar(&tpl,"charset",cs->values[0],strlen(cs->values[0]),0);
      tpl_cf_parse(&tpl);

      tpl_cf_finish(&tpl);
      return FLT_EXIT;
    }
    else {
      printf("Content-Type: text/html; charset=%s\n\n",cs->values[0]);
      str_error_message("E_TPL_NOT_FOUND",NULL,15);
      return FLT_EXIT;
    }
  }
  else {
    u_char *action = NULL;
    if(head) {
      action = cf_cgi_get(head,"a");
    }

    if(action) {
      if(cf_strcmp(action,"b") == 0) {
        gen_tpl_name(buff,TplBlank);

        printf("Content-Type: text/html; charset=%s\n\n",cs->values[0]);
        if(buff) {
          t_cf_template tpl;
          tpl_cf_init(&tpl,buff);
          tpl_cf_setvar(&tpl,"charset",cs->values[0],strlen(cs->values[0]),0);

          if(tpl.tpl) {
            tpl_cf_parse(&tpl);
            tpl_cf_finish(&tpl);
          }
          else {
            printf("Sorry! Could not find template file!\n");
          }
        }
        else {
          printf("Sorry! Could not find template file!\n");
        }

        return FLT_EXIT;
      }
      else {
        if(cf_strcmp(action,"f") == 0) {
          cf_hash_entry_delete(head,"a",1);
          return FLT_OK;
        }
      }
    }
  }

  return FLT_DECLINE;
}

int set_cf_variables(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *top,t_cf_template *end) {
  if(ShallFrameset) {
    tpl_cf_setvar(top,"target","view",4,0);
    tpl_cf_setvar(top,"frame","1",1,0);

    tpl_cf_setvar(end,"target","view",4,0);

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int set_posting_vars(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thr,t_cf_template *tpl) {
  if(ShallFrameset) {
    tpl_cf_setvar(tpl,"frame","1",1,0);
    return FLT_OK;
  }

  return FLT_DECLINE;
}

int set_list_vars(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_message *msg,u_int64_t tid,int mode) {
  if(ShallFrameset) {
    tpl_cf_setvar(&msg->tpl,"frameset","1",1,0);
    if(mode == 0) tpl_cf_setvar(&msg->tpl,"target","view",4,0);
    return FLT_OK;
  }

  return FLT_DECLINE;
}

int get_conf(t_configfile *f,t_conf_opt *opt,u_char **args,int argnum) {
  ShallFrameset = cf_strcmp(args[0],"yes") == 0 ? 1 : 0;
  return 0;
}

int get_tpls(t_configfile *f,t_conf_opt *opt,u_char **args,int argnum) {
  TplFrameset = strdup(args[0]);
  TplBlank    = strdup(args[1]);

  return 0;
}

void finish(void) {
  if(TplFrameset) free(TplFrameset);
  if(TplBlank) free(TplBlank);
}

t_conf_opt config[] = {
  { "ShowForumAsFrameset", get_conf, NULL },
  { "TemplatesFrameset",   get_tpls, NULL },
  { NULL, NULL, NULL }
};

t_handler_config handlers[] = {
  { INIT_HANDLER,      execute_filter   },
  { VIEW_INIT_HANDLER, set_cf_variables    },
  { POSTING_HANDLER,   set_posting_vars },
  { VIEW_LIST_HANDLER, set_list_vars    },
  { 0, NULL }
};

t_module_config flt_frameset = {
  config,
  handlers,
  NULL,
  NULL,
  NULL,
  finish
};

/* eof */
