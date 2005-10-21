/**
 * \file flt_pavatar.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements the pavatar spec (<http://jeenaparadies.net/specs/pavatar>)
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
#include <unistd.h>

#include <time.h>
#include <sys/types.h>

#include <sys/file.h>
#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
/* }}} */

static u_char *flt_pavatar_fn = NULL;

static u_char *flt_pavatar_cachedir       = NULL;
static u_char *flt_pavatar_cacheuri       = NULL;
static time_t flt_pavatar_cachetime       = 7 * 24 * 60 * 60; /* default cache time: one week */
static time_t flt_pavatar_cachetime_least = 7 * 24 * 60 * 60; /* default least cache time: one week */
static int flt_pavatar_expiresoverrides   = 1;
static int flt_pavatar_active             = 0;
static int flt_pavatar_favicon_way        = 1;
static int flt_pavatar_cache_them         = 1;

/* {{{ flt_pavatar_check_robots_rule */
int flt_pavatar_check_robots_rule(const u_char *robotsfile,const u_char *uri) {
  register u_char *ptr;
  u_char *start,*rule_content;

  for(ptr=(u_char *)robotsfile;*ptr;++ptr) {
    if(cf_strncmp(ptr,"User-Agent:",11) == 0) {
      for(ptr+=11;*ptr && isspace(*ptr);++ptr);

      /* eof and now disallow rule seen */
      if(*ptr == 0) return 0;

      if(cf_strncmp(ptr,"pavatar",7) == 0) {
        for(ptr+=7;*ptr && (*ptr == '\012' || *ptr == '\015');++ptr);

        if(*ptr == 0) return 0;

        if(cf_strncmp(ptr,"Disallow:",9) == 0) {
          for(ptr+=9;*ptr && isspace(*ptr);++ptr);
          if(*ptr == 0) return 0; /* Disallow: means, everything is allowed */

          for(start=ptr;*ptr && !isspace(*ptr) && *ptr != '\012' && *ptr != '\015';++ptr);

          rule_content = strndup(start,ptr-start);
          if(cf_strcmp(ptr,"/") == 0) { /* *evrything* is disallowed. Return. */
            free(rule_content);
            return 1;
          }

          if(cf_strncmp(rule_content,uri,strlen(rule_content)) == 0) {
            /* this URL is disallowed. Return. */
            free(rule_content);
            return 1;
          }

          free(rule_content);
        }
      }

    }
  }

  return 0;
}
/* }}} */

/* {{{ flt_pavatar_autodiscover */
u_char *flt_pavatar_autodiscover(const u_char *uri) {
  u_char *ret,*pavatar_uri = NULL,*last;
  cf_http_response_t *rsp,*rsp1;
  register u_char *ptr;

  cf_string_t uri_buff;

  if((rsp = cf_http_simple_get_uri(uri)) == NULL) return NULL;

  if((pavatar_uri = cf_hash_get(rsp->headers,"X-Pavatar",10)) == NULL) {
    if(rsp->content.content && (ret = strcasestr(rsp->content.content,"<link rel=\"pavatar\" href=\"")) != NULL) {
      ret += 27;
      for(ptr=ret;*ptr && *ptr != '"';++ptr);

      if(*ptr && *ptr == '"') pavatar_uri = strndup(ret,ptr-ret);
    }
    else {
      if(flt_pavatar_favicon_way) {
        cf_http_destroy_response(rsp);
        free(rsp);

        for(last=NULL,ptr=(u_char *)uri;*ptr;++ptr) {
          if(*ptr == '/') last = ptr;
        }

        cf_str_init_growth(&uri_buff,128);
        cf_str_char_set(&uri_buff,uri,last-uri);
        cf_str_chars_append(&uri_buff,"robots.txt",11);

        if((rsp = cf_http_simple_get_uri(uri_buff.content)) == NULL) {
          cf_str_cleanup(&uri_buff);
          return NULL;
        }

        for(ptr=(u_char *)uri+strlen(uri)-1;ptr>=uri+7;--ptr) {
          if(*ptr == '/') {
            cf_str_char_set(&uri_buff,uri,ptr-uri);
            cf_str_chars_append(&uri_buff,"pavatar",8);

            if(rsp->status == 200) { /* if robots.txt exists: check if we may check this URL */
              if(flt_pavatar_check_robots_rule(rsp->content.content,uri_buff.content) != 0) continue;
            }

            if((rsp1 = cf_http_simple_head_uri(uri_buff.content)) == NULL) {
              cf_http_destroy_response(rsp);
              free(rsp);
            }

            if(rsp1->status == 200) {
              cf_http_destroy_response(rsp1);
              free(rsp1);

              pavatar_uri = uri_buff.content;
              break;
            }

            cf_http_destroy_response(rsp1);
            free(rsp1);
          }
        }
      }
    }
  }
  else pavatar_uri = strdup(pavatar_uri);

  cf_http_destroy_response(rsp);
  free(rsp);

  return pavatar_uri;
}
/* }}} */

