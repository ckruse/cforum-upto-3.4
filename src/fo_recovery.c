/**
 * \file fo_recovery.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum recovery program
 */

/* {{{ Initial comment */
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
#include <time.h>
#include <pthread.h>
#include <gdome.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

/* socket includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "semaphores.h"
#endif

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "initfinish.h"
#include "serverlib.h"
#include "xml_handling.h"
#include "archiver.h"
/* }}} */

/* definition of the global variables; needed because of undefined references */
int    RUN;
t_head head;

#define CFFS_SUPPORTED_VERSION "0.1"

void read_posting_from_file(FILE *fd,t_posting *p) {
  t_posting n;
  int fr = 0;

  /* in this case we only want to have the filepointer to the next posting */
  if(p == NULL) {
    p = &n;
    fr = 1;
  }

  /* username */
  fread(&p->user.name_len,sizeof(p->user.name_len),1,fd);

  p->user.name = fo_alloc(NULL,p->user.name_len+1,1,FO_ALLOC_MALLOC);
  fread(p->user.name,p->user.name_len,1,fd);
  p->user.name[p->user.name_len] = '\0';


  /* email */
  fread(&p->user.email_len,sizeof(p->user.email_len),1,fd);

  if(p->user.email_len) {
    p->user.email = fo_alloc(NULL,p->user.email_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.email,p->user.email_len,1,fd);
    p->user.email[p->user.email_len] = '\0';
  }
  else p->user.email = NULL;

  /* homepage */
  fread(&p->user.hp_len,sizeof(p->user.hp_len),1,fd);
  if(p->user.hp_len) {
    p->user.hp = fo_alloc(NULL,p->user.hp_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.hp,p->user.hp_len,1,fd);
    p->user.hp[p->user.hp_len] = '\0';
  }
  else p->user.hp = NULL;

  /* image */
  fread(&p->user.img_len,sizeof(p->user.img_len),1,fd);
  if(p->user.img_len) {
    p->user.img = fo_alloc(NULL,p->user.img_len+1,1,FO_ALLOC_MALLOC);
    fread(p->user.img,p->user.img_len,1,fd);
    p->user.img[p->user.img_len] = '\0';
  }
  else p->user.img = NULL;


  /* ip */
  fread(&p->user.ip_len,sizeof(p->user.ip_len),1,fd);
  p->user.ip = fo_alloc(NULL,p->user.ip_len+1,1,FO_ALLOC_MALLOC);
  fread(p->user.ip,p->user.ip_len,1,fd);
  p->user.ip[p->user.ip_len] = '\0';

  /* unique id */
  fread(&p->unid_len,sizeof(p->unid_len),1,fd);
  p->unid = fo_alloc(NULL,p->unid_len+1,1,FO_ALLOC_MALLOC);
  fread(p->unid,p->unid_len,1,fd);
  p->unid[p->unid_len] = '\0';

  /* subject */
  fread(&p->subject_len,sizeof(p->subject_len),1,fd);
  p->subject = fo_alloc(NULL,p->subject_len+1,1,FO_ALLOC_MALLOC);
  fread(p->subject,p->subject_len,1,fd);
  p->subject[p->subject_len] = '\0';

  /* category */
  fread(&p->category_len,sizeof(p->category_len),1,fd);
  if(p->category_len) {
    p->category = fo_alloc(NULL,p->category_len+1,1,FO_ALLOC_MALLOC);
    fread(p->category,p->category_len,1,fd);
    p->category[p->category_len] = '\0';
  }
  else p->category = NULL;

  /* content */
  fread(&p->content_len,sizeof(p->content_len),1,fd);
  p->content = fo_alloc(NULL,p->content_len+1,1,FO_ALLOC_MALLOC);
  fread(p->content,p->content_len,1,fd);
  p->content[p->content_len] = '\0';

  /* date */
  fread(&p->date,sizeof(u_int32_t),1,fd);

  /* votes */
  fread(&p->votes_good,sizeof(p->votes_good),1,fd);
  fread(&p->votes_bad,sizeof(p->votes_bad),1,fd);

  /* level */
  fread(&p->level,sizeof(p->level),1,fd);

  /* visibility */
  fread(&p->invisible,sizeof(p->invisible),1,fd);

  /* ok, end of posting */
  if(fr) {
    if(p->user.email) free(p->user.email);
    if(p->user.hp) free(p->user.hp);
    if(p->user.img) free(p->user.img);
    if(p->category) free(p->category);

    free(p->user.name);
    free(p->user.ip);
    free(p->subject);
    free(p->content);
    free(p->unid);
  }
}

