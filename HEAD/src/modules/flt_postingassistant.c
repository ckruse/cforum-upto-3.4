/*
 * \file flt_postingassistant.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Posting assistant
 *
 * This file is a plugin for fo_post. It tries to make
 * the user do smart postings.
 *
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
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <time.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "fo_post.h"
/* }}} */

struct {
  int poas_must_validate;
  u_char **bws;
  long bws_len;
  int bws_allowed;
  float fds_allowed;
  int qp;
  int qp_must_validate;
} flt_poas_conf = { 0, NULL, 0, 3, 5.0, 25, 0 };

int last_chars_consist_of(u_char *str,u_char *signs,size_t backnum,int all) {
  register u_char *ptr,*sign;
  size_t matched;
  u_char *lsigns = strdup(signs);

  for(ptr=str;ptr != str-backnum;ptr--) {
    for(matched=0,sign=signs;*sign;sign++) {
      if(*ptr == *sign) {
        matched = 1;
        break;
      }
    }

    if(matched == 0) {
      free(lsigns);
      return -1;
    }

    lsigns[sign-signs] = '\0';
  }

  if(all) {
    for(matched=0;matched<strlen(signs);matched++) {
      if(lsigns[matched] != '\0') {
        free(lsigns);
        return -1;
      }
    }
  }

  free(lsigns);
  return 0;
}

float flt_poas_check_for_signs(u_char *str,int strict,int musthave) {
  register u_char *ptr;
  int signs;
  int gotsigns = 0;

  for(signs = 0,ptr=str;*ptr;ptr++) {
    switch(*ptr) {
      case '.':
      case '!':
      case '?':
      case ':':
      case ';':
      case ',':
        signs++;
        gotsigns = 1;
        break;
      default:
       if(signs >= 2) {
          /* we *never* accept more than three signs */
          if(signs > 3) return 3.0;

          switch(strict) {
            case 0:
            case 1:
              /* we accept ... and .. */
              if(cf_strncmp(ptr-signs,"...",signs)) break;

              /* we accept ?!?, !?! ?! and !? (in both cases) */
              if(last_chars_consist_of(ptr,"?!",signs,1) == 0) break;

              /* we do not accept everything else in strict mode 1 */
              if(strict == 1) return 2.0;
              break;

            case 2:
              return 2.0;
              break;
          }
        }

        signs = 0;
    }
  }

  if(musthave && gotsigns == 0) return 3.0;

  return .0;
}

int flt_poas_check_for_cases(u_char *str) {
  register u_char *ptr;
  int has_big = 0,has_small,big_after = 0;

  for(ptr=str;*ptr;ptr++) {
    if(isupper(*ptr)) {
      has_big++;
      big_after++;
    }
    else if(islower(*ptr)) {
      has_small++;
      big_after = 0;
    }
    else big_after = 0;

    if(big_after > 4) return has_small ? 2.0 : 3.0;
  }

  if(!has_small) return 3.0;
  if(!has_big)   return 1.0;

  return .0;
}

int flt_poas_check_newlines(u_char *str) {
  register u_char *ptr;
  int nl;

  for(ptr=str,nl=0;*ptr;ptr++) {
    if(*ptr == '<') {
      if(cf_strncmp(ptr,"<br />",6) == 0) {
        nl++;
      }
    }
  }

  if(nl <= 2) return 3.0;

  return .0;
}

int flt_poas_standardchecks(t_message *p) {
  float score = flt_poas_conf.fds_allowed;

  score -= flt_poas_check_for_signs(p->subject,0,0);
  score -= flt_poas_check_for_signs(p->author,2,0);
  score -= flt_poas_check_for_signs(p->content,1,1);

  score -= flt_poas_check_for_cases(p->subject);
  score -= flt_poas_check_for_cases(p->author);
  score -= flt_poas_check_for_cases(p->content);

  score -= flt_poas_check_newlines(p->content);

  return score >= .0 ? 0 : -1;
}

int flt_poas_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode) {
  /* first: standard checks */
  if(cf_cgi_get(head,"assicheck") == NULL || flt_poas_conf.poas_must_validate) {
    if(flt_poas_standardchecks(p) != 0) {
      cf_cgi_set(head,"assicheck","1");
      strcpy(ErrorString,"E_posting_format");
      display_posting_form(head);
      return FLT_EXIT;
    }

    return FLT_OK;
  }

  return FLT_DECLINE;
}

int flt_poas_handle(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  switch(*opt->name) {
    case 'P':
      flt_poas_conf.poas_must_validate = cf_strcmp(args[0],"yes") == 0;
      break;
    case 'F':
      flt_poas_conf.fds_allowed = atof(args[0]);
      break;
    case 'B':
      if(cf_strcmp(opt->name,"BadWords") == 0) flt_poas_conf.bws_len = split(args[0],",",&flt_poas_conf.bws);
      else flt_poas_conf.bws_allowed = atoi(args[0]);
      break;
    case 'Q':
      if(cf_strcmp(args[0],"QuotingPercent") == 0) flt_poas_conf.qp = atoi(args[0]);
      else flt_poas_conf.qp_must_validate = cf_strcmp(args[0],"yes") == 0;
      break;

    default:
      return 1;
  }

  return 0;
}

void flt_poas_finish(void) {
}

t_conf_opt flt_poas_config[] = {
  { "PostingAssistantMustValidate", flt_poas_handle, NULL },
  { "BadWords",                     flt_poas_handle, NULL },
  { "BadwordsAllowed",              flt_poas_handle, NULL },
  { "FormateDeficitesAllowed",      flt_poas_handle, NULL },
  { "QuotingPercent",               flt_poas_handle, NULL },
  { "QuoteMustValidate",            flt_poas_handle, NULL },
  { NULL, NULL, NULL }
};

t_handler_config flt_poas_handlers[] = {
  { NEW_POST_HANDLER, flt_poas_execute },
  { 0, NULL }
};

t_module_config flt_poas = {
  flt_poas_config,
  flt_poas_handlers,
  NULL,
  NULL,
  NULL,
  flt_poas_finish
};

/* eof */
