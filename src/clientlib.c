/**
 * \file clientlib.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief client library functions
 *
 * This file contains some functions and datatypes used in client modus,
 * e.g. delete_subtree() or generate_tpl_name() or something like that
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

#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <ctype.h>

#include <stdarg.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/socket.h>

#include <dlfcn.h>

#include <locale.h>

#include <db.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#endif

#ifdef CF_SHARED_MEM
#include "semaphores.h"
#endif

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "cfcgi.h"
#include "clientlib.h"
/* }}} */

/* user authentification */
cf_hash_t *GlobalValues = NULL;

cf_hash_t *APIEntries = NULL;

static cf_list_head_t uri_flags;

/* error string */
u_char ErrorString[50];

static DB *Msgs = NULL;

#ifdef CF_SHARED_MEM
/* {{{ Shared memory functions */

int   shm_id        = -1;   /**< Shared memory id */
void *shm_ptr       = NULL; /**< Shared memory pointer */
int   shm_lock_sem  = -1;   /**< semaphore showing which shared memory segment we shall use */

void *cf_reget_shm_ptr(int ids[3]) {
  unsigned short val;

  if(shm_ptr) {
    shmdt(shm_ptr);
    shm_ptr = NULL;
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    if((shm_id = shmget(ids[val],0,0)) == -1) return NULL;

    /*
     * we don't have and don't *need* write-permissions.
     * So set SHM_RDONLY in the shmat-flag.
     */
    if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) return NULL;
  }

  return shm_ptr;
}

void *cf_get_shm_ptr(int ids[3]) {
  unsigned short val;

  if(shm_lock_sem == -1) {
    /* create a new segment */
    if((shm_lock_sem = semget(ids[2],0,0)) == -1) return NULL;
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    val = val == 1 ? 0 : 1;

    if(shm_id == -1) {
      if((shm_id = shmget(ids[val],0,0)) == -1) return NULL;
    }

    if(shm_ptr == NULL) {
      /*
       * we don't have and don't *need* write-permissions.
       * So set SHM_RDONLY in the shmat-flag.
       */
      if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) return NULL;
    }
  }

  return shm_ptr;
}

/* }}} */
#endif

/* {{{ cf_get_uconf_name */
u_char *cf_get_uconf_name(const u_char *confpath,const u_char *uname) {
  u_char *path;
  u_char *ptr;
  struct stat sb;

  cf_string_t str;

  if(!uname || !confpath) return NULL;

  cf_str_init_growth(&str,256);
  cf_str_cstr_set(&str,confpath);

  if(str.content[str.len-1] != '/') cf_str_char_append(&str,'/');
  cf_str_char_append(&str,tolower(uname[0]));
  cf_str_char_append(&str,'/');
  cf_str_char_append(&str,tolower(uname[1]));
  cf_str_char_append(&str,'/');
  cf_str_char_append(&str,tolower(uname[2]));
  cf_str_char_append(&str,'/');

  for(ptr=(u_char *)uname;*ptr;ptr++) cf_str_char_append(&str,tolower(*ptr));

  cf_str_chars_append(&str,".cfcl",5);

  if(stat(path,&sb) == -1) {
    fprintf(stderr,"clientlib: user config file '%s' not found!\n",str.content);
    cf_str_cleanup(&str);
    return NULL;
  }

  return str.content;
}
/* }}} */


/* {{{ cf_socket_setup */
int cf_socket_setup(const u_char *sockpath) {
  int sock;
  struct sockaddr_un addr;

  memset(&addr,0,sizeof(addr));
  addr.sun_family = AF_LOCAL;
  (void)strncpy(addr.sun_path,sockpath,103);

  if((sock = socket(AF_LOCAL,SOCK_STREAM,0)) == -1) {
    strcpy(ErrorString,"E_NO_SOCK");
    return -1;
  }

  if((connect(sock,(struct sockaddr *)&addr,sizeof(addr))) != 0) {
    strcpy(ErrorString,"E_NO_CONN");
    return -1;
  }

  return sock;
}
/* }}} */


/* {{{ cf_gen_tpl_name */
size_t cf_gen_tpl_name(u_char *buff,size_t len,const u_char *mode,const u_char *lang,const u_char *name) {
  return snprintf(buff,len,name,lang,mode);
}
/* }}} */

