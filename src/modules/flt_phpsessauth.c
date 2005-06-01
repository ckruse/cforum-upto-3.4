/**
 * \file flt_phpsessauth.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements user authentification over the HTTP authentification
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

#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

static u_char *flt_phpsessauth_vname    = NULL;
static u_char *flt_phpsessauth_sid      = NULL;
static u_char *flt_phpsessauth_sessname = NULL;
static u_char *flt_phpsessauth_sesspath = NULL;

static u_char *flt_phpsessauth_fn    = NULL;

/* {{{ flt_psa_parser_readarray */
int flt_psa_parser_readarray(const u_char *start,long elems,u_char **end) {
  register u_char *ptr = (u_char *)start;
  u_char *tmp;

  long i = 0;
  size_t len;

  for(;*ptr && i < elems * 2;++i) {
    switch(*ptr) {
      case 's':
        ptr += 2;
        len  = strtol(ptr,(char **)&tmp,10);
        ptr  = tmp + len + 4;
        break;

      case 'i':
        ptr += 2;
        strtol(ptr,(char **)&tmp,10);
        ptr  = tmp + 1;
        break;

      case 'a':
        ptr += 2;
        len  = strtol(ptr,(char **)&tmp,10);
        ptr  = tmp + 2;

        flt_psa_parser_readarray(ptr,(long)len,&tmp);
        ptr = tmp;
        break;
    }
  }

  *end = ptr + 1;
  return 0;
}
/* }}} */

/* {{{ flt_psa_parser */
int flt_psa_parser(t_cf_hash *hash,u_char **pos) {
  register u_char *ptr = *pos;
  u_char *start,*name,*cval;
  double dval;
  long lval;

  size_t len;

  for(;*ptr;) {
    for(start=ptr;*ptr && *ptr != '|';++ptr);
    name = strndup(start,ptr-start);

    ++ptr;

    switch(*ptr) {
      case 's':
        ptr += 2;
        len  = strtol(ptr,(char **)&start,10);
        ptr  = start + 2;

        cval = memdup(ptr,len+1);
        cval[len] = '\0';
        cf_hash_set(hash,name,strlen(name),cval,len+1);

        ptr  = ptr + len + 2;
        free(cval);
        break;

      case 'd':
        ptr += 2;
        dval = strtod(ptr,(char **)&start);
        ptr  = start + 1;

        cf_hash_set(hash,name,strlen(name),&dval,sizeof(dval));
        break;

      case 'i':
        ptr += 2;
        lval = strtol(ptr,(char **)&start,10);
        ptr  = start + 1;

        cf_hash_set(hash,name,strlen(name),&lval,sizeof(lval));
        break;

      case 'a':
        /* parsing an array sucks, so wie don't do it. We read the array without saving it */
        ptr += 2;
        lval = strtol(ptr,(char **)&start,10);
        ptr  = start+2;

        flt_psa_parser_readarray(ptr,lval,&start);
        ptr = start;
    }

    free(name);
  }

  return 0;
}
/* }}} */

