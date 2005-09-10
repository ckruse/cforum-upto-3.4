/**
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

#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
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
} flt_poas_conf = { 0, NULL, 0, 0, 5.0, 25, 0 };

static u_char *flt_poas_fn = NULL;

/* {{{ flt_poas_case_strstr */
u_char *flt_poas_case_strstr(const u_char *haystack,const u_char *needle) {
  size_t len1 = strlen(haystack);
  size_t len2 = strlen(needle);
  size_t i;

  if(len1 < len2) return NULL;
  if(len1 == len2) return cf_strcasecmp(haystack,needle) == 0 ? (u_char *)haystack : NULL;

  for(i=0;i<=len1-len2;i++) {
    if(cf_strncasecmp(haystack+i,needle,len2) == 0) return (u_char *)(haystack+i);
  }

  return NULL;
}
/* }}} */

/* {{{ flt_poas_last_chars_consist_of */
int flt_poas_last_chars_consist_of(u_char *str,u_char *signs,size_t backnum,int all) {
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
/* }}} */

/* {{{ flt_poas_check_for_signs */
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
          if(signs > 3) {
            if(flt_poas_last_chars_consist_of(ptr,".",signs,1)) {
              #ifdef DEBUG
              fprintf(stderr,"signs-check: got %d signs: '%s', returning 3.0\n",signs,strndup(ptr-signs,signs));
              #endif
              return 3.0;
            }
            signs = 0;
            continue;
          }

          switch(strict) {
            case 0:
            case 1:
              /* we accept ... and .. */
              if(cf_strncmp(ptr-signs,"...",signs) == 0) break;

              /* we accept ?!?, !?! ?! and !? (in both cases) */
              if(flt_poas_last_chars_consist_of(ptr,"?!",signs,1) == 0) break;

              /* we do not accept everything else in strict mode 1 */
              if(strict == 1) {
                #ifdef DEBUG
                fprintf(stderr,"signs-check: got %d signs: '%s', returning 2.0\n",signs,strndup(ptr-signs,signs));
                #endif
                return 2.0;
              }
              break;

            case 2:
              #ifdef DEBUG
              fprintf(stderr,"signs-check: got %d signs: '%s', returning 3.0\n",signs,strndup(ptr-signs,signs));
              #endif
              return 3.0;
              break;
          }
        }

        signs = 0;
    }
  }


  if(signs >= 2) {
    /* we *never* accept more than three signs */
    if(signs > 3) {
      if(flt_poas_last_chars_consist_of(ptr,".",signs,1)) return 3.0;
    }

    switch(strict) {
      case 0:
      case 1:
        /* we accept ... and .. */
        if(cf_strncmp(ptr-signs,"...",signs)) break;

        /* we accept ?!?, !?! ?! and !? (in both cases) */
        if(flt_poas_last_chars_consist_of(ptr,"?!",signs,1) == 0) break;

        /* we do not accept everything else in strict mode 1 */
        if(strict == 1) return 2.0;
        break;

      case 2:
        return 3.0;
        break;
    }
  }

  if(musthave && gotsigns == 0) return 3.0;

  return .0;
}
/* }}} */

/* {{{ flt_poas_check_for_cases */
int flt_poas_check_for_cases(u_char *str) {
  register u_char *ptr;
  int has_big = 0,has_small = 0,big_after = 0;

  for(ptr=str;*ptr;++ptr) {
    if(isupper(*ptr)) {
      has_big++;
      big_after++;
    }
    else if(islower(*ptr)) {
      has_small++;
      big_after = 0;
    }
    else big_after = 0;

    if(big_after > 4) {
      if(has_small == 0) return 3.0;
    }
  }

  if(!has_small) return 3.0;
  if(!has_big)   return 1.0;

  return .0;
}
/* }}} */

/* {{{ flt_poas_check_newlines */
int flt_poas_check_newlines(u_char *str) {
  register u_char *ptr;
  int nl;

  for(ptr=str,nl=0;*ptr;ptr++) {
    if(*ptr == '<') {
      if(cf_strncmp(ptr,"<br />",6) == 0) nl++;
    }
  }

  if(nl <= 2) return 3.0;

  return .0;
}
/* }}} */

/* {{{ flt_poas_check_sig */
float flt_poas_check_sig(u_char *str) {
  float score = 0;
  register u_char *ptr;
  u_char *sigstart;
  int newlines = 0;
  size_t normal,sig;

  if((sigstart = strstr(str,"_/_SIG_/_")) == NULL) return .0;

  for(ptr=sigstart+9;*ptr;++ptr) {
    if(*ptr == '[' && cf_strncmp(ptr,"[image:",7) == 0) score += 3.0;
    else if(*ptr == '<' && cf_strncmp(ptr,"<br />",6) == 0) ++newlines;
  }

  if(newlines > 4) score += 3.0;

  normal = sigstart - str;
  sig = ptr - sigstart - 9;

  if(normal < sig) score += 3.0;

  return score;
}
/* }}} */