/* {{{ cf_set_variable */
void cf_set_variable(cf_template_t *tpl,const u_char *cs,u_char *vname,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs,"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs,&len1,0);
        html = 0;
      }
      else tmp = charset_convert_entities(val,len,"UTF-8",cs,&len1);

      /* This should only happen if we use charset_convert() -- and we should not use it. */
      if(!tmp) {
        tmp = htmlentities(val,0);
        len1 = strlen(val);
      }

      cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,tmp,len1);
      free(tmp);
    }
    /* ExternCharset is also UTF-8 */
    else {
      if(html) {
        tmp = htmlentities(val,0);
        len = strlen(tmp);

        cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,tmp,len);
        free(tmp);
      }
      else cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,val,len);
    }
  }
  else {
    if(html) {
      tmp = htmlentities(val,0);
      len = strlen(tmp);

      cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,tmp,len);
      free(tmp);
    }
    else cf_tpl_setvalue(tpl,vname,TPL_VARIABLE_STRING,val,len);
  }
}
/* }}} */

/* {{{ cf_add_variable */
void cf_add_variable(cf_tpl_variable_t *ary,const u_char *cs,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs,"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs,&len1,0);
        html = 0;
      }
      else tmp = charset_convert_entities(val,len,"UTF-8",cs,&len1);

      /* This should only happen if we use charset_convert() -- and we should not use it. */
      if(!tmp) {
        tmp = htmlentities(val,0);
        len1 = strlen(val);
      }

      cf_tpl_var_addvalue(ary,TPL_VARIABLE_STRING,tmp,len1);
      free(tmp);
    }
    /* ExternCharset is also UTF-8 */
    else {
      if(html) {
        tmp = htmlentities(val,0);
        len = strlen(tmp);

        cf_tpl_var_addvalue(ary,TPL_VARIABLE_STRING,tmp,len);
        free(tmp);
      }
      else cf_tpl_var_addvalue(ary,TPL_VARIABLE_STRING,val,len);
    }
  }
  else {
    if(html) {
      tmp = htmlentities(val,0);
      len = strlen(tmp);

      cf_tpl_var_addvalue(ary,TPL_VARIABLE_STRING,tmp,len);
      free(tmp);
    }
    else cf_tpl_var_addvalue(ary,TPL_VARIABLE_STRING,val,len);
  }
}
/* }}} */

/* {{{ cf_set_variable_hash */
void cf_set_variable_hash(cf_tpl_variable_t *hash,const u_char *cs,u_char *key,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs,"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs,&len1,0);
        html = 0;
      }
      else tmp = charset_convert_entities(val,len,"UTF-8",cs,&len1);

      /* This should only happen if we use charset_convert() -- and we should not use it. */
      if(!tmp) {
        tmp = htmlentities(val,0);
        len1 = strlen(val);
      }

      cf_tpl_hashvar_setvalue(hash,key,TPL_VARIABLE_STRING,tmp,len1);
      free(tmp);
    }
    /* ExternCharset is also UTF-8 */
    else {
      if(html) {
        tmp = htmlentities(val,0);
        len = strlen(tmp);

        cf_tpl_hashvar_setvalue(hash,key,TPL_VARIABLE_STRING,tmp,len);
        free(tmp);
      }
      else cf_tpl_hashvar_setvalue(hash,key,TPL_VARIABLE_STRING,val,len);
    }
  }
  else {
    if(html) {
      tmp = htmlentities(val,0);
      len = strlen(tmp);

      cf_tpl_hashvar_setvalue(hash,key,TPL_VARIABLE_STRING,tmp,len);
      free(tmp);
    }
    else cf_tpl_hashvar_setvalue(hash,key,TPL_VARIABLE_STRING,val,len);
  }
}
/* }}} */


