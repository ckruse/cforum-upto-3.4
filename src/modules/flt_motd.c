/**
 * \file flt_motd.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements a »message of the day« feature
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

#include <sys/stat.h>
#include <unistd.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static int MOTD_enable = 1;
static u_char *MOTD_File = NULL;

int flt_motd_execute(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cf_template *begin,t_cf_template *end) {
  FILE *fd;
  struct stat st;
  u_char *txt;
  t_name_value *cs = cfg_get_first_value(dc,NULL,"ExternCharset");

  if(MOTD_File && MOTD_enable) {
    if(stat(MOTD_File,&st) != -1) {
      if((fd = fopen(MOTD_File,"r")) != NULL) {
        txt = fo_alloc(NULL,1,st.st_size+1,FO_ALLOC_MALLOC);
        fread(txt,1,st.st_size,fd);
        cf_set_variable(begin,cs,"motd",txt,st.st_size,0);
        free(txt);
        fclose(fd);
      }
      else perror("fopen");
    }
    else perror("stat");
  }

  return FLT_DECLINE;
}


int flt_motd_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(cf_strcmp(opt->name,"MotdFile") == 0) {
    MOTD_File = strdup(args[0]);
  }
  else MOTD_enable = cf_strcmp(args[0],"yes") == 0;

  return 0;
}

t_conf_opt flt_motd_config[] = {
  { "MotdFile",   flt_motd_handle, CFG_OPT_CONFIG, NULL },
  { "EnableMotd", flt_motd_handle, CFG_OPT_USER|CFG_OPT_CONFIG, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_motd_handlers[] = {
  { VIEW_INIT_HANDLER, flt_motd_execute },
  { 0, NULL }
};

t_module_config flt_motd = {
  flt_motd_config,
  flt_motd_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
