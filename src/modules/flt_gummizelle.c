/**
 * \file flt_gummizelle.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin provides gummizelle-functionality
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
#include <ctype.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

typedef struct {
  u_char *uname,*ip,*name;

  time_t start;
  time_t end;

  u_int64_t tid;
} flt_gummizelle_t;

static u_char *flt_gummizelle_db = NULL;
static u_char *flt_gummizelle_fn = NULL;

static int flt_gummizelle_is_caged = 0;

static u_char *flt_gummizelle_ip    = NULL;
static u_char *flt_gummizelle_uname = NULL;
static u_char *flt_gummizelle_name  = NULL;

static array_t flt_gummizelle_entries;

flt_gummizelle_t *flt_gummizelle_entry = NULL;

/* {{{ flt_gummizelle_read_string */
void flt_gummizelle_read_string(u_char *ptr,string_t *str,u_char **pos) {
  for(;*ptr;++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case 'n':
            str_char_append(str,'\n');
            break;
          case 't':
            str_char_append(str,'\t');
            break;
          case 'r':
            str_char_append(str,'r');
            break;
          default:
            str_char_append(str,*(ptr+1));
        }
        ++ptr;
        continue;

      case '"':
        *pos = ptr;
        return;

      default:
        str_char_append(str,*ptr);
    }
  }
}
/* }}} */

/* {{{ flt_gummizelle_parse_date */
time_t flt_gummizelle_parse_date(const u_char *date) {
  struct tm tm;
  int val;

  u_char *ptr;

  memset(&tm,0,sizeof(tm));

  /* we have date in y-m-d h:m:s, so first parse year */
  val = strtol(date,(char **)&ptr,10);
  tm.tm_year = val - 1900;

  /* next: month */
  val = strtol(ptr+1,(char **)&ptr,10);
  tm.tm_mon = val - 1;

  /* day of month */
  val = strtol(ptr+1,(char **)&ptr,10);
  tm.tm_mday = val;

  /* hour */
  val = strtol(ptr+1,(char **)&ptr,10);
  tm.tm_hour = val;

  /* minute */
  val = strtol(ptr+1,(char **)&ptr,10);
  tm.tm_min = val;

  /* seconds */
  val = strtol(ptr+1,(char **)&ptr,10);
  tm.tm_sec = val;

  return mktime(&tm);
}
/* }}} */

/* {{{ flt_gummizelle_check_caged */
int flt_gummizelle_check_caged(void) {
  size_t i;
  flt_gummizelle_t *ent;

  for(i=0;i<flt_gummizelle_entries.elements;++i) {
    ent = array_element_at(&flt_gummizelle_entries,i);

    if(flt_gummizelle_ip && ent->ip && cf_strcmp(ent->ip,flt_gummizelle_ip) == 0) {
      flt_gummizelle_entry = ent;
      return 1;
    }
    if(flt_gummizelle_uname && ent->uname && cf_strcmp(ent->uname,flt_gummizelle_uname) == 0) {
      flt_gummizelle_entry = ent;
      return 1;
    }
    if(flt_gummizelle_name && ent->name && cf_strcmp(ent->name,flt_gummizelle_name) == 0) {
      flt_gummizelle_entry = ent;
      return 1;
    }
  }

  return 0;
}
/* }}} */

