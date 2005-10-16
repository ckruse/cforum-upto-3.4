/**
 * \file flt_tipoftheday.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plug-in provides »tip of the day« functionality
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
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

static u_char *TOTD_File = NULL;
static u_char *TOTD_Idx  = NULL;
static int TOTD_Activate = 0;

static u_char *TOTD_fn = NULL;

/* {{{ flt_tipoftheday_execute */
int flt_tipoftheday_execute(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc,cf_template_t *top,cf_template_t *down) {
  FILE *fd;
  u_char *line = NULL;
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  ssize_t linelen;
  size_t bufflen = 0;
  u_int32_t num,offset;
  int chosen;
  cf_name_value_t *cs;

  srand(getpid() ^ time(NULL));

  if(TOTD_Activate && TOTD_File && TOTD_Idx) {
    if((fd = fopen(TOTD_Idx,"r")) != NULL) {
      fread(&num,sizeof(num),1,fd);

      chosen = (int)(((float)num) * rand() / (RAND_MAX + 1.0));

      fseek(fd,(long)(sizeof(offset) + (chosen * sizeof(offset))),SEEK_SET);
      fread(&offset,sizeof(offset),1,fd);
      fclose(fd);

      if((fd = fopen(TOTD_File,"r")) != NULL) {
        if(fseek(fd,(long)offset,SEEK_SET) != -1) {
          if((linelen = getline((char **)&line,&bufflen,fd)) != -1) {
            cs = cf_cfg_get_first_value(dc,forum_name,"ExternCharset");
            cf_set_variable(top,cs,"tipoftheday",line,linelen-1,0);

            free(line);
          }
        }

        fclose(fd);
      }
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_tipoftheday_confighandler */
int flt_tipoftheday_confighandler(cf_configfile_t *cf,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(!TOTD_fn) TOTD_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(TOTD_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"TipOfTheDayFile") == 0) {
    TOTD_File = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"TipOfTheDayIndex") == 0) {
    TOTD_Idx = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"TipOfTheDayActivate") == 0) {
    TOTD_Activate = cf_strcmp(args[0],"yes") == 0;
  }

  return 0;
}
/* }}} */

/* {{{ flt_tipoftheday_cleanup */
void flt_tipoftheday_cleanup(void) {
  if(TOTD_File) free(TOTD_File);
  if(TOTD_Idx) free(TOTD_Idx);
}
/* }}} */

cf_conf_opt_t flt_tipoftheday_config[] = {
  { "TipOfTheDayFile",     flt_tipoftheday_confighandler, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "TipOfTheDayActivate", flt_tipoftheday_confighandler, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { "TipOfTheDayIndex",    flt_tipoftheday_confighandler, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_tipoftheday_handlers[] = {
  { VIEW_INIT_HANDLER,    flt_tipoftheday_execute },
  { 0, NULL }
};

cf_module_config_t flt_tipoftheday = {
  MODULE_MAGIC_COOKIE,
  flt_tipoftheday_config,
  flt_tipoftheday_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_tipoftheday_cleanup
};


/* eof */