/* {{{ flt_poas_standardchecks */
int flt_poas_standardchecks(message_t *p) {
  float score = flt_poas_conf.fds_allowed;

  #ifdef DEBUG
  fprintf(stderr,"score is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_signs(p->subject.content,0,0);
  #ifdef DEBUG
  fprintf(stderr,"score after signs-check in subject is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_signs(p->author.content,2,0);
  #ifdef DEBUG
  fprintf(stderr,"score after signs-check in author is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_signs(p->content.content,1,1);
  #ifdef DEBUG
  fprintf(stderr,"score after signs-check in content is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_cases(p->subject.content);
  #ifdef DEBUG
  fprintf(stderr,"score after cases-check in subject is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_cases(p->author.content);
  #ifdef DEBUG
  fprintf(stderr,"score after cases-check in author is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_for_cases(p->content.content);
  #ifdef DEBUG
  fprintf(stderr,"score after cases-check in content is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_newlines(p->content.content);
  #ifdef DEBUG
  fprintf(stderr,"score after newlines-check in content is: %3.3f\n",score);
  #endif

  score -= flt_poas_check_sig(p->content.content);
  #ifdef DEBUG
  fprintf(stderr,"score after sig-check is: %3.3f\n",score);
  #endif

  if(p->email.len == 0) score -= 1.0;
  #ifdef DEBUG
  fprintf(stderr,"score after mail-check is: %3.3f\n",score);
  #endif

  return score > .0 ? 0 : -1;
}
/* }}} */

/* {{{ flt_poas_badwords_check */
int flt_poas_badwords_check(message_t *p) {
  long i;
  int score;

  if(flt_poas_conf.bws_len == 0) return 0;

  for(i=0,score=flt_poas_conf.bws_allowed;i<flt_poas_conf.bws_len && score > 0;++i) {
    if(flt_poas_case_strstr(p->content.content,flt_poas_conf.bws[i]) != NULL) score -= 1;
    if(flt_poas_case_strstr(p->subject.content,flt_poas_conf.bws[i]) != NULL) score -= 3;
    if(p->email.len && flt_poas_case_strstr(p->email.content,flt_poas_conf.bws[i]) != NULL) score -= 1;
  }

  return score <= 0 ? -1 : 0;
}
/* }}} */

/* {{{ flt_poas_qp_check */
int flt_poas_qp_check(message_t *p) {
  long quote_chars = 0,normal_chars = 0,all = 0;
  register u_char *ptr;
  int qmode = 0;
  size_t len = 0,len1 = 0;
  u_int32_t cnum;
  float qpc;

  /* {{{ count quoted and non-quoted characters */
  for(ptr=p->content.content,len1=p->content.len;*ptr;ptr+=len,len1-=len) {
    if(*ptr == (char)127) {
      qmode = 1;
      len = 1;
      continue;
    }
    else if(cf_strncmp(ptr,"<br />",6) == 0) {
      qmode = 0;
      len = 6;
      continue;
    }

    if((len = utf8_to_unicode(ptr,len1,&cnum)) == EILSEQ) {
      len = 1;
      continue;
    }

    if(qmode) quote_chars += 1;
    else normal_chars += 1;
    ++all;
  }
  /* }}} */

  /* both cases are an formatting error */
  if(normal_chars == 0) return -1;
  if(quote_chars == 0) return 0;

  /*
   * if percentage of quoted characters is bigger than
   * configured value, we have a formatting error
   */
  qpc = ((float)quote_chars / (float)all) * 100.0;
  if(qpc >= flt_poas_conf.qp) return -1;

  return 0;
}
/* }}} */

/* {{{ flt_poas_execute */
#ifdef CF_SHARED_MEM
int flt_poas_execute(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_poas_execute(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  /* first: standard checks */
  if(cf_cgi_get(head,"assicheck") == NULL || flt_poas_conf.poas_must_validate) {
    if(flt_poas_standardchecks(p) != 0) {
      cf_cgi_set(head,"assicheck","1");
      strcpy(ErrorString,"E_posting_format");
      display_posting_form(head,p,NULL);
      return FLT_EXIT;
    }
  }

  /* check for bad words */
  if(flt_poas_badwords_check(p) != 0) {
    cf_cgi_set(head,"assicheck","1");
    strcpy(ErrorString,"E_posting_badwords");
    display_posting_form(head,p,NULL);
    return FLT_EXIT;
  }

  /* check for quotes */
  if(cf_cgi_get(head,"assicheck") == NULL || flt_poas_conf.qp_must_validate) {
    if(flt_poas_qp_check(p) != 0) {
      cf_cgi_set(head,"assicheck","1");
      strcpy(ErrorString,"E_posting_quoting");
      display_posting_form(head,p,NULL);
      return FLT_EXIT;
    }
  }


  return FLT_OK;
}
/* }}} */

/* {{{ flt_poas_handle */
int flt_poas_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_poas_fn == NULL) flt_poas_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_poas_fn,context) != 0) return 0;

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
      if(cf_strcmp(opt->name,"QuotingPercent") == 0) flt_poas_conf.qp = atoi(args[0]);
      else flt_poas_conf.qp_must_validate = cf_strcmp(args[0],"yes") == 0;
      break;

    default:
      return 1;
  }

  return 0;
}
/* }}} */

/* {{{ flt_poas_finish */
void flt_poas_finish(void) {
  long i;

  if(flt_poas_conf.bws) {
    for(i=0;i<flt_poas_conf.bws_len;i++) free(flt_poas_conf.bws[i]);
  }
  free(flt_poas_conf.bws);

}
/* }}} */

conf_opt_t flt_poas_config[] = {
  { "PostingAssistantMustValidate", flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "BadWords",                     flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "BadwordsAllowed",              flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "FormateDeficitesAllowed",      flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "QuotingPercent",               flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "QuoteMustValidate",            flt_poas_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_poas_handlers[] = {
  { NEW_POST_HANDLER, flt_poas_execute },
  { 0, NULL }
};

module_config_t flt_postingassistant = {
  MODULE_MAGIC_COOKIE,
  flt_poas_config,
  flt_poas_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_poas_finish
};

/* eof */