/* {{{ flt_gummizelle_init */
int flt_gummizelle_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  u_char *UserName,*ip;
  struct stat st;
  int fd;
  u_char *f_ptr;
  u_char *ptr;

  name_value_t *v = NULL;
  flt_gummizelle_t ent;
  string_t val;

  if(flt_gummizelle_fn == NULL) flt_gummizelle_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  array_init(&flt_gummizelle_entries,sizeof(ent),NULL);

  if(!flt_gummizelle_db) return FLT_DECLINE;
  if(stat(flt_gummizelle_db,&st) == -1) {
    fprintf(stderr,"flt_gummizelle: stat: could not stat file '%s': %s\n",flt_gummizelle_db,strerror(errno));
    return FLT_DECLINE;
  }

  if(st.st_size == 0) {
    fprintf(stderr,"flt_gummizelle: stat: file '%s' is empty!\n",flt_gummizelle_db);
    return FLT_DECLINE;
  }

  if((fd = open(flt_gummizelle_db,O_RDONLY)) < 0) {
    fprintf(stderr,"flt_gummizelle: open: could not open file '%s': %s\n",flt_gummizelle_db,strerror(errno));
    return FLT_DECLINE;
  }

  if(((void *)(f_ptr = mmap(0,st.st_size+1,PROT_READ,MAP_FILE|MAP_SHARED,fd,0))) == (caddr_t)-1) {
    close(fd);
    fprintf(stderr,"flt_gummizelle: mmap: could not map file '%s': %s\n",flt_gummizelle_db,strerror(errno));
    return FLT_DECLINE;
  }

  /* parse db file */
  ptr = f_ptr;
  while(*ptr) {
    ent.uname = ent.ip = ent.name = NULL;
    ent.start = ent.end = ent.tid = 0;

    while(*ptr && *ptr != '\n') {
      /* {{{ get next token */
      /* skip leading spaces */
      for(;*ptr && isspace(*ptr);++ptr);

      if(!*ptr) continue;

      /* read name */
      str_init(&val);
      if(*ptr == '"') flt_gummizelle_read_string(ptr,&val,&ptr);
      else {
        for(;*ptr && !isspace(*ptr);++ptr) str_char_append(&val,*ptr);
      }

      if(!*ptr) {
        str_cleanup(&val);
        continue;
      }
      /* }}} */

      if(cf_strncmp(val.content,"name:",5) == 0) ent.name = strdup(val.content+5);
      else if(cf_strncmp(val.content,"uname:",6) == 0) ent.uname = strdup(val.content+6);
      else if(cf_strncmp(val.content,"ip:",3) == 0) ent.ip = strdup(val.content+3);
      else if(cf_strncmp(val.content,"start:",6) == 0) ent.start = flt_gummizelle_parse_date(val.content+6);
      else if(cf_strncmp(val.content,"end:",4) == 0) ent.end = flt_gummizelle_parse_date(val.content+4);
      else if(cf_strncmp(val.content,"tid:",4) == 0) ent.tid = str_to_u_int64(val.content+4);

      str_cleanup(&val);
    }

    if(*ptr) ++ptr;
    if(ent.name || ent.uname || ent.ip) array_push(&flt_gummizelle_entries,&ent);
  }

  close(fd);
  munmap(f_ptr,st.st_size+1);

  /* file parsed, now collect infos about the client */

  UserName = cf_hash_get(GlobalValues,"UserName",8);
  if((ip = getenv("HTTP_X_FORWARED_FOR")) == NULL) ip = getenv("REMOTE_ADDR");
  if(UserName) v = cfg_get_first_value(vc,flt_gummizelle_fn,"Name");

  if(v) flt_gummizelle_name = v->values[0];
  flt_gummizelle_ip    = ip;
  flt_gummizelle_uname = UserName;

  if((flt_gummizelle_is_caged = flt_gummizelle_check_caged()) == 1) cf_hash_set(GlobalValues,"ShowInvisible",13,"1",1);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_gummizelle_view_init */