/* {{{ cf_error_message */
void cf_error_message(cf_cfg_config_t *cfg,const u_char *err,FILE *out, ...) {
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"ErrorTemplate");
  cf_cfg_config_value_t *db = cf_cfg_get_value(cfg,"MessagesDatabase");
  cf_cfg_config_value_t *lang = cf_cfg_get_value(cfg,"Language");
  cf_cfg_config_value_t *cs = cf_cfg_get_value(cfg,"ExternCharset");
  cf_cfg_config_value_t *vs = cf_cfg_get_value(cfg,"BaseURL");
  cf_cfg_config_value_t *mode = cf_cfg_get_value(cfg,"TemplateMode");
  int isuser = cf_hash_get(GlobalValues,"UserName",8) != NULL;
  cf_template_t tpl;
  u_char tplname[256];
  u_char errname[256];
  va_list ap;

  u_char *buff = NULL,ibuff[256];
  register u_char *ptr;
  cf_string_t msg;

  int ivar,ret;
  double fvar;
  u_char *svar;

  size_t size;

  DBT key,value;

  if(v && db && lang) {
    cf_gen_tpl_name(tplname,256,mode->sval,lang->sval,v->sval);

    if(cf_tpl_init(&tpl,tplname) == 0) {
      cf_set_variable(&tpl,cs->sval,"forumbase",vs->avals[isuser].sval,strlen(vs->avals[isuser].sval),1);

      cf_str_init(&msg);

      if(Msgs == NULL) {
        if((ret = db_create(&Msgs,NULL,0)) == 0) {
          if((ret = Msgs->open(Msgs,NULL,db->sval,NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
            fprintf(stderr,"clientlib: DB->open(%s) error: %s\n",db->sval,db_strerror(ret));
          }
        }
        else fprintf(stderr,"clientlib: db_create() error: %s\n",db_strerror(ret));
      }

      if(Msgs) {
        memset(&key,0,sizeof(key));
        memset(&value,0,sizeof(value));

        size = snprintf(errname,256,"%s_%s",lang->sval,err);

        key.data = errname;
        key.size = size;

        if(Msgs->get(Msgs,NULL,&key,&value,0) == 0) {
          buff = strndup(value.data,value.size);
        }
      }

      if(buff) {
        va_start(ap,out);

        for(ptr=buff;*ptr;ptr++) {
          if(*ptr == '%') {
            ++ptr;

            switch(*ptr) {
              case '%':
                cf_str_char_append(&msg,*ptr);
                break;

              case 's':
                svar = va_arg(ap,u_char *);
                cf_str_chars_append(&msg,svar,strlen(svar));
                break;

              case 'd':
                ivar = va_arg(ap,int);
                size = snprintf(ibuff,50,"%d",ivar);
                cf_str_chars_append(&msg,ibuff,size);
                break;

              case 'f':
                fvar = va_arg(ap,double);
                size = snprintf(ibuff,50,"%.2f",fvar);
                str_chars_append(&msg,ibuff,size);
                break;

              default:
                cf_str_char_append(&msg,*ptr);
                break;
            }
          }
          else {
            cf_str_char_append(&msg,*ptr);
          }
        }

        va_end(ap);

        cf_set_variable(&tpl,cs->sval,"error",msg.content,msg.len,1);
        cf_str_cleanup(&msg);
        free(buff);

        if(out) {
          cf_tpl_parse_to_mem(&tpl);
          fwrite(tpl.parsed.content,1,tpl.parsed.len,out);
        }
        else {
          cf_tpl_parse(&tpl);
        }

        cf_tpl_finish(&tpl);
      }
      else printf("Sorry, internal error (%s), cannot do anything. Perhaps you should kick your system administrator.\n",err);
    }
    else printf("Sorry, could not find template file. I got error %s\n",err);
  }
  else printf("Sorry, but I could not find my configuration.\nI got error %s\n",err);
}
/* }}} */

/* {{{ cf_get_error_message */
u_char *cf_get_error_message(cf_cfg_config_t *cfg,const u_char *err,size_t *len, ...) {
  cf_cfg_config_value_t *db = cf_cfg_get_value(cfg,"MessagesDatabase");
  cf_cfg_config_value_t *lang = cf_cfg_get_value(cfg,"Language");
  va_list ap;

  u_char *buff = NULL,ibuff[256],errname[256];
  register u_char *ptr;
  cf_string_t msg;

  int ivar,ret;
  double fvar;
  u_char *svar;

  size_t size;

  DBT key,value;

  cf_str_init(&msg);

  if(Msgs == NULL) {
    if((ret = db_create(&Msgs,NULL,0)) == 0) {
      if((ret = Msgs->open(Msgs,NULL,db->sval,NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
        fprintf(stderr,"clientlib: DB->open(%s) error: %s\n",db->sval,db_strerror(ret));
        return NULL;
      }
    }
    else {
      fprintf(stderr,"clientlib: db_create() error: %s\n",db_strerror(ret));
      return NULL;
    }
  }

  memset(&key,0,sizeof(key));
  memset(&value,0,sizeof(value));

  size = snprintf(errname,256,"%s_%s",lang->sval,err);

  key.data = errname;
  key.size = size;

  if(Msgs->get(Msgs,NULL,&key,&value,0) == 0) buff = strndup(value.data,value.size);
  else return NULL;

  va_start(ap,len);

  for(ptr=buff;*ptr;ptr++) {
    if(*ptr == '%') {
      ptr++;

      switch(*ptr) {
        case '%':
          cf_str_char_append(&msg,*ptr);
          break;

        case 's':
          svar = va_arg(ap,u_char *);
          cf_str_chars_append(&msg,svar,strlen(svar));
          break;

        case 'd':
          ivar = va_arg(ap,int);
          size = snprintf(ibuff,50,"%d",ivar);
          cf_str_chars_append(&msg,ibuff,size);
          break;

        case 'f':
          fvar = va_arg(ap,double);
          size = snprintf(ibuff,50,"%.2f",fvar);
          str_chars_append(&msg,ibuff,size);
          break;

        default:
          cf_str_char_append(&msg,*ptr);
          break;
      }
    }
    else cf_str_char_append(&msg,*ptr);
  }


  va_end(ap);
  free(buff);
  if(len) *len = msg.len;

  return msg.content;
}
/* }}} */


/* {{{ cf_add_static_uri_flag */
void cf_add_static_uri_flag(const u_char *name,const u_char *value,int encode) {
  cf_uri_flag_t flag;

  flag.name   = strdup(name);
  flag.val    = strdup(value);
  flag.encode = encode;

  cf_list_append(&uri_flags,&flag,sizeof(flag));
}
/* }}} */

/* {{{ cf_remove_static_uri_flag */
void cf_remove_static_uri_flag(const u_char *name) {
  cf_uri_flag_t *flag;
  cf_list_element_t *elem;

  for(elem=uri_flags.elements;elem;elem=elem->next) {
    flag = (cf_uri_flag_t *)elem->data;

    if(cf_strcmp(flag->name,name) == 0) {
      cf_list_delete(&uri_flags,elem);

      free(flag->name);
      free(flag->val);
      free(flag);
    }
  }

}
/* }}} */

/* {{{ cf_get_link */
u_char *cf_get_link(const u_char *link,u_int64_t tid,u_int64_t mid) {
  register const u_char *ptr;
  cf_string_t buff;
  u_char *tmp;
  int qm = 0,run = 1;
  cf_readmode_t *rm;
  cf_uri_flag_t *flag;
  cf_list_element_t *elem;
  size_t len;

  cf_str_init_growth(&buff,128);

  if(link == NULL)  {
    rm = cf_hash_get(GlobalValues,"RM",2);
    if(rm) link = rm->posting_uri[cf_hash_get(GlobalValues,"UserName",8) ? 1 : 0];
    else return NULL;
    if(!link) return NULL;
  }

  /* {{{ work on link template */
  for(ptr=link;*ptr && run;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1) == 't') {
          cf_uint64_to_str(&buff,tid);
          ptr += 1;
        }
        else if(*(ptr+1) == 'm') {
          cf_uint64_to_str(&buff,mid);
          ptr += 1;
        }
        else cf_str_char_append(&buff,*ptr);
        break;

      case '#':
        /* uh! Anchor in link template */
        ptr -= 1;
        run = 0;
        break;

      case '?':
        qm = 1;

      default:
        cf_str_char_append(&buff,*ptr);
    }
  }
  /* }}} */

  /* {{{ append static URI flags */
  for(elem=uri_flags.elements;elem;elem=elem->next) {
    flag = (cf_uri_flag_t *)elem->data;

    if(qm) cf_str_char_append(&buff,'&');
    else {
      cf_str_char_append(&buff,'?');
      qm = 1;
    }

    if(flag->encode) {
      len = strlen(flag->name);
      tmp = cf_cgi_url_encode(flag->name,&len);
      cf_str_chars_append(&buff,tmp,len);
      free(tmp);

      cf_str_char_append(&buff,'=');

      len = strlen(flag->val);
      tmp = cf_cgi_url_encode(flag->val,&len);
      cf_str_chars_append(&buff,tmp,len);
      free(tmp);
    }
    else {
      cf_str_chars_append(&buff,flag->name,strlen(flag->name));
      cf_str_char_append(&buff,'=');
      cf_str_chars_append(&buff,flag->val,strlen(flag->val));
    }
  }
  /* }}} */

  /* {{{ we got an anchor, perhaps, append it to the _end_ of the uri */
  if(*ptr) {
    for(;*ptr;++ptr) {
      switch(*ptr) {
        case '%':
          if(*(ptr+1) == 't') {
            cf_uint64_to_str(&buff,tid);
            ptr += 1;
          }
          else if(*(ptr+1) == 'm') {
            cf_uint64_to_str(&buff,mid);
            ptr += 1;
          }
          else cf_str_char_append(&buff,*ptr);

          break;

        default:
          cf_str_char_append(&buff,*ptr);
      }
    }
  }
  /* }}} */

  return buff.content;
}
/* }}} */

