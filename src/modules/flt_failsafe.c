/**
 * \file flt_failsafe.c
 * \author Christian Kruse
 *
 * This plugin writes every new posting in an special format to
 * the hard disc. In case of a crash the data can be restored from
 * this file by a special script.
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

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

#define CFFS_VERSION "0.2"

typedef struct {
  u_char *BackupFile;
  t_cf_mutex BackupMutex;
} t_cf_failsafe;

static t_cf_hash *flt_failsafe_hsh = NULL;
static int flt_failsafe_error = 0;

void flt_failsafe_cleanup_hash(void *data) {
  t_cf_failsafe *fl = (t_cf_failsafe *)data;

  if(flt_failsafe_error == 0) remove(fl->BackupFile);
  free(fl->BackupFile);
  cf_mutex_destroy(&fl->BackupMutex);
}

void flt_failsafe_cleaner(t_forum *forum) {
  FILE *fd;
  t_cf_failsafe *fl = cf_hash_get(flt_failsafe_hsh,forum->name,strlen(forum->name));

  /* lock backup mutex */
  CF_LM(&fl->BackupMutex);

  if((fd = fopen(fl->BackupFile,"wb")) != NULL) {
    fwrite("CFFS",4,1,fd);
    fwrite(CFFS_VERSION,3,1,fd);
    fclose(fd);
  }

  CF_UM(&fl->BackupMutex);
}

/* {{{ flt_failsafe_write */
void flt_failsafe_write(t_forum *forum,FILE *fd,u_int64_t tid,u_int64_t bmid,t_posting *p) {
  t_cf_list_element *elem;
  t_posting_flag *flag;
  u_int32_t flagnum;

  /* write thread id */
  fwrite(&tid,sizeof(tid),1,fd);

  /* write posting id */
  fwrite(&p->mid,sizeof(p->mid),1,fd);

  /* write previous message id */
  fwrite(&bmid,sizeof(bmid),1,fd);

  /*
   * write everything of the posting
   *
   * We use the byte representation of the data; this makes the
   * file platform dependent, but it is faster to write and
   * it is faster to read. And there is no need to make this backup
   * file platform independent
   */
  for(flagnum=0,elem=p->flags.elements;elem;elem=elem->next,++flagnum);
  fwrite(&flagnum,sizeof(flagnum),1,fd);

  for(elem=p->flags.elements;elem;elem=elem->next) {
    flag = (t_posting_flag *)elem->data;

    flagnum = strlen(flag->name);
    fwrite(&flagnum,sizeof(flagnum),1,fd);
    fwrite(flag->name,1,flagnum,fd);

    flagnum = strlen(flag->val);
    fwrite(&flagnum,sizeof(flagnum),1,fd);
    fwrite(flag->val,1,flagnum,fd);
  }

  fwrite(&p->user.name.len,sizeof(p->user.name.len),1,fd);
  fwrite(p->user.name.content,1,p->user.name.len,fd);

  fwrite(&p->user.email.len,sizeof(p->user.email.len),1,fd);
  if(p->user.email.len) fwrite(p->user.email.content,1,p->user.email.len,fd);

  fwrite(&p->user.hp.len,sizeof(p->user.hp.len),1,fd);
  if(p->user.hp.len) fwrite(p->user.hp.content,1,p->user.hp.len,fd);

  fwrite(&p->user.img.len,sizeof(p->user.img.len),1,fd);
  if(p->user.img.len) fwrite(p->user.img.content,1,p->user.img.len,fd);

  fwrite(&p->user.ip.len,sizeof(p->user.ip.len),1,fd);
  fwrite(p->user.ip.content,1,p->user.ip.len,fd);

  fwrite(&p->unid.len,sizeof(p->unid.len),1,fd);
  fwrite(p->unid.content,1,p->unid.len,fd);

  fwrite(&p->subject.len,sizeof(p->subject.len),1,fd);
  fwrite(p->subject.content,1,p->subject.len,fd);

  fwrite(&p->category.len,sizeof(p->category.len),1,fd);
  if(p->category.len) fwrite(p->category.content,1,p->category.len,fd);

  fwrite(&p->content.len,sizeof(p->content.len),1,fd);
  fwrite(p->content.content,1,p->content.len,fd);

  /* write date */
  fwrite(&p->date,sizeof(u_int32_t),1,fd);

  /* write votes */
  fwrite(&p->votes_good,sizeof(p->votes_good),1,fd);
  fwrite(&p->votes_bad,sizeof(p->votes_bad),1,fd);

  /* write level */
  fwrite(&p->level,sizeof(p->level),1,fd);

  /* visibility */
  fwrite(&p->invisible,sizeof(p->invisible),1,fd);
}
/* }}} */

