/**
 * \file fo_recovery.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* for sockets */
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>

#include <signal.h>
#include <pthread.h>
#include <getopt.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "semaphores.h"
#endif

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "readline.h"

#include "cf_pthread.h"

#include "serverutils.h"
#include "serverlib.h"
#include "archiver.h"
#include "fo_server.h"
/* }}} */

/* definition of the global variables; needed because of undefined references */
int    RUN;
head_t head;

#define CFFS_SUPPORTED_VERSION "0.2"

/* {{{ read_posting_from_file */
void read_posting_from_file(FILE *fd,cf_posting_t *p) {
  cf_posting_t n;
  int fr = 0;
  u_int32_t flagnum = 0,i,len;

  cf_posting_flag_t flag;

  /* in this case we only want to have the filepointer to the next posting */
  if(p == NULL) {
    p = &n;
    fr = 1;
  }

  cf_list_init(&p->flags);

  /* read flags */
  fread(&flagnum,sizeof(flagnum),1,fd);
  for(i=0;i<flagnum;++i) {
    fread(&len,sizeof(len),1,fd);
    flag.name = cf_alloc(NULL,1,len+1,CF_ALLOC_MALLOC);
    fread(flag.name,len,1,fd);
    flag.name[len] = '\0';

    fread(&len,sizeof(len),1,fd);
    flag.val = cf_alloc(NULL,1,len+1,CF_ALLOC_MALLOC);
    fread(flag.val,len,1,fd);
    flag.val[len] = '\0';

    cf_list_append(&p->flags,&flag,sizeof(flag));
  }


  /* username */
  fread(&p->user.name.len,sizeof(p->user.name.len),1,fd);

  p->user.name.content = cf_alloc(NULL,p->user.name.len+1,1,CF_ALLOC_MALLOC);
  fread(p->user.name.content,p->user.name.len,1,fd);
  p->user.name.content[p->user.name.len] = '\0';
  p->user.name.reserved = p->user.name.len + 1;


  /* email */
  fread(&p->user.email.len,sizeof(p->user.email.len),1,fd);

  if(p->user.email.len) {
    p->user.email.content = cf_alloc(NULL,p->user.email.len+1,1,CF_ALLOC_MALLOC);
    fread(p->user.email.content,p->user.email.len,1,fd);
    p->user.email.content[p->user.email.len] = '\0';
    p->user.email.reserved = p->user.email.len + 1;
  }
  else memset(&p->user.email,0,sizeof(p->user.email));

  /* homepage */
  fread(&p->user.hp.len,sizeof(p->user.hp.len),1,fd);
  if(p->user.hp.len) {
    p->user.hp.content = cf_alloc(NULL,p->user.hp.len+1,1,CF_ALLOC_MALLOC);
    fread(p->user.hp.content,p->user.hp.len,1,fd);
    p->user.hp.content[p->user.hp.len] = '\0';
    p->user.hp.reserved = p->user.hp.len + 1;
  }
  else memset(&p->user.hp,0,sizeof(p->user.hp));

  /* image */
  fread(&p->user.img.len,sizeof(p->user.img.len),1,fd);
  if(p->user.img.len) {
    p->user.img.content = cf_alloc(NULL,p->user.img.len+1,1,CF_ALLOC_MALLOC);
    fread(p->user.img.content,p->user.img.len,1,fd);
    p->user.img.content[p->user.img.len] = '\0';
    p->user.img.reserved = p->user.img.len + 1;
  }
  else memset(&p->user.img,0,sizeof(p->user.img));


  /* ip */
  fread(&p->user.ip.len,sizeof(p->user.ip.len),1,fd);
  p->user.ip.content = cf_alloc(NULL,p->user.ip.len+1,1,CF_ALLOC_MALLOC);
  fread(p->user.ip.content,p->user.ip.len,1,fd);
  p->user.ip.content[p->user.ip.len] = '\0';
  p->user.ip.reserved = p->user.ip.len + 1;

  /* unique id */
  fread(&p->unid.len,sizeof(p->unid.len),1,fd);
  p->unid.content = cf_alloc(NULL,p->unid.len+1,1,CF_ALLOC_MALLOC);
  fread(p->unid.content,p->unid.len,1,fd);
  p->unid.content[p->unid.len] = '\0';
  p->unid.reserved = p->unid.len + 1;

  /* subject */
  fread(&p->subject.len,sizeof(p->subject.len),1,fd);
  p->subject.content = cf_alloc(NULL,p->subject.len+1,1,CF_ALLOC_MALLOC);
  fread(p->subject.content,p->subject.len,1,fd);
  p->subject.content[p->subject.len] = '\0';
  p->subject.reserved = p->subject.len + 1;

  /* category */
  fread(&p->category.len,sizeof(p->category.len),1,fd);
  if(p->category.len) {
    p->category.content = cf_alloc(NULL,p->category.len+1,1,CF_ALLOC_MALLOC);
    fread(p->category.content,p->category.len,1,fd);
    p->category.content[p->category.len] = '\0';
    p->category.reserved = p->category.len + 1;
  }
  else memset(&p->category,0,sizeof(p->category));

  /* content */
  fread(&p->content.len,sizeof(p->content.len),1,fd);
  p->content.content = cf_alloc(NULL,p->content.len+1,1,CF_ALLOC_MALLOC);
  fread(p->content.content,p->content.len,1,fd);
  p->content.content[p->content.len] = '\0';
  p->content.reserved = p->content.len + 1;

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
  if(fr) cf_cleanup_posting(p);
}
/* }}} */