int flt_gummizelle_view_init(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  if(flt_gummizelle_is_caged) cf_hash_entry_delete(GlobalValues,"ShowInvisible",13);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_gummizelle_posting_filter */
int flt_gummizelle_posting_filter(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  if(flt_gummizelle_is_caged) cf_hash_entry_delete(GlobalValues,"ShowInvisible",13);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_gummizelle_undelete_subtree */
message_t *flt_gummizelle_undelete_subtree(message_t *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next) msg->invisible = 0;

  return msg;
}
/* }}} */

/* {{{ flt_gummizelle_execute */
int flt_gummizelle_execute(cf_hash_t *head,configuration_t *dc,configuration_t *vc,message_t *msg,u_int64_t tid,int mode) {
  int restore = 0;
  time_t now = time(NULL);

  cf_list_element_t *elem;
  cf_post_flag_t *flag;

  if(flt_gummizelle_is_caged == 0 || flt_gummizelle_entry == NULL || msg->invisible == 0) return FLT_DECLINE;
  if(flt_gummizelle_entry->start && now < flt_gummizelle_entry->start) return FLT_DECLINE;
  if(flt_gummizelle_entry->end && now > flt_gummizelle_entry->end) return FLT_DECLINE;

  if(flt_gummizelle_entry->tid) {
    if(flt_gummizelle_entry->tid != tid) return FLT_DECLINE;
    restore = 1;
  }
  else {
    if(flt_gummizelle_entry->name && flt_gummizelle_name) {
      if(cf_strcasecmp(msg->author.content,flt_gummizelle_name) == 0) restore = 1;
    }

    if(restore == 0 && flt_gummizelle_uname) {
      for(elem=msg->flags.elements;elem;elem=elem->next) {
        flag = (cf_post_flag_t *)elem->data;
        if(cf_strcmp(flag->name,"UserName") == 0) {
          if(cf_strcmp(flt_gummizelle_entry->uname,flag->val) == 0) restore = 1;
          break;
        }
      }
    }
  }

  msg->invisible = 0;
  flt_gummizelle_undelete_subtree(msg);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_gummizelle_newpost */
#ifdef CF_SHARED_MEM
int flt_gummizelle_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_gummizelle_newpost(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  int restore = 0;
  time_t now = time(NULL);
  u_char *uname = cf_hash_get(GlobalValues,"UserName",8);
  size_t i;
  flt_gummizelle_t *ent;

  if(flt_gummizelle_is_caged == 0 || flt_gummizelle_entry == NULL) {
    for(i=0;i<flt_gummizelle_entries.elements;++i) {
      ent = array_element_at(&flt_gummizelle_entries,i);

      if(flt_gummizelle_ip && ent->ip && cf_strcmp(ent->ip,flt_gummizelle_ip) == 0) {
        flt_gummizelle_entry = ent;
        break;
      }
      if(flt_gummizelle_uname && ent->uname && cf_strcmp(ent->uname,flt_gummizelle_uname) == 0) {
        flt_gummizelle_entry = ent;
        break;
      }
      if(ent->name && cf_strcmp(ent->name,p->author.content) == 0) {
        flt_gummizelle_entry = ent;
        break;
      }
    }

    if(flt_gummizelle_entry == NULL) return FLT_DECLINE;
  }

  if(flt_gummizelle_entry->start && now < flt_gummizelle_entry->start) return FLT_DECLINE;
  if(flt_gummizelle_entry->end && now > flt_gummizelle_entry->end) return FLT_DECLINE;

  /* filter works only for a specific thread */
  if(flt_gummizelle_entry->tid) return FLT_DECLINE;

  if(flt_gummizelle_entry->name) {
    if(cf_strcasecmp(p->author.content,flt_gummizelle_entry->name) == 0) {
      restore = 1;
      p->invisible = 1;
    }
  }

  if(restore == 0 && flt_gummizelle_uname) {
    if(cf_strcmp(flt_gummizelle_entry->uname,uname) == 0) p->invisible = 1;
  }

  return FLT_OK;
}
/* }}} */

/* {{{ flt_gummizelle_handle */
int flt_gummizelle_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_gummizelle_fn == NULL) flt_gummizelle_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_gummizelle_fn,context) != 0) return 0;

  if(flt_gummizelle_db) free(flt_gummizelle_db);
  flt_gummizelle_db = strdup(args[0]);

  return 0;
}
/* }}} */

conf_opt_t flt_gummizelle_config[] = {
  { "Gummizelle",  flt_gummizelle_handle,  CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_gummizelle_handlers[] = {
  { INIT_HANDLER,         flt_gummizelle_init },
  { VIEW_INIT_HANDLER,    flt_gummizelle_view_init },
  { VIEW_LIST_HANDLER,    flt_gummizelle_execute },
  { POSTING_HANDLER,      flt_gummizelle_posting_filter },
  { NEW_POST_HANDLER,     flt_gummizelle_newpost },
  { 0, NULL }
};

module_config_t flt_gummizelle = {
  MODULE_MAGIC_COOKIE,
  flt_gummizelle_config,
  flt_gummizelle_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
