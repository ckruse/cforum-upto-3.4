/**
 * \file clientlib.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
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
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "cfcgi.h"
#include "clientlib.h"
/* }}} */

/* user authentification */
t_cf_hash *GlobalValues = NULL;

t_cf_hash *APIEntries = NULL;

static t_cf_list_head uri_flags;

/* error string */
u_char ErrorString[50];

static DB *Msgs = NULL;

#ifdef CF_SHARED_MEM
/* {{{ Shared memory functions */

int   shm_id        = -1;   /**< Shared memory id */
void *shm_ptr       = NULL; /**< Shared memory pointer */
int   shm_lock_sem  = -1;   /**< semaphore showing which shared memory segment we shall use */

void *cf_reget_shm_ptr(void) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *shm  = cfg_get_first_value(&fo_default_conf,forum_name,"SharedMemIds");
  unsigned short val;

  if(shm_ptr) {
    shmdt(shm_ptr);
    shm_ptr = NULL;
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    if((shm_id = shmget(atoi(shm->values[val]),0,0)) == -1) {
      return NULL;
    }

    /*
     * we don't have and don't *need* write-permissions.
     * So set SHM_RDONLY in the shmat-flag.
     */
    if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) {
      return NULL;
    }
  }

  return shm_ptr;
}

void *cf_get_shm_ptr(void) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *shm  = cfg_get_first_value(&fo_default_conf,forum_name,"SharedMemIds");
  unsigned short val;

  if(shm_lock_sem == -1) {
    /* create a new segment */
    if((shm_lock_sem = semget(atoi(shm->values[2]),0,0)) == -1) {
      //perror("semget");
      return NULL;
    }
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    val = val == 1 ? 0 : 1;

    if(shm_id == -1) {
      if((shm_id = shmget(atoi(shm->values[val]),0,0)) == -1) {
        //perror("shmget");
        return NULL;
      }
    }

    if(shm_ptr == NULL) {
      /*
       * we don't have and don't *need* write-permissions.
       * So set SHM_RDONLY in the shmat-flag.
       */
      if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) {
        //perror("shmat");
        return NULL;
      }
    }
  }

  return shm_ptr;
}

/* }}} */
#endif

/* {{{ cf_get_uconf_name */
u_char *cf_get_uconf_name(const u_char *uname) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *path,*name;
  u_char *ptr;
  struct stat sb;
  t_name_value *confpath = cfg_get_first_value(&fo_default_conf,forum_name,"ConfigDirectory");

  if(!uname) return NULL;

  name = strdup(uname);
  if(!name) return NULL;

  for(ptr = name;*ptr;ptr++) {
    if(isupper(*ptr)) {
      *ptr = tolower(*ptr);
    }
  }

  path = fo_alloc(NULL,strlen(confpath->values[0]) + strlen(name) + 12,1,FO_ALLOC_MALLOC);

  sprintf(path,"%s%c/%c/%c/%s.conf",confpath->values[0],name[0],name[1],name[2],name);

  if(stat(path,&sb) == -1) {
    fprintf(stderr,"clientlib: user config file '%s' not found!\n",path);
    free(path);
    free(name);
    return NULL;
  }

  free(name);

  return path;
}
/* }}} */


/* {{{ cf_socket_setup */
int cf_socket_setup(void) {
  int sock;
  struct sockaddr_un addr;
  t_name_value *sockpath = cfg_get_first_value(&fo_default_conf,NULL,"SocketName");

  if(sockpath) {
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;
    (void)strncpy(addr.sun_path,sockpath->values[0],103);

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

  strcpy(ErrorString,"E_CONFIG_ERR");
  return -1;
}
/* }}} */


/* {{{ cf_gen_tpl_name */
void cf_gen_tpl_name(u_char *buff,size_t len,const u_char *name) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *vn = cfg_get_first_value(&fo_default_conf,forum_name,"TemplateMode");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,forum_name,"Language");

  snprintf(buff,len,name,lang->values[0],vn->values[0]);
}
/* }}} */