static struct option cmdline_options[] = {
  { "config-directory", 1, NULL, 'c' },
  { "forum-name",       1, NULL, 'f' },
  { "backup-file",      1, NULL, 'b' },
  { NULL,               0, NULL, 0   }
};

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] cf-recovery [options]\n\n" \
    "where options are:\n" \
    "\t-c, --config-directory  Path to the configuration directory\n" \
    "\t-f, --forum-name        Name of the forum to index\n" \
    "\t-b, --backup-file       Path to the backup file\n" \
    "\t-h, --help              Show this help screen\n\n" \
    "One of both must be set: config-directory option or CF_CONF_DIR\n" \
    "environment variable. We suggest to make a backup of the message files before running\n" \
    "cf-recovery!\n\n"
  );
  exit(-1);
}
/* }}} */

head_t head;
int RUN = 1;

int main(int argc,char *argv[]) {
  u_char *forum_name = NULL,*bf = NULL,buff[512];
  char c;

  FILE *fd;

  int n,sort_m;
  u_int64_t int64_val;
  cf_thread_t *t;
  cf_posting_t *p,*p1,*p2;

  cf_forum_t *forum;

  cf_cfg_config_value_t *v,*sort_m_v;

  cf_cfg_config_t cfg;

  struct stat st;

  static const u_char *wanted[] = {
    "fo_server","fo_default"
  };

  /* {{{ initialize  variables */
  cf_rwlock_init("head.lock",&head.lock);
  cf_rw_list_init("head.workers",&head.workers.list);

  pthread_mutex_init(&head.log.lock,NULL);
  head.log.std = NULL;
  head.log.err = NULL;

  head.forums  = cf_hash_new(NULL);

  head.workers.num = 0;
  head.clients.num = 0;

  cf_list_init(&head.clients.list);
  cf_cond_init("head.clients.cond",&head.clients.cond,NULL);
  cf_mutex_init("head.clients.lock",&head.clients.lock);

  cf_mutex_init("head.servers.lock",&head.servers.lock);
  cf_list_init(&head.servers.list);

  head.protocol_handlers = cf_hash_new(NULL);
  /* }}} */

  /* {{{ read options from commandline */
  while((c = getopt_long(argc,argv,"c:f:b:",cmdline_options,NULL)) > 0) {
    switch(c) {
      case 'c':
        if(!optarg) {
          fprintf(stderr,"no configuration path given for argument --config-directory\n");
          usage();
        }
        setenv("CF_CONF_DIR",optarg,1);
        break;

      case 'f':
        if(!optarg) {
          fprintf(stderr,"no forum name given for argument --forum-name\n");
          usage();
        }
        forum_name = strdup(optarg);
        break;

      case 'b':
        if(!optarg) {
          fprintf(stderr,"no backup file path given for argument --backup-file\n");
          usage();
        }
        bf = strdup(optarg);
        break;

      default:
        fprintf(stderr,"unknown option: %d\n",c);
        usage();
    }
  }
  /* }}} */

  if(forum_name == NULL) {
    fprintf(stderr,"forum name not given!\n");
    usage();
  }

  /* read config */
  if(cf_cfg_get_conf(&cfg,wanted,2) != 0) {
    fprintf(stderr,"config file error!\n");
    return EXIT_FAILURE;
  }

  /* {{{ check if backup file exists */
  if(bf == NULL) {
    v = cf_cfg_get_value(&cfg,"BackupFile");
    bf = strdup(v->sval);
  }

  if(stat(bf,&st) == -1) {
    fprintf(stderr,"Could not find backup file, maybe there is nothing to recover?\n");
    exit(-1);
  }
  /* }}} */

  /* {{{ register forum and load forum data */
  if((forum = cf_register_forum(&cfg,forum_name)) == NULL) {
    fprintf(stderr,"could not register forum %s!\n",forum_name);
    return EXIT_FAILURE;
  }

  if(cf_load_data(&cfg,forum) == -1) {
    fprintf(stderr,"could not load forum data for forum %s\n",forum_name);
    return EXIT_FAILURE;
  }
  /* }}} */

  sort_m_v = cf_cfg_get_value(&cfg,"SortMessages");
  sort_m = cf_strcmp(sort_m_v->sval,"ascending") == 0 ? CF_SORT_ASCENDING : CF_SORT_DESCENDING;

  /* open recovery file */
  if((fd = fopen(bf,"rb")) == NULL) {
    fprintf(stderr,"Sorry, could not open backup file: %s\n",strerror(errno));
    exit(-1);
  }

  /* {{{ check for file version; first 4 bytes are for file identification, the following 3 bytes are version information */
  fread(buff,1,7,fd);
  buff[7] = '\0';

  if(cf_strncmp(buff,"CFFS",4)) {
    fclose(fd);
    fprintf(stderr,"Backup file seems not to be a valid Classic Forum backup file\n");
    exit(-1);
  }

  if(cf_strcmp(buff+4,CFFS_SUPPORTED_VERSION)) {
    fclose(fd);
    fprintf(stderr,"We support version %s, but we got version %s\n",CFFS_SUPPORTED_VERSION,buff+4);
    exit(-1);
  }
  /* }}} */

  while(!feof(fd)) {
    n = 0;

    /* read thread id */
    if(fread(&int64_val,sizeof(int64_val),1,fd) <= 0) break;

    if((t  = cf_get_thread(&cfg,forum,int64_val)) == NULL) {
      t = cf_alloc(NULL,1,sizeof(*t),CF_ALLOC_CALLOC);
      t->tid = int64_val;
      n = 1;
    }

    /* read the message id */
    fread(&int64_val,sizeof(int64_val),1,fd);

    /* ok, we already got this posting, fine -- next one, please! */
    if((p1 = cf_get_posting(&cfg,t,int64_val)) != NULL) {
      if(n) free(t);
      fread(&int64_val,sizeof(int64_val),1,fd);
      read_posting_from_file(fd,NULL);
      continue;
    }
    else {
      p = cf_alloc(NULL,sizeof(*p),1,CF_ALLOC_CALLOC);
      p->mid = int64_val;
    }

    /* read the previous posting id */
    fread(&int64_val,sizeof(int64_val),1,fd);
    read_posting_from_file(fd,p);

    if(t->postings) {
      p1 = cf_get_posting(&cfg,t,int64_val);

      if(sort_m == CF_SORT_DESCENDING || p1->next == NULL) {
        p->next       = p1->next;
        p1->next      = p;
        p->prev       = p1;

        if(p->next) p->next->prev = p;
        else        t->last = p;
      }
      else {
        /* let's find the end of the subtree */
        for(p2=p1->next;p2->next && p2->level > p1->level;p2=p2->next);
        if(p2->level > p1->level) {
          p2->next = p;
          p->prev = p2;
        }
        else {
          p->next = p2;
          p->prev = p2->prev;
          p2->prev->next = p;
          p2->prev = p;
        }
      }
    }

    if(n) {
      t->next = forum->threads.list;
      forum->threads.list->prev = t;
      forum->threads.list = t;
    }

    if(forum->threads.last_mid < p->mid) forum->threads.last_mid = p->mid;
    if(forum->threads.last_tid < t->tid) forum->threads.last_tid = t->tid;
  }

  fclose(fd);

  /* ok, we read everything, now write threads to disk */
  cf_write_threadlist(&cfg,forum);

  return EXIT_SUCCESS;
}


/* eof */