/* {{{ cf_advanced_get_link */
u_char *cf_advanced_get_link(const u_char *link,u_int64_t tid,u_int64_t mid,u_char *anchor,size_t plen,size_t *l,...) {
  register const u_char *ptr;
  cf_string_t buff;
  int qm = 0,run = 1;
  u_char *name,*value,*tmp;
  size_t i;
  va_list ap;
  cf_uri_flag_t *flag;
  cf_list_element_t *elem;
  size_t namlen,vallen;

  cf_str_init_growth(&buff,128);

  /* {{{ work on link template */
  for(ptr=link;*ptr && run;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1) == 't') {
          cf_uint64_to_str(&buff,tid);
          ptr += 1;
        }
        else if(*(ptr+1) == 'm') {
          cf_uint64_to_str(&buff,mid);
          ptr += 1;
        }
        else cf_str_char_append(&buff,*ptr);

        break;
      case '#':
        /* uh! Anchor in link template */
        ptr -= 1;
        run = 0;
        break;

      case '?':
        qm = 1;
      default:
        cf_str_char_append(&buff,*ptr);
    }
  }
  /* }}} */

  /* {{{ append uri arguments */
  va_start(ap,l);
  for(i=0;i<plen;++i) {
    tmp   = va_arg(ap,u_char *);
    namlen= strlen(tmp);
    name  = cf_cgi_url_encode(tmp,&namlen);

    tmp   = va_arg(ap,u_char *);
    vallen= strlen(tmp);
    value = cf_cgi_url_encode(tmp,&vallen);

    if(qm == 0) {
      cf_str_char_append(&buff,'?');
      qm = 1;
    }
    else cf_str_char_append(&buff,'&');

    cf_str_chars_append(&buff,name,namlen);
    cf_str_char_append(&buff,'=');
    cf_str_chars_append(&buff,value,vallen);

    free(name);
    free(value);
  }
  va_end(ap);
  /* }}} */

  /* {{{ append static uri flags */
  for(elem=uri_flags.elements;elem;elem=elem->next) {
    flag = (cf_uri_flag_t *)elem->data;

    cf_str_char_append(&buff,'&');

    if(flag->encode) {
      namlen = strlen(flag->name);
      tmp    = cf_cgi_url_encode(flag->name,&namlen);
      cf_str_chars_append(&buff,tmp,namlen);
      free(tmp);

      cf_str_char_append(&buff,'=');

      vallen = strlen(flag->val);
      tmp    = cf_cgi_url_encode(flag->val,&vallen);
      cf_str_chars_append(&buff,tmp,vallen);
      free(tmp);
    }
    else {
      cf_str_chars_append(&buff,flag->name,strlen(flag->name));
      cf_str_char_append(&buff,'=');
      cf_str_chars_append(&buff,flag->val,strlen(flag->val));
    }
  }
  /* }}} */

  /* {{{ append anchor */
  if(anchor) {
    cf_str_char_append(&buff,'#');
    cf_str_chars_append(&buff,anchor,strlen(anchor));
  }
  /* }}} */

  /* {{{ we got an anchor, perhaps, append it to the _end_ of the uri */
  if(*ptr) {
    for(;*ptr;++ptr) {
      switch(*ptr) {
        case '%':
          if(*(ptr+1) == 't') {
            cf_uint64_to_str(&buff,tid);
            ptr += 1;
          }
          else if(*(ptr+1) == 'm') {
            cf_uint64_to_str(&buff,mid);
            ptr += 1;
          }
          else cf_str_char_append(&buff,*ptr);

          break;

        default:
          cf_str_char_append(&buff,*ptr);
      }
    }
  }
  /* }}} */

  if(l) *l = buff.len;
  return buff.content;
}
/* }}} */