/* {{{ flt_failsafe_thread_handler */
int flt_failsafe_thread_handler(t_forum *forum,t_thread *t) {
  FILE *fd;
  t_cf_failsafe *fl = cf_hash_get(flt_failsafe_hsh,forum->name,strlen(forum->name));

  /* lock backup mutex */
  CF_LM(&fl->BackupMutex);

  if((fd = fopen(fl->BackupFile,"ab")) != NULL) {
    flt_failsafe_write(forum,fd,t->tid,0,t->postings);
    fclose(fd);

    /* Unlock the backup mutex */
    CF_UM(&fl->BackupMutex);
    return FLT_OK;
  }

  /* unlock backup mutex */
  CF_UM(&fl->BackupMutex);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_failsafe_post_handler */
int flt_failsafe_post_handler(t_forum *forum,u_int64_t tid,t_posting *p) {
  FILE *fd;
  t_cf_failsafe *fl = cf_hash_get(flt_failsafe_hsh,forum->name,strlen(forum->name));

  /* lock backup mutex */
  CF_LM(&fl->BackupMutex);

  if((fd = fopen(fl->BackupFile,"ab")) != NULL) {
    flt_failsafe_write(forum,fd,tid,p->prev->mid,p);
    fclose(fd);

    /* Unlock the backup mutex */
    CF_UM(&fl->BackupMutex);
    return FLT_OK;
  }

  /* unlock backup mutex */
  CF_UM(&fl->BackupMutex);

  return FLT_DECLINE;

}
/* }}} */

/* {{{ flt_failsafe_init */
int flt_failsafe_init(int main_socket) {
  FILE *fd;
  struct stat st;
  t_name_value *forums = cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  size_t i;
  t_cf_failsafe *fl;

  for(i=0;i<forums->valnum;++i) {
    fl = cf_hash_get(flt_failsafe_hsh,forums->values[i],strlen(forums->values[i]));

    if(stat(fl->BackupFile,&st) == 0) {
      fprintf(stderr,"flt_failsafe: There is a backup file, perhaps you should run fo_recovery!\n");
      flt_failsafe_error = 1;
      return FLT_EXIT;
    }

    if((fd = fopen(fl->BackupFile,"wb")) == NULL) {
      fprintf(stderr,"flt_failsafe: Could not open BackupFile %s: %s\n",fl->BackupFile,strerror(errno));
      flt_failsafe_error = 1;
      return FLT_EXIT;
    }

    fwrite("CFFS",4,1,fd);
    fwrite(CFFS_VERSION,3,1,fd);
    fclose(fd);
  }

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_failsafe_handle_command */
int flt_failsafe_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  t_cf_failsafe *fl,fl1;
  u_char buff[512];

  if(flt_failsafe_hsh == NULL) flt_failsafe_hsh = cf_hash_new(flt_failsafe_cleanup_hash);

  if(argnum == 1) {
    if((fl = cf_hash_get(flt_failsafe_hsh,(u_char *)context,strlen(context))) == NULL) {
      fl1.BackupFile = strdup(args[0]);

      snprintf(buff,512,"BackupMutex_%s",context);
      cf_mutex_init(buff,&fl1.BackupMutex);

      cf_hash_set(flt_failsafe_hsh,(u_char *)context,strlen(context),&fl1,sizeof(fl1));
    }
    else {
      free(fl->BackupFile);
      fl->BackupFile = strdup(args[0]);
    }
  }
  else {
    cf_log(CF_ERR,__FILE__,__LINE__,"flt_failsafe: expecting one argument for directive BackupFile!\n");
  }

  return 0;
}
/* }}} */

/* {{{ flt_failsafe_cleanup */
void flt_failsafe_cleanup(void) {
  if(flt_failsafe_hsh) cf_hash_destroy(flt_failsafe_hsh);
}
/* }}} */

t_conf_opt flt_failsafe_config[] = {
  { "BackupFile", flt_failsafe_handle_command, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_failsafe_handlers[] = {
  { INIT_HANDLER,       flt_failsafe_init           },
  { NEW_POST_HANDLER,   flt_failsafe_post_handler   },
  { NEW_THREAD_HANDLER, flt_failsafe_thread_handler },
  { THRDLST_WRITTEN_HANDLER, flt_failsafe_cleaner },
  { 0, NULL }
};

t_module_config flt_failsafe = {
  MODULE_MAGIC_COOKIE,
  flt_failsafe_config,
  flt_failsafe_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_failsafe_cleanup
};

/* eof */
