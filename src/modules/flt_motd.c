/**
 * \file flt_motd.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static int MOTD_enable = 1;
static u_char *MOTD_File = NULL;

static u_char *flt_motd_fn = NULL;

/* {{{ flt_motd_execute */
int flt_motd_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  FILE *fd;
  struct stat st;
  u_char *txt;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_name_value_t *cs = cf_cfg_get_first_value(dc,forum_name,"DF:ExternCharset");

  if(MOTD_File && MOTD_enable) {
    if(stat(MOTD_File,&st) != -1) {
      if((fd = fopen(MOTD_File,"r")) != NULL) {
        txt = cf_alloc(NULL,1,st.st_size+1,CF_ALLOC_MALLOC);
        fread(txt,1,st.st_size,fd);
        cf_set_variable(begin,cs,"motd",txt,st.st_size,0);
        free(txt);
        fclose(fd);
      }
      else fprintf(stderr,"flt_motd: fopen: could not open file '%s': %s\n",MOTD_File,strerror(errno));
    }
    else fprintf(stderr,"flt_motd: stat: could not stat file '%s': %s\n",MOTD_File,strerror(errno));
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_motd_handle */
int flt_motd_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_motd_fn == NULL) flt_motd_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_motd_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"Motd:File") == 0) MOTD_File = strdup(args[0]);
  else MOTD_enable = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

void flt_motd_cleanup(void) {
  if(MOTD_File) free(MOTD_File);
}

cf_conf_opt_t flt_motd_config[] = {
  { "Motd:File",   flt_motd_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "Motd:Enable", flt_motd_handle, CF_CFG_OPT_USER|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_motd_handlers[] = {
  { VIEW_INIT_HANDLER, flt_motd_execute },
  { 0, NULL }
};

cf_module_config_t flt_motd = {
  MODULE_MAGIC_COOKIE,
  flt_motd_config,
  flt_motd_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_motd_cleanup
};


/* eof */
