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
 * $LastChangedDate: 2004-08-09 15:10:05 +0200 (Mon, 09 Aug 2004) $
 * $LastChangedRevision$
 * $LastChangedBy: ckruse $
 *
 */
/* }}} */

/* {{{ Includes */
#include "config.h"
#include "defines.h"

#include <db.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

#include <pthread.h>

#include "cf_pthread.h"

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
/* }}} */

#define CFFS_VERSION "0.1"

static u_char *BackupFile = NULL;
t_cf_mutex BackupMutex;

/* {{{ flt_failsafe_write */
void flt_failsafe_write(FILE *fd,u_int64_t tid,u_int64_t bmid,t_posting *p) {
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

  fwrite(&p->user.name_len,sizeof(p->user.name_len),1,fd);
  fwrite(p->user.name,1,p->user.name_len,fd);

  fwrite(&p->user.email_len,sizeof(p->user.email_len),1,fd);
  if(p->user.email) fwrite(p->user.email,1,p->user.email_len,fd);

  fwrite(&p->user.hp_len,sizeof(p->user.hp_len),1,fd);
  if(p->user.hp) fwrite(p->user.hp,1,p->user.hp_len,fd);

  fwrite(&p->user.img_len,sizeof(p->user.img_len),1,fd);
  if(p->user.img) fwrite(p->user.img,1,p->user.img_len,fd);

  fwrite(&p->user.ip_len,sizeof(p->user.ip_len),1,fd);
  fwrite(p->user.ip,1,p->user.ip_len,fd);

  fwrite(&p->unid_len,sizeof(p->unid_len),1,fd);
  fwrite(p->unid,1,p->unid_len,fd);

  fwrite(&p->subject_len,sizeof(p->subject_len),1,fd);
  fwrite(p->subject,1,p->subject_len,fd);

  fwrite(&p->category_len,sizeof(p->category_len),1,fd);
  if(p->category) fwrite(p->category,1,p->category_len,fd);

  fwrite(&p->content_len,sizeof(p->content_len),1,fd);
  fwrite(p->content,1,p->content_len,fd);

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
int flt_failsafe_thread_handler(t_configuration *dcfg,t_configuration *scfg,t_thread *t) {
  FILE *fd;

  /* lock backup mutex */
  CF_LM(&BackupMutex);

  if((fd = fopen(BackupFile,"ab")) != NULL) {
    flt_failsafe_write(fd,t->tid,0,t->postings);
    fclose(fd);

    /* Unlock the backup mutex */
    CF_UM(&BackupMutex);
    return FLT_OK;
  }

  /* unlock backup mutex */
  CF_UM(&BackupMutex);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_failsafe_post_handler */
int flt_failsafe_post_handler(t_configuration *dcfg,t_configuration *scfg,u_int64_t tid,t_posting *p) {
  FILE *fd;

  /* lock backup mutex */
  CF_LM(&BackupMutex);

  if((fd = fopen(BackupFile,"ab")) != NULL) {
    flt_failsafe_write(fd,tid,p->prev->mid,p);
    fclose(fd);

    /* Unlock the backup mutex */
    CF_UM(&BackupMutex);
    return FLT_OK;
  }

  /* unlock backup mutex */
  CF_UM(&BackupMutex);

  return FLT_DECLINE;

}
/* }}} */

/* {{{ flt_failsafe_init */
int flt_failsafe_init(int main_socket) {
  FILE *fd;
  struct stat st;

  cf_mutex_init("BackupMutex",&BackupMutex);

  if(stat(BackupFile,&st) == 0) {
    fprintf(stderr,"There is a backup file, perhaps you should run fo_recovery!\n");
    return FLT_EXIT;
  }

  if((fd = fopen(BackupFile,"wb")) == NULL) {
    fprintf(stderr,"Could not open BackupFile %s: %s\n",BackupFile,strerror(errno));
    return FLT_EXIT;
  }

  fwrite("CFFS",4,1,fd);
  fwrite(CFFS_VERSION,3,1,fd);
  fclose(fd);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_failsafe_handle_command */
int flt_failsafe_handle_command(t_configfile *cf,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  if(argnum == 1) {
    if(BackupFile) free(BackupFile);
    BackupFile = strdup(args[0]);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_failsafe: expecting one argument for directive BackupFile!\n");
  }

  return 0;
}
/* }}} */

/* {{{ flt_failsafe_cleanup */
void flt_failsafe_cleanup(void) {
  remove(BackupFile);
  free(BackupFile);
  cf_mutex_destroy(&BackupMutex);
}
/* }}} */

t_conf_opt flt_failsafe_config[] = {
  { "BackupFile", flt_failsafe_handle_command, CFG_OPT_CONFIG|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_failsafe_handlers[] = {
  { INIT_HANDLER,       flt_failsafe_init           },
  { NEW_POST_HANDLER,   flt_failsafe_post_handler   },
  { NEW_THREAD_HANDLER, flt_failsafe_thread_handler },
  { 0, NULL }
};

t_module_config flt_failsafe = {
  flt_failsafe_config,
  flt_failsafe_handlers,
  NULL,
  NULL,
  NULL,
  flt_failsafe_cleanup
};

/* eof */