/* {{{ flt_pavatar_get_cache_filename */
void flt_pavatar_get_cache_filename(const u_char *uri,u_char **cachefile,u_char **cacheuri) {
  cf_string_t str,str1;
  register u_char *ptr;

  if(!cachefile && !cacheuri) return;

  cf_str_init_growth(&str,256);
  cf_str_init_growth(&str1,256);
  cf_str_cstr_set(&str,flt_pavatar_cachedir);
  cf_str_cstr_set(&str1,flt_pavatar_cacheuri);
  cf_str_char_append(&str,'/');
  cf_str_char_append(&str1,'/');

  for(ptr=(u_char *)uri;*ptr;++ptr) {
    switch(*ptr) {
      case ':':
      case '/':
        cf_str_char_append(&str,'-');
        cf_str_char_append(&str1,'-');
        break;
      default:
        cf_str_char_append(&str,*ptr);
        cf_str_char_append(&str1,*ptr);
    }
  }

  if(cachefile) *cachefile = str.content;
  else cf_str_cleanup(&str);

  if(cacheuri) *cacheuri = str1.content;
  else cf_str_cleanup(&str1);
}
/* }}} */

/* {{{ flt_pavatar_is_cachend */
u_char *flt_pavatar_is_cached(const u_char *uri) {
  struct stat st;
  u_char *file,*curi;

  flt_pavatar_get_cache_filename(uri,&file,&curi);

  if(stat(file,&st) == -1) {
    free(file);
    free(curi);
    return NULL;
  }

  free(file);
  return curi;
}
/* }}} */

/* {{{ flt_pavatar_get_month */
int flt_pavatar_get_month(const u_char *m) {
  if(cf_strncmp(m,"Jan",3) == 0) return 0;
  else if(cf_strncmp(m,"Feb",3) == 0) return 1;
  else if(cf_strncmp(m,"Mar",3) == 0) return 2;
  else if(cf_strncmp(m,"Apr",3) == 0) return 3;
  else if(cf_strncmp(m,"May",3) == 0) return 4;
  else if(cf_strncmp(m,"Jun",3) == 0) return 5;
  else if(cf_strncmp(m,"Jul",3) == 0) return 6;
  else if(cf_strncmp(m,"Aug",3) == 0) return 7;
  else if(cf_strncmp(m,"Sep",3) == 0) return 8;
  else if(cf_strncmp(m,"Oct",3) == 0) return 9;
  else if(cf_strncmp(m,"Nov",3) == 0) return 10;
  else if(cf_strncmp(m,"Dec",3) == 0) return 11;

  return -1;
}
/* }}} */

/* {{{ flt_pavatar_parse_datestr */
void flt_pavatar_parse_datestr(const u_char *str,struct tm *tm) {
  tm->tm_mday  = atoi(str+5);
  tm->tm_mon   = flt_pavatar_get_month(str+8);
  tm->tm_year  = atoi(str+12) - 1900;
  tm->tm_wday  = 0;
  tm->tm_hour  = atoi(str+17);
  tm->tm_min   = atoi(str+20);
  tm->tm_sec   = atoi(str+23);

  tm->tm_yday  = 0;
  tm->tm_isdst = -1;
}
/* }}} */

/* {{{ flt_pavatar_cache_it */
u_char *flt_pavatar_cache_it(const u_char *hpuri,const u_char *pavatar_uri) {
  cf_string_t str;
  FILE *fd;
  u_char *curi,*file,*val,*ptr;
  cf_http_response_t *rsp = cf_http_simple_get_uri(pavatar_uri);
  time_t lm,expires,maxage,now;
  struct tm tm;
  if(!rsp || rsp->status != 200) return NULL;


  flt_pavatar_get_cache_filename(hpuri,&file,&curi);

  if((fd = fopen(file,"wb")) == NULL) {
    free(file);
    free(curi);
    cf_http_destroy_response(rsp);
    free(rsp);
    return NULL;
  }
  fwrite(rsp->content.content,1,rsp->content.len,fd);
  fclose(fd);

  /* {{{ save for caching necessary infos: time received, last-modified header, expires header, max-age field of cache-control header */
  now = time(NULL);
  if(flt_pavatar_expiresoverrides) {
    if((val = cf_hash_get(rsp->headers,"Expires",8)) != NULL) {
      flt_pavatar_parse_datestr(val,&tm);

      if((expires = timegm(&tm)) > 0) {
        if(expires < now + flt_pavatar_cachetime_least) expires = now + flt_pavatar_cachetime_least;
      }
      else expires = now + flt_pavatar_cachetime;
    }
    else expires = now + flt_pavatar_cachetime;
  }
  else expires = now + flt_pavatar_cachetime;

  if((val = cf_hash_get(rsp->headers,"Last-Modified",14)) != NULL) {
    flt_pavatar_parse_datestr(val,&tm);
    if((lm = timegm(&tm)) <= 0) lm = now;
  }
  else lm = now;

  if((val = cf_hash_get(rsp->headers,"Cache-Control",14)) != NULL) {
    if((ptr=strstr(val,"max-age=")) != NULL) {
      ptr += 7;
      maxage = atoi(ptr);
    }
  }
  else maxage = flt_pavatar_cachetime;

  cf_str_init_growth(&str,256);
  cf_str_cstr_set(&str,file);
  cf_str_chars_append(&str,".hdrs",5);

  if((fd = fopen(str.content,"wb")) == NULL) {
    unlink(file);
    free(file);
    free(curi);
    cf_http_destroy_response(rsp);
    free(rsp);
    cf_str_cleanup(&str);
    return NULL;
  }
  fwrite(&now,sizeof(now),1,fd);
  fwrite(&lm,sizeof(lm),1,fd);
  fwrite(&expires,sizeof(expires),1,fd);
  fwrite(&maxage,sizeof(maxage),1,fd);
  fclose(fd);
  /* }}} */

  cf_str_cleanup(&str);
  free(file);
  cf_http_destroy_response(rsp);
  free(rsp);

  return curi;
}
/* }}} */