/* {{{ cf_set_variable */
void cf_set_variable(t_cf_template *tpl,t_name_value *cs,u_char *vname,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs->values[0],"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs->values[0],&len1,0);
        html = 0;
      }
      else tmp = charset_convert_entities(val,len,"UTF-8",cs->values[0],&len1);

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

/* {{{ cf_set_variable_hash */
void cf_set_variable_hash(t_cf_tpl_variable *hash,t_name_value *cs,u_char *key,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs->values[0],"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs->values[0],&len1,0);
        html = 0;
      }
      else tmp = charset_convert_entities(val,len,"UTF-8",cs->values[0],&len1);

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
void cf_error_message(const u_char *err,FILE *out, ...) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *v = cfg_get_first_value(&fo_default_conf,forum_name,"ErrorTemplate");
  t_name_value *db = cfg_get_first_value(&fo_default_conf,forum_name,"MessagesDatabase");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,forum_name,"Language");
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  t_name_value *vs = cfg_get_first_value(&fo_default_conf,forum_name,cf_hash_get(GlobalValues,"UserName",8) ? "UBaseURL" : "BaseURL");
  t_cf_template tpl;
  u_char tplname[256];
  u_char errname[256];
  va_list ap;

  u_char *buff = NULL,ibuff[256];
  register u_char *ptr;
  t_string msg;

  int ivar,ret;
  u_char *svar;

  size_t size;

  DBT key,value;

  if(v && db && lang) {
    cf_gen_tpl_name(tplname,256,v->values[0]);

    if(cf_tpl_init(&tpl,tplname) == 0) {
      cf_set_variable(&tpl,cs,"forumbase",vs->values[0],strlen(vs->values[0]),1);

      str_init(&msg);

      if(Msgs == NULL) {
        if((ret = db_create(&Msgs,NULL,0)) == 0) {
          if((ret = Msgs->open(Msgs,NULL,db->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
            fprintf(stderr,"clientlib: DB->open(%s) error: %s\n",db->values[0],db_strerror(ret));
          }
        }
        else fprintf(stderr,"clientlib: db_create() error: %s\n",db_strerror(ret));
      }

      if(Msgs) {
        memset(&key,0,sizeof(key));
        memset(&value,0,sizeof(value));

        size = snprintf(errname,256,"%s_%s",lang->values[0],err);

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
            ptr++;

            switch(*ptr) {
              case '%':
                str_char_append(&msg,*ptr);
                break;

              case 's':
                svar = va_arg(ap,u_char *);
                str_chars_append(&msg,svar,strlen(svar));
                break;

              case 'd':
                ivar = va_arg(ap,int);
                size = snprintf(ibuff,50,"%d",ivar);
                str_chars_append(&msg,ibuff,50);
                break;

              default:
                str_char_append(&msg,*ptr);
                break;
            }
          }
          else {
            str_char_append(&msg,*ptr);
          }
        }

        va_end(ap);

        cf_set_variable(&tpl,cs,"error",msg.content,msg.len,1);
        str_cleanup(&msg);
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
u_char *cf_get_error_message(const u_char *err,size_t *len, ...) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  t_name_value *db = cfg_get_first_value(&fo_default_conf,forum_name,"MessagesDatabase");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,forum_name,"Language");
  va_list ap;

  u_char *buff = NULL,ibuff[256],errname[256];
  register u_char *ptr;
  t_string msg;

  int ivar,ret;
  u_char *svar;

  size_t size;

  DBT key,value;

  str_init(&msg);

  if(Msgs == NULL) {
    if((ret = db_create(&Msgs,NULL,0)) == 0) {
      if((ret = Msgs->open(Msgs,NULL,db->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
        fprintf(stderr,"clientlib: DB->open(%s) error: %s\n",db->values[0],db_strerror(ret));
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

  size = snprintf(errname,256,"%s_%s",lang->values[0],err);

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
          str_char_append(&msg,*ptr);
          break;

        case 's':
          svar = va_arg(ap,u_char *);
          str_chars_append(&msg,svar,strlen(svar));
          break;

        case 'd':
          ivar = va_arg(ap,int);
          size = snprintf(ibuff,50,"%d",ivar);
          str_chars_append(&msg,ibuff,50);
          break;

        default:
          str_char_append(&msg,*ptr);
          break;
      }
    }
    else str_char_append(&msg,*ptr);
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
  t_cf_list_element *elem;

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
  t_string buff;
  u_char *tmp;
  int qm = 0;
  cf_readmode_t *rm;
  cf_uri_flag_t *flag;
  t_cf_list_element *elem;

  str_init_growth(&buff,128);

  if(link == NULL)  {
    rm = cf_hash_get(GlobalValues,"RM",2);
    if(rm) link = rm->posting_uri[cf_hash_get(GlobalValues,"UserName",8) ? 1 : 0];
    else return NULL;
  }

  if(link) {
    for(ptr=link;*ptr;++ptr) {
      switch(*ptr) {
        case '%':
          if(*(ptr+1) == 't') {
            u_int64_to_str(&buff,tid);
            ptr += 1;
          }
          else if(*(ptr+1) == 'm') {
            u_int64_to_str(&buff,mid);
            ptr += 1;
          }
          else str_char_append(&buff,*ptr);
          break;

        case '?':
          qm = 1;
        default:
          str_char_append(&buff,*ptr);
      }
    }
  }

  for(elem=uri_flags.elements;elem;elem=elem->next) {
    flag = (cf_uri_flag_t *)elem->data;

    if(qm) str_char_append(&buff,'&');
    else {
      str_char_append(&buff,'?');
      qm = 1;
    }

    if(flag->encode) {
      tmp = cf_cgi_url_encode(flag->name,strlen(flag->name));
      str_chars_append(&buff,tmp,strlen(tmp));
      free(tmp);

      str_char_append(&buff,'=');

      tmp = cf_cgi_url_encode(flag->val,strlen(flag->val));
      str_chars_append(&buff,tmp,strlen(tmp));
      free(tmp);
    }
    else {
      str_chars_append(&buff,flag->name,strlen(flag->name));
      str_char_append(&buff,'=');
      str_chars_append(&buff,flag->val,strlen(flag->val));
    }
  }


  return buff.content;
}
/* }}} */

/* {{{ cf_advanced_get_link */
u_char *cf_advanced_get_link(const u_char *link,u_int64_t tid,u_int64_t mid,u_char *anchor,size_t plen,size_t *l,...) {
  register const u_char *ptr;
  t_string buff;
  u_char *my_anchor;
  int qm = 0,run = 1;
  u_char *name,*value,*tmp;
  size_t i;
  va_list ap;
  cf_uri_flag_t *flag;
  t_cf_list_element *elem;

  str_init_growth(&buff,128);

  /* {{{ work on link template */
  for(ptr=link;*ptr && run;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1) == 't') {
          u_int64_to_str(&buff,tid);
          ptr += 1;
        }
        else if(*(ptr+1) == 'm') {
          u_int64_to_str(&buff,mid);
          ptr += 1;
        }
        else str_char_append(&buff,*ptr);

        break;
      case '#':
        /* uh! Anchor in link template */
        ptr -= 1;
        run = 0;
        break;

      case '?':
        qm = 1;
      default:
        str_char_append(&buff,*ptr);
    }
  }
  /* }}} */

  /* {{{ append uri arguments */
  va_start(ap,l);
  for(i=0;i<plen;++i) {
    tmp = va_arg(ap,u_char *);
    name = cf_cgi_url_encode(tmp,strlen(tmp));

    tmp = va_arg(ap,u_char *);
    value = cf_cgi_url_encode(tmp,strlen(tmp));

    if(qm == 0) {
      str_char_append(&buff,'?');
      qm = 1;
    }
    else str_char_append(&buff,'&');

    str_chars_append(&buff,name,strlen(name));
    str_char_append(&buff,'=');
    str_chars_append(&buff,value,strlen(value));

    free(name);
    free(value);
  }
  va_end(ap);
  /* }}} */

  /* {{{ append static uri flags */
  for(elem=uri_flags.elements;elem;elem=elem->next) {
    flag = (cf_uri_flag_t *)elem->data;

    str_char_append(&buff,'&');

    if(flag->encode) {
      tmp = cf_cgi_url_encode(flag->name,strlen(flag->name));
      str_chars_append(&buff,tmp,strlen(tmp));
      free(tmp);

      str_char_append(&buff,'=');

      tmp = cf_cgi_url_encode(flag->val,strlen(flag->val));
      str_chars_append(&buff,tmp,strlen(tmp));
      free(tmp);
    }
    else {
      str_chars_append(&buff,flag->name,strlen(flag->name));
      str_char_append(&buff,'=');
      str_chars_append(&buff,flag->val,strlen(flag->val));
    }
  }
  /* }}} */

  /* {{{ append anchor */
  if(anchor) {
    str_char_append(&buff,'#');
    str_chars_append(&buff,anchor,strlen(anchor));
  }
  /* }}} */

  /* {{{ we got an anchor, perhaps, append it to the _end_ of the uri */
  if(*ptr) {
    for(;*ptr;++ptr) {
      switch(*ptr) {
        case '%':
          if(*(ptr+1) == 't') {
            u_int64_to_str(&buff,tid);
            ptr += 1;
          }
          else if(*(ptr+1) == 'm') {
            u_int64_to_str(&buff,mid);
            ptr += 1;
          }
          else str_char_append(&buff,*ptr);

          break;

        default:
          str_char_append(&buff,*ptr);
      }
    }
  }
  /* }}} */

  if(l) *l = buff.len;
  return buff.content;
}
/* }}} */

/* {{{ cf_general_get_time */
u_char *cf_general_get_time(u_char *fmt,u_char *locale,int *len,time_t *date) {
  u_char *buff;
  struct tm *tptr;
  size_t ln;

  if(!setlocale(LC_TIME,locale)) return NULL;

  ln    = strlen(fmt) + 256;
  buff  = fo_alloc(NULL,ln,1,FO_ALLOC_MALLOC);
  tptr  = localtime(date);

  *len = strftime(buff,ln,fmt,tptr);

  return buff;
}
/* }}} */


/* {{{ cf_flag_by_name */
t_cf_post_flag *cf_flag_by_name(t_cf_list_head *flags,const u_char *name) {
  t_cf_list_element *elem;
  t_cf_post_flag *flag;

  for(elem=flags->elements;elem;elem=elem->next) {
    flag = (t_cf_post_flag *)elem->data;
    if(cf_strcmp(flag->name,name) == 0) return flag;
  }

  return NULL;
}
/* }}} */


/* {{{ cf_register_mod_api_ent */
int cf_register_mod_api_ent(const u_char *mod_name,const u_char *unique_identifier,t_mod_api func) {
  size_t len2 = strlen(unique_identifier);
  t_mod_api_ent *ent;

  if((ent = cf_hash_get(APIEntries,(u_char *)unique_identifier,len2)) != NULL) {
    if(cf_strcmp(ent->mod_name,mod_name)) {
      return -1;
    }

    ent->function          = func;
  }
  else {
    ent                    = fo_alloc(NULL,1,sizeof(*ent),FO_ALLOC_MALLOC);
    ent->mod_name          = strdup(mod_name);
    ent->unique_identifier = strdup(unique_identifier);
    ent->function          = func;

    cf_hash_set(APIEntries,(u_char *)unique_identifier,len2,ent,sizeof(*ent));

    free(ent);
  }

  return 0;
}
/* }}} */

/* {{{ cf_unregister_mod_api_ent */
int cf_unregister_mod_api_ent(const u_char *unid) {
  size_t len1 = strlen(unid);
  t_mod_api_ent *ent;

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) {
    return -1;
  }

  cf_hash_entry_delete(APIEntries,(u_char *)unid,len1);
  return 0;
}
/* }}} */

/* {{{ cf_get_mod_api_ent */
t_mod_api cf_get_mod_api_ent(const u_char *unid) {
  t_mod_api_ent *ent;
  size_t len1 = strlen(unid);

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) {
    return NULL;
  }

  return ent->function;
}
/* }}} */



/* {{{ cf_api_destroy_entry */
/**
 * private function for destroying a module api entry
 * \param elem The element
 */
void cf_api_destroy_entry(void *elem) {
  t_mod_api_ent *a = (t_mod_api_ent *)elem;
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
  APIEntries = cf_hash_new(cf_api_destroy_entry);
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