/* {{{ cf_general_get_time */
u_char *cf_general_get_time(const u_char *fmt,const u_char *locale,size_t *len,time_t *date) {
  u_char *buff;
  struct tm *tptr;
  size_t ln;

  if(!setlocale(LC_TIME,locale)) return NULL;

  ln    = strlen(fmt) + 256;
  buff  = cf_alloc(NULL,ln,1,CF_ALLOC_MALLOC);
  tptr  = localtime(date);

  *len = strftime(buff,ln,fmt,tptr);

  return buff;
}
/* }}} */


/* {{{ cf_flag_by_name */
cf_post_flag_t *cf_flag_by_name(cf_list_head_t *flags,const u_char *name) {
  cf_list_element_t *elem;
  cf_post_flag_t *flag;

  for(elem=flags->elements;elem;elem=elem->next) {
    flag = (cf_post_flag_t *)elem->data;
    if(cf_strcmp(flag->name,name) == 0) return flag;
  }

  return NULL;
}
/* }}} */


/* {{{ cf_register_mod_api_ent */
int cf_register_mod_api_ent(const u_char *mod_name,const u_char *unique_identifier,cf_mod_api_t func) {
  size_t len2 = strlen(unique_identifier);
  cf_mod_api_ent_t *ent,ent1;

  if((ent = cf_hash_get(APIEntries,(u_char *)unique_identifier,len2)) != NULL) {
    if(cf_strcmp(ent->mod_name,mod_name)) return -1;

    ent->function          = func;
  }
  else {
    ent1.mod_name          = strdup(mod_name);
    ent1.unique_identifier = strdup(unique_identifier);
    ent1.function          = func;

    cf_hash_set(APIEntries,(u_char *)unique_identifier,len2,&ent1,sizeof(ent1));
  }

  return 0;
}
/* }}} */

