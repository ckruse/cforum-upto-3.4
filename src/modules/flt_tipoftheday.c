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

int flt_tipoftheday_execute(t_cf_hash *cgi,t_configuration *dc,t_configuration *vc,t_cf_template *top,t_cf_template *down) {
  FILE *fd;
  u_char *line = NULL;
  ssize_t linelen;
  size_t bufflen;
  u_int32_t num,offset;
  int chosen;
  t_name_value *cs;

  srand(getpid() ^ time(NULL));

  if(TOTD_Activate && TOTD_File) {
    if((fd = fopen(TOTD_Idx,"r")) != NULL) {
      fread(&num,sizeof(num),1,fd);

      chosen = (int)(((float)num) * rand() / (RAND_MAX + 1.0));

      fseek(fd,(long)(sizeof(offset) + (chosen * sizeof(offset))),SEEK_SET);
      fread(&offset,sizeof(offset),1,fd);
      fclose(fd);

      if((fd = fopen(TOTD_File,"r")) != NULL) {
        if(fseek(fd,(long)offsets[chosen],SEEK_SET) != -1) {
          if((linelen = getline((char **)&line,&bufflen,fd)) != -1) {
            cs = cfg_get_first_value(dc,"ExternCharset");
            cf_set_variable(top,cs,"tipoftheday",line,linelen-1,0);

            free(line);
          }
        }

        fclose(fd);
        free(offsets);
      }
      else free(offsets);
    }
  }

  return FLT_DECLINE;
}

int flt_tipoftheday_confighandler(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
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

void flt_tipoftheday_cleanup(void) {
  if(TOTD_File) free(TOTD_File);
  if(TOTD_Idx) free(TOTD_Idx);
}

t_conf_opt flt_tipoftheday_config[] = {
  { "TipOfTheDayFile",     flt_tipoftheday_confighandler, NULL },
  { "TipOfTheDayActivate", flt_tipoftheday_confighandler, NULL },
  { "TipOfTheDayIndex",    flt_tipoftheday_confighandler, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_tipoftheday_handlers[] = {
  { VIEW_INIT_HANDLER,    flt_tipoftheday_execute },
  { 0, NULL }
};

t_module_config flt_tipoftheday = {
  flt_tipoftheday_config,
  flt_tipoftheday_handlers,
  NULL,
  NULL,
  NULL,
  flt_tipoftheday_cleanup
};


/* eof */