/* {{{ flt_pavatar_handleit */
u_char *flt_pavatar_handleit(const u_char *uri) {
  u_char *ret,*pavatar_uri = NULL;

  if(flt_pavatar_cache_them && (ret = flt_pavatar_is_cached(uri)) != NULL) return ret;
  if((pavatar_uri = flt_pavatar_autodiscover(uri)) == NULL) return NULL;

  if(flt_pavatar_cache_them) { /* create a cache */
    ret = flt_pavatar_cache_it(uri,pavatar_uri);
    free(pavatar_uri);
    return ret;
  }

  return pavatar_uri;
}
/* }}} */

/* {{{ flt_pavatar_exec */
int flt_pavatar_exec(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,message_t *msg,cf_tpl_variable_t *hash) {
  u_char *uri;
  cf_name_value_t *cs;

  if(!flt_pavatar_active) return FLT_DECLINE;

  cs = cf_cfg_get_first_value(dc,flt_pavatar_fn,"ExternCharset");

  if(thread && thread->messages->hp.content) {
    if((uri = flt_pavatar_handleit(thread->messages->hp.content)) != NULL) {
      cf_set_variable_hash(hash,cs,"pavatar_uri",uri,strlen(uri),1);
      free(uri);
    }
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_pavatar_conf */
int flt_pavatar_conf(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *ptr;

  if(flt_pavatar_fn == NULL) flt_pavatar_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_pavatar_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"PavatarCacheDir") == 0) {
    if(flt_pavatar_cachedir) free(flt_pavatar_cachedir);
    flt_pavatar_cachedir = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"PavatarCacheTime") == 0) {
    flt_pavatar_cachetime = strtol(args[0],(char **)&ptr,10);

    if(ptr) {
      if(toupper(*ptr) == 'M') flt_pavatar_cachetime *= 60L;
      else if(toupper(*ptr) == 'H') flt_pavatar_cachetime *= 60L * 60L;
      else if(toupper(*ptr) == 'D') flt_pavatar_cachetime *= 24L * 60L * 60L;
      else if(toupper(*ptr) == 'W') flt_pavatar_cachetime *= 7L * 24L * 60L * 60L;
      else if(toupper(*ptr) == 'M') flt_pavatar_cachetime *= 30L * 24L * 60L * 60L;
    }
  }
  else if(cf_strcmp(opt->name,"PavatarExpiresOverrides") == 0) flt_pavatar_expiresoverrides = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"PavatarEnableFaviconWay") == 0) flt_pavatar_favicon_way = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"PavatarCacheThem") == 0) flt_pavatar_cache_them = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"PavatarActive") == 0) flt_pavatar_active = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"PavatarCacheURI") == 0) {
    if(flt_pavatar_cacheuri) free(flt_pavatar_cacheuri);
    flt_pavatar_cacheuri = strdup(args[0]);
  }

  return 0;
}
/* }}} */

void flt_pavatar_cleanup(void) {
  if(flt_pavatar_cachedir) free(flt_pavatar_cachedir);
  if(flt_pavatar_cacheuri) free(flt_pavatar_cacheuri);
}

cf_conf_opt_t flt_pavatar_config[] = {
  { "PavatarCacheThemAtLeast", flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarCacheThem",        flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarCacheDir",         flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarCacheURI",         flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarCacheTime",        flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarExpiresOverrides", flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarEnableFaviconWay", flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "PavatarActive",           flt_pavatar_conf, CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_pavatar_handlers[] = {
  { PERPOST_VAR_HANDLER, flt_pavatar_exec },
  { 0, NULL }
};

cf_module_config_t flt_pavatar = {
  MODULE_MAGIC_COOKIE,
  flt_pavatar_config,
  flt_pavatar_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_pavatar_cleanup
};

/* eof */