/* {{{ flt_phpsessauth_getvar */
u_char *flt_phpsessauth_getvar(const u_char *vname) {
  t_string path;
  int fd,rc;
  t_cf_hash *hash = cf_hash_new(NULL);
  u_char *start,*ptr,*name = NULL;
  struct stat st;

  str_init_growth(&path,128);
  if(flt_phpsessauth_sesspath) str_char_set(&path,flt_phpsessauth_sesspath,strlen(flt_phpsessauth_sesspath));
  else str_char_set(&path,"/tmp",4);

  str_chars_append(&path,"/sess_",6);
  str_chars_append(&path,flt_phpsessauth_sid,strlen(flt_phpsessauth_sid));

  /* {{{ open file and map it into our memory */
  if((fd = open(path.content,O_RDONLY)) == -1) {
    fprintf(stderr,"flt_phpsessauth: open: could not open file '%s': %s\n",path.content,strerror(errno));
    str_cleanup(&path);
    return NULL;
  }

  if(stat(path.content,&st) == -1) {
    fprintf(stderr,"flt_phpsessauth: stat: could not stat file '%s': %s\n",path.content,strerror(errno));
    close(fd);
    str_cleanup(&path);
    return NULL;
  }

  if(st.st_size == 0) {
    fprintf(stderror,"flt_phpsessauth: file '%s' is empty!\n",path.content);
    close(fd);
    str_cleanup(&path);
    return NULL;
  }

  if((caddr_t)(ptr = start = mmap(0,st.st_size+1,PROT_READ,MAP_FILE|MAP_SHARED,fd,0)) == (caddr_t)-1) {
    fprintf(stderr,"flt_phpsessauth: mmap: could not map file '%s': %s\n",path.content,strerror(errno));
    close(fd);
    str_cleanup(&path);
    return NULL;
  }
  /* }}} */

  rc = flt_psa_parser(hash,&ptr);

  munmap(start,st.st_size);
  close(fd);
  str_cleanup(&path);

  if(rc != 0) {
    cf_hash_destroy(hash);
    return NULL;
  }

  name = cf_hash_get(hash,(u_char *)vname,strlen(vname));
  if(name) name = strdup(name);
  cf_hash_destroy(hash);

  return name;
}
/* }}} */

/* {{{ flt_httpauth_run */
int flt_phpsessauth_run(t_cf_hash *head,t_configuration *dc,t_configuration *vc) {
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v = cfg_get_first_value(dc,fn,"AuthMode");
  u_char *name = NULL,*path;
  t_cf_hash *cookies;

  if(!flt_phpsessauth_vname) return FLT_DECLINE;
  if(!v || !v->values[0] || cf_strcmp(v->values[0],"phpsess") != 0) return FLT_DECLINE;

  if(head == NULL || (flt_phpsessauth_sid = cf_cgi_get(head,flt_phpsessauth_sessname)) == NULL) {
    cookies = cf_hash_new(cf_cgi_destroy_entry);
    cf_cgi_parse_cookies(cookies);

    if((flt_phpsessauth_sid = cf_cgi_get(cookies,flt_phpsessauth_sessname)) == NULL) {
      cf_hash_destroy(cookies);
      return FLT_DECLINE;
    }

    cf_hash_destroy(cookies);
  }
  else cf_add_static_uri_flag(flt_phpsessauth_sessname,flt_phpsessauth_sid,0);

  if((name = flt_phpsessauth_getvar(flt_phpsessauth_vname)) != NULL) {
    path = cf_get_uconf_name(name);

    if(path) {
      free(path);
      cf_hash_set(GlobalValues,"UserName",8,name,strlen(name)+1);
    }

    free(name);
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_phpsessauth_handle */
int flt_phpsessauth_handle(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_phpsessauth_fn == NULL) flt_phpsessauth_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_phpsessauth_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SessionVName") == 0) {
    if(flt_phpsessauth_vname) free(flt_phpsessauth_vname);
    flt_phpsessauth_vname = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"SessionPath") == 0) {
    if(flt_phpsessauth_sesspath) free(flt_phpsessauth_sesspath);
    flt_phpsessauth_sesspath = strdup(args[0]);
  }
  else {
    if(flt_phpsessauth_sessname) free(flt_phpsessauth_sessname);
    flt_phpsessauth_sessname = strdup(args[0]);
  }

  return 0;
}
/* }}} */

void flt_phpsessauth_cleanup(void) {
  if(flt_phpsessauth_vname) free(flt_phpsessauth_vname);
  if(flt_phpsessauth_sesspath) free(flt_phpsessauth_sesspath);
  if(flt_phpsessauth_sessname) free(flt_phpsessauth_sessname);
}

t_conf_opt flt_phpsessauth_config[] = {
  { "SessionName",  flt_phpsessauth_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { "SessionPath",  flt_phpsessauth_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "SessionVName", flt_phpsessauth_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_phpsessauth_handlers[] = {
  { AUTH_HANDLER, flt_phpsessauth_run },
  { 0, NULL }
};

t_module_config flt_phpsessauth = {
  flt_phpsessauth_config,
  flt_phpsessauth_handlers,
  NULL,
  NULL,
  NULL,
  flt_phpsessauth_cleanup
};

/* eof */