/* {{{ cf_unregister_mod_api_ent */
int cf_unregister_mod_api_ent(const u_char *unid) {
  size_t len1 = strlen(unid);
  cf_mod_api_ent_t *ent;

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) return -1;

  cf_hash_entry_delete(APIEntries,(u_char *)unid,len1);
  return 0;
}
/* }}} */

/* {{{ cf_get_mod_api_ent */
cf_mod_api_t cf_get_mod_api_ent(const u_char *unid) {
  cf_mod_api_ent_t *ent;
  size_t len1 = strlen(unid);

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) return NULL;
  return ent->function;
}
/* }}} */



/* {{{ cf_api_destroy_entry */
/**
 * private function for destroying a module api entry
 * \param a The element
 */
void cf_api_destroy_entry(cf_mod_api_ent_t *a) {
  free(a->mod_name);
  free(a->unique_identifier);
}
/* }}} */

/* {{{ cf_init */
/**
 * library constructor function
 */
void cf_init(void) {
  u_char *val = getenv("CF_FORUM_NAME");

  GlobalValues = cf_hash_new(NULL);
  APIEntries = cf_hash_new((void (*)(void *))cf_api_destroy_entry);
  memset(ErrorString,0,sizeof(ErrorString));

  cf_list_init(&uri_flags);

  if(val) cf_hash_set(GlobalValues,"FORUM_NAME",10,val,strlen(val)+1);
}
/* }}} */

/* {{{ cf_fini */
/**
 * library destructor function
 */
void cf_fini(void) {
  cf_hash_destroy(GlobalValues);
  cf_hash_destroy(APIEntries);
}
/* }}} */

/* eof */