int main(int argc,char *argv[]) {
  FILE *fd;
  u_char *fname,buff[512];
  t_array *fnames;
  t_configfile default_conf;
  t_configfile server_conf;
  t_name_value *backup_file;
  t_thread *t;
  t_posting *p,*p1;
  u_int64_t int64_val;
  int n;

  t_name_value *sort_m_v;
  int sort_m;

  t_name_value  *mpath;
  GdomeDOMImplementation *impl = gdome_di_mkref();
  GdomeException e;
  GdomeDocument *doc           = xml_create_doc(impl,FORUM_DTD);
  GdomeElement *el             = gdome_doc_documentElement(doc,&e);

  static t_conf_opt config[] = {
    { "BackupFile", handle_command, CFG_OPT_CONFIG|CFG_OPT_NEEDED, &fo_server_conf },
    { NULL, NULL, 0, NULL }
  };

  static const u_char *wanted[] = {
    "fo_default", "fo_server"
  };

  /* {{{ do initialization */
  cfg_init();
  init_modules();

  /* initialisation of the head-variable */
  str_init(&head.cache_invisible);
  str_init(&head.cache_visible);

  head.date_visible      = 0;
  head.date_invisible    = 0;

  head.thread            = NULL;

  head.std               = NULL;
  head.err               = NULL;

  head.locked            = 0;

  head.clients.workers   = 0;
  head.clients.clientnum = 0;
  head.clients.down      = 0;
  head.clients.clients   = NULL;
  head.clients.last      = NULL;

  head.servers           = NULL;

  head.protocol_handlers = NULL;

  head.threads           = NULL;
  head.unique_ids        = cf_hash_new(NULL);

  #ifdef CF_SHARED_MEM
  head.shm_ids[0]        = -1;
  head.shm_ids[1]        = -1;

  head.shm_ptrs[0]       = NULL;
  head.shm_ptrs[1]       = NULL;

  head.shm_sem           = -1;
  #endif

  RUN                    = 1;

  cf_rwlock_init("head.lock",&head.lock);
  cf_rwlock_init("head.threads_lock",&head.threads_lock);

  pthread_mutex_init(&head.log_lock,NULL);

  cf_mutex_init("head.clients.cond_lock",&head.clients.cond_lock);
  cf_mutex_init("head.clients.lock",&head.clients.lock);
  cf_mutex_init("head.server_lock",&head.server_lock);
  cf_mutex_init("head.unique_ids_mutex",&head.unique_ids_mutex);

  pthread_cond_init(&head.clients.cond,NULL);

  /* }}} */

  /* {{{ configuration */
  if((fnames = get_conf_file(wanted,2)) == NULL) {
    return EXIT_FAILURE;
  }

  fname = *((u_char **)array_element_at(fnames,0));
  cfg_init_file(&default_conf,fname);
  cfg_register_options(&default_conf,default_options);
  free(fname);

  fname = *((u_char **)array_element_at(fnames,1));
  cfg_init_file(&server_conf,fname);
  cfg_register_options(&server_conf,fo_server_options);
  cfg_register_options(&server_conf,config);
  free(fname);

  array_destroy(fnames);
  free(fnames);

  if(read_config(&default_conf,NULL,CFG_MODE_CONFIG) != 0) {
    cfg_cleanup_file(&default_conf);
    cfg_cleanup_file(&server_conf);

    fprintf(stderr,"configfile error!\n");
    return EXIT_FAILURE;
  }

  if(read_config(&server_conf,NULL,CFG_MODE_CONFIG) != 0) {
    cfg_cleanup_file(&default_conf);
    cfg_cleanup_file(&server_conf);

    fprintf(stderr,"configfile error!\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ more initialization */
  sort_m_v = cfg_get_first_value(&fo_server_conf,"SortMessages");
  sort_m   = atoi(sort_m_v->values[0]);
  mpath    = cfg_get_first_value(&fo_default_conf,"MessagePath");

  #ifdef CF_SHARED_MEM
  cf_mutex_init("head.shm_lock",&head.shm_lock);
  #endif

  /* create the forum tree */
  make_forumtree(&fo_default_conf,&head);
  /* }}} */


  /* ok, first lets read the backup file */
  if((backup_file = cfg_get_first_value(&fo_server_conf,"BackupFile")) == NULL) {
    fprintf(stderr,"Sorry, could not find backup file\n");
    exit(-1);
  }

  if((fd = fopen(backup_file->values[0],"rb")) == NULL) {
    fprintf(stderr,"Sorry, could not open backup file: %s\n",strerror(errno));
    exit(-1);
  }

  /* first 4 bytes are for file identification, the following 3 bytes are version information */
  fread(buff,1,7,fd);
  buff[7] = '\0';

  if(cf_strncmp(buff,"CFFS",4)) {
    fclose(fd);
    fprintf(stderr,"Hey, backup file seems not to be a valid Classic Forum backup file\n");
    exit(-1);
  }

  if(cf_strcmp(buff+4,CFFS_SUPPORTED_VERSION)) {
    fclose(fd);
    fprintf(stderr,"Hey, we support version %s, but we got version %s\n",CFFS_SUPPORTED_VERSION,buff+4);
    exit(-1);
  }

  while(!feof(fd)) {
    n = 0;

    /* read thread id */
    if(fread(&int64_val,sizeof(int64_val),1,fd) <= 0) break;

    /* uh, oh, we got a new thread, the crash was nasty */
    if((t  = cf_get_thread(int64_val)) == NULL) {
      t = fo_alloc(NULL,1,sizeof(*t),FO_ALLOC_CALLOC);
      t->tid = int64_val;
      n = 1;
    }

    /* read the message id */
    fread(&int64_val,sizeof(int64_val),1,fd);

    /* ok, we already got this posting, fine -- next one, please! */
    if((p1 = cf_get_posting(t,int64_val)) != NULL) {
      if(n) free(t);
      fread(&int64_val,sizeof(int64_val),1,fd);
      read_posting_from_file(fd,NULL);
      continue;
    }
    else {
      p = fo_alloc(NULL,sizeof(*p),1,FO_ALLOC_CALLOC);
      p->mid = int64_val;
    }

    /* read the previous posting id */
    fread(&int64_val,sizeof(int64_val),1,fd);

    read_posting_from_file(fd,p);
    if(t->postings) {
      p1 = cf_get_posting(t,int64_val);

      if(sort_m == 2 || p1->next == NULL) {
        p->next       = p1->next;
        p1->next      = p;
        p->prev       = p1;

        if(p->next) p->next->prev = p;
        else        t->last = p;
      }
      else {
        for(p1=p1->next;p1->next && p1->level == p->level;p1=p1->next);
        p->next       = p1->next;
        p1->next      = p;
        p->prev       = p1;

        if(p->next) p->next->prev = p;
        else        t->last       = p;
      }
    }
    else t->postings = p;

    if(n) {
      t->next = head.thread;
      head.thread->prev = t;
      head.thread = t;
    }

    if(head.mid <= p->mid) head.mid = p->mid + 1;
    if(head.tid <= t->tid) head.tid = t->tid + 1;
  }

  fclose(fd);

  /* ok, we read everything, now write threads to disk */

  /* set lastThread and lastMessage */
  sprintf(buff,"m%lld",head.mid);
  xml_set_attribute(el,"lastMessage",buff);

  sprintf(buff,"t%lld",head.tid);
  xml_set_attribute(el,"lastThread",buff);

  gdome_el_unref(el,&e);

  t = head.thread;
  while(t) {
    /* we need no locking in the child process */
    stringify_thread_and_write_to_disk(doc,t);
    t = t->next;
  }

  snprintf(buff,256,"%s/forum.xml",mpath->values[0]);

  if(!gdome_di_saveDocToFile(impl,doc,buff,0,&e)) {
    fprintf(stderr,"ERROR! COULD NOT WRITE XML FILE!\n");
    exit(-1);
  }
  gdome_doc_unref(doc,&e);

  printf("Ok, safely recovered all backuped data, now you can do a »rm %s« and start the server\n",backup_file->values[0]);

  /* cleanup the config */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_server_conf);

  cfg_cleanup_file(&default_conf);
  cfg_cleanup_file(&server_conf);

  return 0;
}


/* eof */
