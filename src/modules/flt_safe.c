/**
 * \file flt_safe.c
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

static u_char *BackupFile = NULL;
t_cf_mutex BackupMutex;

/* {{{ flt_safe_thread_handler */
int flt_safe_thread_handler(t_configuration *dcfg,t_configuration *scfg,t_thread *t) {
  FILE *fd;
  t_posting *p;

  if(BackupFile == NULL) return FLT_DECLINE;

  /* lock backup mutex */
  CF_LM(&BackupMutex);

  if((fd = fopen(BackupFile,"ab")) != NULL) {
    /* write thread id */
    fwrite(&t->tid,sizeof(t->tid),1,fd);

    p = t->postings;

    /* write posting id */
    fwrite(&p->mid,sizeof(p->mid),1,fd);

    /*
     * write everything of the posting
     *
     * We use the byte representation of the data; this makes the
     * file not platform independent, but it is faster to write and
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

/* {{{ flt_safe_post_handler */
int flt_safe_post_handler(t_configuration *dcfg,t_configuration *scfg,u_int64_t tid,t_posting *p) {
  FILE *fd;

  if(BackupFile == NULL) return FLT_DECLINE;

  /* lock backup mutex */
  CF_LM(&BackupMutex);

  if((fd = fopen(BackupFile,"ab")) != NULL) {
    /* write thread id */
    fwrite(&tid,sizeof(tid),1,fd);

    /* write posting id */
    fwrite(&p->mid,sizeof(p->mid),1,fd);

    /*
     * write everything of the posting
     *
     * We use the byte representation of the data; this makes the
     * file not platform independent, but it is faster to write and
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

/* {{{ flt_safe_init */
int flt_safe_init(int main_socket) {
  cf_mutex_init("BackupMutex",&BackupMutex);
  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_safe_handle_command */
int flt_safe_handle_command(t_configfile *cf,t_conf_opt *opt,u_char **args,int argnum) {
  if(argnum == 1) {
    if(BackupFile) free(BackupFile);
    BackupFile = strdup(args[0]);
  }
  else {
    cf_log(LOG_ERR,__FILE__,__LINE__,"flt_safe: expecting one argument for directive BackupFile!\n");
  }

  return 0;
}
/* }}} */

/* {{{ flt_safe_cleanup */
void flt_safe_cleanup(void) {
  if(BackupFile) {
    free(BackupFile);
    cf_mutex_destroy(&BackupMutex);
  }
}
/* }}} */

t_conf_opt flt_safe_config[] = {
  { "BackupFile", flt_safe_handle_command, CFG_OPT_CONFIG|CFG_OPT_NEEDED, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_safe_handlers[] = {
  { INIT_HANDLER,       flt_safe_init           },
  { NEW_POST_HANDLER,   flt_safe_post_handler   },
  { NEW_THREAD_HANDLER, flt_safe_thread_handler },
  { 0, NULL }
};

t_module_config flt_safe = {
  flt_safe_config,
  flt_safe_handlers,
  NULL,
  NULL,
  NULL,
  flt_safe_cleanup
};

/* eof */
