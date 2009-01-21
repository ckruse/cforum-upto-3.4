/**
 * \file serverlib.c
 * \author Christian Kruse
 *
 * Contains all server library functions
 */

/* {{{ Initial comment */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 */
/* }}} */

/* {{{ includes */
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

#include <stdarg.h>

#include <inttypes.h>

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
#include "fo_server.h"
#include "archiver.h"
/* }}} */


/* {{{ cf_setup_shared_mem */
#ifdef CF_SHARED_MEM
void cf_setup_shared_mem(cf_cfg_config_t *cfg,cf_forum_t *forum) {
  union semun smn;
  unsigned short x = 0;
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"DF:SharedMemIds");

  if((forum->shm.sem = semget(v->avals[2].ival,1,S_IRWXU|S_IRWXG|S_IRWXO|IPC_CREAT)) == -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"semget: %s\n",strerror(errno));
    exit(-1);
  }

  smn.array = &x;
  if(semctl(forum->shm.sem,0,SETALL,smn) == -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));
    exit(-1);
  }

  /* check if there are shared memory segments and, if there are, report an error */
  if(shmget(v->avals[0].ival,0,0) != -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"The shared memory segments for forum %s already exist! This can be because of a crash, but this can also be because of a running server with this ids. We exit, please repair!\n",forum->name);
    exit(-1);
  }

  if(shmget(v->avals[1].ival,0,0) != -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"The shared memory segments for forum %s already exist! This can be because of a crash, but this can also be because of a running server with this ids. We exit, please repair!\n",forum->name);
    exit(-1);
  }
}
#endif
/* }}} */

/* {{{ cf_register_forum */
cf_forum_t *cf_register_forum(cf_cfg_config_t *cfg,const u_char *name) {
  cf_forum_t *forum = cf_alloc(NULL,1,sizeof(*forum),CF_ALLOC_MALLOC);

  forum->name = strdup(name);
  forum->cache.fresh = forum->locked = 0;

  cf_str_init(&forum->cache.visible);
  cf_str_init(&forum->cache.invisible);

  forum->date.visible = forum->date.invisible = 0;

  #ifdef CF_SHARED_MEM
  forum->shm.ids[0] = -1;
  forum->shm.ids[1] = -1;
  forum->shm.sem    = 0;

  forum->shm.ptrs[0] = NULL;
  forum->shm.ptrs[1] = NULL;

  cf_mutex_init("forum.shm.lock",&forum->shm.lock);

  cf_setup_shared_mem(cfg,forum);
  #endif

  cf_rwlock_init("forum.lock",&forum->lock);

  forum->threads.last_tid = forum->threads.last_mid = 0;
  forum->threads.threads  = cf_hash_new(NULL);
  forum->threads.list     = NULL;
  forum->threads.last     = NULL;

  cf_rwlock_init("forum.threads.lock",&forum->threads.lock);

  forum->uniques.ids = cf_hash_new(NULL);
  cf_mutex_init("forum.uniques.lock",&forum->uniques.lock);

  cf_hash_set_static(head.forums,(u_char *)name,strlen(name),forum);
  return forum;
}
/* }}} */

/* {{{ cf_destroy_forum */
void cf_destroy_forum(cf_cfg_config_t *cfg,cf_forum_t *forum) {
  #ifdef CF_SHARED_MEM
  int i;
  #endif

  cf_rwlock_destroy(&forum->lock);
  cf_rwlock_destroy(&forum->threads.lock);
  cf_mutex_destroy(&forum->uniques.lock);

  free(forum->name);
  cf_str_cleanup(&forum->cache.visible);
  cf_str_cleanup(&forum->cache.invisible);

  #ifdef CF_SHARED_MEM
  cf_mutex_destroy(&forum->shm.lock);

  if(forum->shm.sem >= 0) if(semctl(forum->shm.sem,0,IPC_RMID,NULL) == -1) cf_log(cfg,CF_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));

  for(i=0;i<2;++i) {
    if(forum->shm.ids[i] >= 0) {
      cf_log(cfg,CF_DBG,__FILE__,__LINE__,"removing shm segment %d\n",forum->shm.ids[i]);

      if(forum->shm.ptrs[i])
        if(shmdt(forum->shm.ptrs[i]) < 0) cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmdt: %s\n",strerror(errno));

      if(shmctl(forum->shm.ids[i],IPC_RMID,0) < 0) cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
    }
  }
  #endif

  cf_hash_destroy(forum->threads.threads);
  cf_hash_destroy(forum->uniques.ids);

  if(forum->threads.list) cf_cleanup_forumtree(forum);
}
/* }}} */

/* {{{ cf_log */
void cf_log(cf_cfg_config_t *cfg,int mode,const u_char *file,unsigned int line,const u_char *format, ...) {
  u_char str[300];
  int status;
  cf_cfg_config_value_t *v;
  time_t t;
  struct tm *tm;
  int sz;
  va_list ap;
  register u_char *ptr,*ptr1;

  #ifndef DEBUG
  if(mode & CF_DBG) return;
  #endif

  for(ptr1=ptr=(u_char *)file;*ptr;ptr++) {
    if(*ptr == '/') ptr1 = ptr + 1;
  }

  t  = time(NULL);
  tm = localtime(&t);

  if((status = pthread_mutex_lock(&head.log.lock)) != 0) {
    fprintf(head.log.err,"pthread_mutex_lock: %s\n",strerror(status));
    return;
  }

  if(!head.log.std) {
    v = cf_cfg_get_value(cfg,"StdLog");
    head.log.std = fopen(v->sval,"a");
  }
  if(!head.log.err) {
    v = cf_cfg_get_value(cfg,"ErrorLog");
    head.log.err = fopen(v->sval,"a");
  }

  sz = snprintf(str,300,"[%4d-%02d-%02d %02d:%02d:%02d]:%s:%d ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ptr1,line);

  va_start(ap, format);
  if(mode & CF_ERR) {
    #ifndef DEBUG
    fwrite(str,sz,1,head.log.err);
    vfprintf(head.log.err,format,ap);

    if(mode & CF_FLSH) fflush(head.log.err);
    #endif

    #ifdef DEBUG
    fwrite(str,sz,1,stderr);
    vfprintf(stderr,format,ap);
    fflush(stderr);
    #endif
  }
  else if(mode & CF_STD) {
    #ifndef DEBUG
    fwrite(str,sz,1,head.log.std);
    vfprintf(head.log.std,format,ap);

    if(mode & CF_FLSH) fflush(head.log.std);
    #endif

    #ifdef DEBUG
    fwrite(str,sz,1,stdout);
    vfprintf(stdout,format,ap);
    fflush(stdout);
    #endif
  }
  #ifdef DEBUG
  else {
    fwrite(str,sz,1,stdout);
    fwrite("DEBUG: ",7,1,stdout);
    vfprintf(stdout,format,ap);
    fflush(stdout);
  }
  #endif
  va_end(ap);

  pthread_mutex_unlock(&head.log.lock);
}
/* }}} */

/* {{{ cf_load_data */
int cf_load_data(cf_cfg_config_t *cfg,cf_forum_t *forum) {
  int ret = FLT_DECLINE;
  size_t i;
  cf_data_loading_filter_t fkt;
  cf_handler_config_t *handler;

  /* all references to this thread are released, so run the archiver plugins */
  if(cfg->modules[DATA_LOADING_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(i=0;i<cfg->modules[DATA_LOADING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = cf_array_element_at(&cfg->modules[DATA_LOADING_HANDLER],i);
      fkt     = (cf_data_loading_filter_t)handler->func;
      ret     = fkt(cfg,forum);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_cleanup_posting */
void cf_cleanup_posting(cf_posting_t *p) {
  if(p->user.name.len)  cf_str_cleanup(&p->user.name);
  if(p->subject.len)    cf_str_cleanup(&p->subject);
  if(p->unid.len)       cf_str_cleanup(&p->unid);
  if(p->user.ip.len)    cf_str_cleanup(&p->user.ip);
  if(p->content.len)    cf_str_cleanup(&p->content);

  if(p->category.len)   cf_str_cleanup(&p->category);
  if(p->user.email.len) cf_str_cleanup(&p->user.email);
  if(p->user.hp.len)    cf_str_cleanup(&p->user.hp);
  if(p->user.img.len)   cf_str_cleanup(&p->user.img);

  if(p->flags.elements) cf_list_destroy(&p->flags,cf_destroy_flag);
}
/* }}} */

/* {{{ cf_cleanup_thread */
void cf_cleanup_thread(cf_thread_t *t) {
  cf_posting_t *p,*p1;

  for(p=t->postings;p;p=p1) {
    cf_cleanup_posting(p);

    p1 = p->next;
    free(p);
  }

  cf_rwlock_destroy(&t->lock);
}
/* }}} */

/* {{{ cf_cleanup_forumtree */
void cf_cleanup_forumtree(cf_forum_t *forum) {
  cf_thread_t *t,*t1;

  for(t=forum->threads.list;t;t=t1) {
    cf_cleanup_thread(t);
    t1 = t->next;
    free(t);
  }
}
/* }}} */

/* {{{ cf_periodical_worker */
void *cf_periodical_worker(void *arg) {
  unsigned long rs = 0;
  cf_list_element_t *elem;
  cf_periodical_t *per;
  cf_cfg_config_t *cfg = (cf_cfg_config_t *)arg;

  cf_log(cfg,CF_DBG|CF_FLSH,__FILE__,__LINE__,"Periodical worker startet...\n");

  while(RUN) {
    sleep(1);
    ++rs;

    for(elem=head.periodicals.elements;elem;elem=elem->next) {
      per = (cf_periodical_t *)elem->data;
      if(rs % per->periode == 0 && rs >= per->periode) per->worker(cfg);
    }

    if(rs >= 86400) rs = 0;
  }

  for(elem=head.periodicals.elements;elem;elem=elem->next) {
    per = (cf_periodical_t *)elem->data;
    per->worker(cfg);
  }

  return NULL;
}
/* }}} */

/* {{{ cf_io_worker */
void cf_io_worker(cf_cfg_config_t *cfg) {
  cf_log(cfg,CF_DBG|CF_FLSH,__FILE__,__LINE__,"starting cf_run_archiver()...\n");
  cf_run_archiver(cfg);

  cf_log(cfg,CF_DBG|CF_FLSH,__FILE__,__LINE__,"starting cf_write_threadlist(NULL)...\n");
  cf_write_threadlist(cfg,NULL);
}
/* }}} */

/* {{{ cf_worker */
void *cf_worker(void *arg) {
  cf_list_element_t *selfelem,*lclient;
  pthread_t thread,self;
  struct timespec timeout;
  cf_client_t *client;
  cf_cfg_config_t *cfg = (cf_cfg_config_t *)arg;


  /* {{{ get our list element */
  self = pthread_self();

  do {
    /* give main thread time to append us to the worker list */
    usleep(50);

    CF_RW_RD(cfg,&head.workers.list.lock);
    for(selfelem=head.workers.list.head.elements;selfelem;selfelem=selfelem->next) {
      thread = *((pthread_t *)selfelem->data);
      if(thread == self) break;
    }
    CF_RW_UN(cfg,&head.workers.list.lock);
  } while(selfelem == NULL);
  /* }}} */


  while(RUN) {
    /*
     * First, we look, if there are already entries in the queque. Therefore
     * we have to lock it.
     */
    CF_LM(cfg,&head.clients.lock);

    while(!head.clients.list.elements && RUN) {
      /*  ok, no clients seem to exist in the queque. Lets wait for them. */
      CF_UM(cfg,&head.clients.lock);

      timeout.tv_sec = time(NULL) + 5;
      timeout.tv_nsec = 0;

      if(CF_CD_TW(cfg,&head.clients.cond,&timeout) != 0) {
        /* we got a timeout; go and try it again */
        CF_LM(cfg,&head.clients.lock);
        continue;
      }

      /* regular signal, go and look for client */
      CF_LM(cfg,&head.clients.lock);
    }

    if(RUN) {
      lclient = head.clients.list.elements;

      if(lclient) {
        head.clients.list.elements = lclient->next;
        head.clients.num--;

        if(!head.clients.list.elements) head.clients.list.last = NULL;

        CF_UM(cfg,&head.clients.lock);

        client = (cf_client_t *)lclient->data;
        if(client->worker) client->worker(cfg,client->sock);
        else {
          cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Client without worker?!\n");
          close(client->sock);
        }

        free(client);
        free(lclient);
      }
      else CF_UM(cfg,&head.clients.lock);
    }
    else CF_UM(cfg,&head.clients.lock);
  }

  cf_log(cfg,CF_DBG|CF_FLSH,__FILE__,__LINE__,"going down...\n");
  return NULL;
}
/* }}} */

/* {{{ cf_setup_socket */
int cf_setup_socket(cf_cfg_config_t *cfg,struct sockaddr_un *addr) {
  int sock;
  cf_cfg_config_value_t *sockpath = cf_cfg_get_value(cfg,"DF:SocketName");

  if((sock = socket(AF_LOCAL,SOCK_STREAM,0)) == -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"socket: %s\n",strerror(errno));
    RUN = 0;
    return sock;
  }

  unlink(sockpath->sval);

  memset(addr,0,sizeof(*addr));
  addr->sun_family = AF_LOCAL;
  strncpy(addr->sun_path,sockpath->sval,104);

  if(bind(sock,(struct sockaddr *)addr,sizeof(*addr)) < 0) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"bind(%s): %s\n",sockpath->sval,strerror(errno));
    RUN = 0;
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"listen: %s\n",strerror(errno));
    RUN = 0;
    return -1;
  }

  /*
   * we don't care for errors in this chmod() call; if it
   * fails, the administrator has to do it by hand
   */
  chmod(sockpath->sval,S_IRWXU|S_IRWXG|S_IRWXO);

  return sock;
}
/* }}} */

/* {{{ cf_push_server */
void cf_push_server(cf_cfg_config_t *cfg,int sockfd,struct sockaddr *addr,size_t size,cf_worker_t handler) {
  cf_server_t srv;

  srv.sock   = sockfd;
  srv.size   = size;
  srv.worker = handler;
  srv.addr   = cf_memdup(addr,size);

  CF_LM(cfg,&head.servers.lock);
  cf_list_append(&head.servers.list,&srv,sizeof(srv));
  CF_UM(cfg,&head.servers.lock);
}
/* }}} */

/* {{{ cf_push_client */
int cf_push_client(cf_cfg_config_t *cfg,int connfd,cf_worker_t handler,int spare_threads,int max_threads,pthread_attr_t *attr) {
  int status,num;
  pthread_t thread;
  cf_client_t client;

  client.worker = handler;
  client.sock   = connfd;

  /*
   * we don't care for access synchronization of head.workers.num
   * 'cause we are the only one accessing this value
   */

  CF_LM(cfg,&head.clients.lock);

  /* {{{ are there enough workers? */
  while(head.workers.num < head.clients.num + spare_threads && head.workers.num < max_threads) {
    if((status = pthread_create(&thread,attr,cf_worker,NULL)) != 0) {
      cf_log(cfg,CF_ERR,__FILE__,__LINE__,"pthread_create: %s\n",strerror(status));
      RUN = 0;
      return -1;
    }

    cf_rw_list_append(cfg,&head.workers.list,&thread,sizeof(thread));
    cf_log(cfg,CF_STD,__FILE__,__LINE__,"created worker %d now!\n",++head.workers.num);
  }
  /* }}} */

  /* {{{ are we in very high traffic? */
  if(head.clients.num >= max_threads) {
    CF_UM(cfg,&head.clients.lock);

    cf_log(cfg,CF_STD,__FILE__,__LINE__,"handling request directly...\n");
    handler(cfg,connfd);
  }
  /* }}} */
  else {
    cf_list_append(&head.clients.list,&client,sizeof(client));
    num = ++head.clients.num;

    CF_UM(cfg,&head.clients.lock);

    /*
     * we broadcast instead of signaling to get more than one worker
     * working
     */
    CF_CD_BC(cfg,&head.clients.cond);

    /* uh, high traffic, go sleeping for 20ms (this gives the workers time to work) */
    if(num >= max_threads - spare_threads * 2) usleep(20);
  }

  return 0;
}
/* }}} */

/* {{{ cf_register_protocol_handler */
int cf_register_protocol_handler(cf_cfg_config_t *cfg,u_char *handler_hook,cf_server_protocol_handler_t handler) {
  CF_RW_WR(cfg,&head.lock);

  if(head.protocol_handlers == NULL) {
    if((head.protocol_handlers = cf_hash_new(NULL)) == NULL) {
      cf_log(cfg,CF_ERR,__FILE__,__LINE__,"cf_hash_new: %s\n",strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if(cf_hash_set_static(head.protocol_handlers,handler_hook,strlen(handler_hook),handler) == 0) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"cf_hash_set: %s\n",strerror(errno));
    return -1;
  }

  CF_RW_UN(cfg,&head.lock);

  return 0;
}
/* }}} */

/* {{{ cf_register_thread */
void cf_register_thread(cf_cfg_config_t *cfg,cf_forum_t *forum,cf_thread_t *t) {
  CF_RW_WR(cfg,&forum->threads.lock);
  if(!forum->threads.threads) forum->threads.threads = cf_hash_new(NULL);

  cf_hash_set(forum->threads.threads,(u_char *)&t->tid,sizeof(t->tid),&t,sizeof(t));

  CF_RW_UN(cfg,&forum->threads.lock);
}
/* }}} */

/* {{{ cf_unregister_thread */
void cf_unregister_thread(cf_cfg_config_t *cfg,cf_forum_t *forum,cf_thread_t *t) {
  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"unregistering thread %"PRIu64"...\n",t->tid);

  CF_RW_WR(cfg,&forum->threads.lock);
  cf_hash_entry_delete(forum->threads.threads,(u_char *)&t->tid,sizeof(t->tid));
  CF_RW_UN(cfg,&forum->threads.lock);
}
/* }}} */

/* {{{ cf_remove_thread */
void cf_remove_thread(cf_cfg_config_t *cfg,cf_forum_t *forum,cf_thread_t *t) {
  CF_RW_WR(cfg,&t->lock);
  if(t->prev) {
    CF_RW_WR(cfg,&t->prev->lock);
    t->prev->next = t->next;

    if(t->next) {
      CF_RW_WR(cfg,&t->next->lock);
      t->next->prev = t->prev;
      CF_RW_UN(cfg,&t->next->lock);
    }

    CF_RW_UN(cfg,&t->prev->lock);
  }
  else {
    CF_RW_WR(cfg,&forum->threads.lock);
    forum->threads.list = t->next;

    if(t->next) {
      CF_RW_WR(cfg,&t->next->lock);
      t->next->prev = NULL;
      CF_RW_UN(cfg,&t->next->lock);
    }

    CF_RW_UN(cfg,&forum->threads.lock);
  }
  CF_RW_UN(cfg,&t->lock);
}
/* }}} */

/* {{{ cf_tokenize */
int cf_tokenize(u_char *line,u_char ***tokens) {
  int n = 0,reser = 5;
  register u_char *ptr,*prev;

  *tokens = cf_alloc(NULL,5,sizeof(*tokens),CF_ALLOC_MALLOC);

  for(prev=ptr=line;*ptr;ptr++) {
    if(n >= reser) {
      reser += 5;
      *tokens = cf_alloc(*tokens,reser,sizeof(*tokens),CF_ALLOC_REALLOC);
    }

    if(isspace(*ptr) || *ptr == ':') {
      if(ptr - 1 == prev || ptr == prev) {
        prev = ptr;
        continue;
      }

      *ptr = 0;
      (*tokens)[n++] = cf_memdup(prev,ptr-prev+1);
      prev = ptr+1;
    }
  }

  if(prev != ptr && prev-1 != ptr && *ptr) (*tokens)[n++] = cf_memdup(prev,ptr-prev);

  return n;
}
/* }}} */

/* {{{ cf_cftp_handler */
void cf_cftp_handler(cf_cfg_config_t *cfg,int sockfd) {
  int           shallRun = 1,i;
  rline_t      *tsd        = cf_alloc(NULL,1,sizeof(*tsd),CF_ALLOC_CALLOC);
  u_char *line  = NULL,**tokens;
  int   locked = 0,tnum = 0;
  cf_forum_t *forum = NULL;
  cf_server_protocol_handler_t handler;
  int ret;


  while(shallRun) {
    line = readline(sockfd,tsd);
    cf_log(cfg,CF_DBG,__FILE__,__LINE__,"%s",line?line:(u_char *)"(NULL)\n");

    if(line) {
      tnum = cf_tokenize(line,&tokens);

      if(tnum) {
        /* {{{ quit this request */
        if(cf_strcmp(tokens[0],"QUIT") == 0) {
          shallRun = 0;
          writen(sockfd,"200 Bye, bye\n",13);
        }
        /* }}} */
        /* {{{ select a forum */
        else if(cf_strcmp(tokens[0],"SELECT") == 0) {
          if(tnum == 2) {
            CF_RW_RD(cfg,&head.lock);
            forum = cf_hash_get(head.forums,tokens[1],strlen(tokens[1]));
            CF_RW_UN(cfg,&head.lock);

            cf_log(cfg,CF_DBG,__FILE__,__LINE__,"here we go again, forum name: %s, forum: %p\n",tokens[1],forum);

            if(forum == NULL) writen(sockfd,"509 Forum does not exist\n",25);
            else writen(sockfd,"200 Ok\n",7);
          }
          else writen(sockfd,"500 Sorry\n",10);
        }
        /* }}} */
        /* {{{ forum is selected, other commands are allowed */
        else if(forum) {
          /* {{{ archive a thread */
          if(cf_strcmp(tokens[0],"ARCHIVE") == 0) {
            u_int64_t tid;
            u_char *ln = readline(sockfd,tsd);

            if(ln == NULL) {
              cf_log(cfg,CF_ERR,__FILE__,__LINE__,"declined archivation because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            if(tnum < 3) {
              writen(sockfd,"500 Sorry\n",10);
              cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Bad request\n");
            }
            else {
              tid      = cf_str_to_uint64(tokens[2]+1);
              cf_log(cfg,CF_ERR,__FILE__,__LINE__,"archiving thread %"PRIu64" by user %s",tid,ln);

              writen(sockfd,"200 Ok\n",7);

              cf_archive_thread(cfg,forum,tid);
              cf_generate_cache(cfg,forum);
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ delete a thread */
          else if(cf_strcmp(tokens[0],"DELETE") == 0) {
            u_int64_t tid,mid;
            cf_thread_t *t;
            cf_posting_t *p;
            int lvl;
            u_char *ln = readline(sockfd,tsd);

            if(ln == NULL) {
              cf_log(cfg,CF_ERR,__FILE__,__LINE__,"declined deletion because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            else if(tnum != 3) writen(sockfd,"501 Thread id or message id missing\n",36);
            else {
              tid = cf_str_to_uint64(tokens[1]+1);
              mid = cf_str_to_uint64(tokens[2]+1);

              t = cf_get_thread(cfg,forum,tid);

              if(!t) {
                writen(sockfd,"404 Thread Not Found\n",21);
                cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Thread not found\n");
              }
              else {
                p = cf_get_posting(cfg,t,mid);

                if(!p) {
                  writen(sockfd,"404 Message Not Found\n",22);
                  cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Message not found\n");
                }
                else {
                  cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Deleted posting %"PRIu64" in thread %"PRIu64" by user %s",mid,tid,ln);

                  CF_RW_WR(cfg,&t->lock);

                  lvl          = p->level;
                  p->invisible = 1;

                  for(p=p->next;p && p->level > lvl;p=p->next) p->invisible = 1;

                  writen(sockfd,"200 Ok\n",7);

                  CF_RW_UN(cfg,&t->lock);

                  /* we need new caches if a message has been deleted */
                  cf_generate_cache(cfg,forum);
                }
              }
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ undelete a thread */
          else if(cf_strcmp(tokens[0],"UNDELETE") == 0) {
            u_int64_t tid,mid;
            cf_thread_t *t;
            cf_posting_t *p;
            int lvl;
            u_char *ln = readline(sockfd,tsd);

            if(ln == NULL) {
              cf_log(cfg,CF_ERR,__FILE__,__LINE__,"declined undelete because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            if(tnum < 3) writen(sockfd,"501 Thread id or message id missing\n",36);
            else {
              tid = cf_str_to_uint64(tokens[1]+1);
              mid = cf_str_to_uint64(tokens[2]+1);

              t = cf_get_thread(cfg,forum,tid);

              if(!t) {
                writen(sockfd,"404 Thread Not Found\n",21);
                cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Thread not found\n");
              }
              else {
                p = cf_get_posting(cfg,t,mid);
                cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Undelete posting %"PRIu64" in thread %"PRIu64" by user %s\n",mid,tid,ln);

                if(!p) {
                  writen(sockfd,"404 Message Not Found\n",22);
                  cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Message not found\n");
                }
                else {
                  CF_RW_WR(cfg,&t->lock);

                  lvl          = p->level;
                  p->invisible = 0;

                  for(p=p->next;p && p->level > lvl;p=p->next) p->invisible = 0;

                  CF_RW_UN(cfg,&t->lock);

                  writen(sockfd,"200 Ok\n",7);

                  /* we need new caches if a message has been undeleted */
                  cf_generate_cache(cfg,forum);
                }
              }
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ unlock a forum */
          else if(cf_strcmp(line,"UNLOCK") == 0) {
            CF_RW_WR(cfg,&forum->lock);
            forum->locked = 0;
            CF_RW_UN(cfg,&forum->lock);

            writen(sockfd,"200 Ok\n",7);
          }
          /* }}} */
          /* {{{ lock forum */
          else if(cf_strcmp(tokens[0],"LOCK") == 0) {
            CF_RW_WR(cfg,&forum->lock);
            forum->locked = 1;
            CF_RW_UN(cfg,&forum->lock);

            writen(sockfd,"200 Ok\n",7);
          }
          /* }}} */
          /* {{{ everything else may only be done when the forum is *not* locked */
          else {
            CF_RW_RD(cfg,&forum->lock);
            locked = forum->locked;
            CF_RW_UN(cfg,&forum->lock);

            if(locked == 0) {
              /* {{{ handler plugin or 500 */
              CF_RW_RD(cfg,&head.lock);

              if(head.protocol_handlers) {
                CF_RW_UN(cfg,&head.lock);

                handler = cf_hash_get(head.protocol_handlers,tokens[0],strlen(tokens[0]));
                if(handler == NULL) writen(sockfd,"500 What's up?\n",15);
                else {
                  ret = handler(cfg,sockfd,forum,(const u_char **)tokens,tnum,tsd);
                  if(ret == FLT_DECLINE) writen(sockfd,"500 What's up?\n",15);
                }
              }
              else {
                CF_RW_UN(cfg,&head.lock);
                writen(sockfd,"500 What's up?\n",15);
              }
              /* }}} */
            }
            else writen(sockfd,"600 Forum locked\n",17);
          }
          /* }}} */
        }
        /* }}} */
        else writen(sockfd,"508 No forum selected\n",22);

        free(line);
        line = NULL;

        for(i=0;i<tnum;i++) free(tokens[i]);
        free(tokens);
      }
      else writen(sockfd,"500 What's up?\n",15);
    }
    else shallRun = 0; /* connection broke */
  }

  if(line) free(line);

  free(tsd);
  close(sockfd);
}
/* }}} */

/* {{{ cf_get_posting */
cf_posting_t *cf_get_posting(cf_cfg_config_t *cfg,cf_thread_t *t,u_int64_t mid) {
  cf_posting_t *p;

  CF_RW_RD(cfg,&t->lock);

  for(p=t->postings;p && p->mid != mid;p=p->next);

  CF_RW_UN(cfg,&t->lock);

  return p;  /* this returns NULL or the posting */
}
/* }}} */

/* {{{ cf_get_thread */
cf_thread_t *cf_get_thread(cf_cfg_config_t *cfg,cf_forum_t *forum,u_int64_t tid) {
  cf_thread_t **t = NULL;

  CF_RW_RD(cfg,&forum->threads.lock);
  if(forum->threads.threads) t = cf_hash_get(forum->threads.threads,(u_char *)&tid,sizeof(tid));
  CF_RW_UN(cfg,&forum->threads.lock);

  return t ? *t : NULL;
}
/* }}} */

/* {{{ cf_get_flag_by_name */
cf_posting_flag_t *cf_get_flag_by_name(cf_list_head_t *flags,const u_char *name) {
  cf_list_element_t *elem;
  cf_posting_flag_t *flag;

  for(elem=flags->elements;elem;elem=elem->next) {
    flag = (cf_posting_flag_t *)elem->data;
    if(cf_strcmp(flag->name,name) == 0) return flag;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_send_posting */
void cf_send_posting(cf_cfg_config_t *cfg,cf_forum_t *forum,int sock,u_int64_t tid,u_int64_t mid,int invisible) {
  cf_thread_t *t = cf_get_thread(cfg,forum,tid);
  cf_posting_t *p = NULL;
  cf_string_t bff;
  int first = 1;

  cf_list_element_t *elem;
  cf_posting_flag_t *flag;

  if(!t) {
    writen(sock,"404 Thread not found\n",21);
    return;
  }

  if(mid != 0) {
    p = cf_get_posting(cfg,t,mid);

    if(!p) {
      writen(sock,"404 Posting not found\n",22);
      return;
    }
  }

  cf_str_init(&bff);

  cf_str_chars_append(&bff,"200 Ok\n",7);

  CF_RW_RD(cfg,&t->lock);

  if(p) {
    if(p->invisible == 1 && !invisible) {
      writen(sock,"404 Posting not found\n",22);
      CF_RW_UN(cfg,&t->lock);
      return;
    }
  }

  p = t->postings;
  cf_str_chars_append(&bff,"THREAD t",8);
  cf_uint64_to_str(&bff,t->tid);
  cf_str_chars_append(&bff," m",2);
  cf_uint64_to_str(&bff,p->mid);
  cf_str_char_append(&bff,'\n');

  for(;p;p=p->next) {
    if(p->invisible && !invisible) {
      for(;p && p->invisible;p=p->next);
      if(!p) break;
    }

    if(!first) {
      cf_str_chars_append(&bff,"MSG m",5);
      cf_uint64_to_str(&bff,p->mid);
      cf_str_char_append(&bff,'\n');
    }

    first = 0;

    /* {{{ serialize flags */
    for(elem=p->flags.elements;elem;elem=elem->next) {
      flag = (cf_posting_flag_t *)elem->data;
      cf_str_chars_append(&bff,"Flag:",5);
      cf_str_chars_append(&bff,flag->name,strlen(flag->name));
      cf_str_char_append(&bff,'=');
      cf_str_chars_append(&bff,flag->val,strlen(flag->val));
      cf_str_char_append(&bff,'\n');
    }
    /* }}} */

    cf_str_chars_append(&bff,"Author:",7);
    cf_str_chars_append(&bff,p->user.name.content,p->user.name.len);

    if(p->user.email.len) {
      cf_str_chars_append(&bff,"\nEMail:",7);
      cf_str_chars_append(&bff,p->user.email.content,p->user.email.len);
    }

    if(p->user.hp.len) {
      cf_str_chars_append(&bff,"\nHomepage:",10);
      cf_str_chars_append(&bff,p->user.hp.content,p->user.hp.len);
    }

    if(p->user.img.len) {
      cf_str_chars_append(&bff,"\nImage:",7);
      cf_str_chars_append(&bff,p->user.img.content,p->user.img.len);
    }

    cf_str_chars_append(&bff,"\nSubject:",9);
    cf_str_chars_append(&bff,p->subject.content,p->subject.len);

    if(p->category.len) {
      cf_str_chars_append(&bff,"\nCategory:",10);
      cf_str_chars_append(&bff,p->category.content,p->category.len);
    }

    cf_str_chars_append(&bff,"\nRemote-Addr:",13);
    cf_str_str_append(&bff,&p->user.ip);

    cf_str_chars_append(&bff,"\nDate:",6);
    cf_uint32_to_str(&bff,p->date);
    cf_str_char_append(&bff,'\n');

    cf_str_chars_append(&bff,"Level:",6);
    cf_uint16_to_str(&bff,p->level);
    cf_str_char_append(&bff,'\n');

    cf_str_chars_append(&bff,"Visible:",8);
    cf_uint16_to_str(&bff,(u_int16_t)(p->invisible == 0));
    cf_str_char_append(&bff,'\n');

    cf_str_chars_append(&bff,"Votes-Good:",11);
    cf_uint32_to_str(&bff,p->votes_good);
    cf_str_char_append(&bff,'\n');

    cf_str_chars_append(&bff,"Votes-Bad:",10);
    cf_uint32_to_str(&bff,p->votes_bad);
    cf_str_char_append(&bff,'\n');

    cf_str_chars_append(&bff,"Content:",8);
    cf_str_chars_append(&bff,p->content.content,p->content.len);

    cf_str_char_append(&bff,'\n');
  }

  CF_RW_UN(cfg,&t->lock);

  cf_str_chars_append(&bff,"END\n",4);

  writen(sock,bff.content,bff.len);
  free(bff.content);
}
/* }}} */

/* {{{ cf_read_posting */
int cf_read_posting(cf_cfg_config_t *cfg,cf_forum_t *forum,cf_posting_t *p,int sock,rline_t *tsd) {
  u_char *line = NULL;
  u_char *ptr;
  unsigned long llen;
  cf_posting_flag_t flag;

  do {
    line = readline(sock,tsd);

    if(line) {
      llen = tsd->rl_len;
      line[llen-1] = '\0';

      cf_log(cfg,CF_DBG,__FILE__,__LINE__,"read_posting: got line %s\n",line);

      if(cf_strncmp(line,"Unid:",5) == 0) {
        cf_str_char_set(&p->unid,line+6,llen-7);
      }
      else if(cf_strncmp(line,"Author:",7) == 0) {
        cf_str_char_set(&p->user.name,line+8,llen-9);
      }
      else if(cf_strncmp(line,"EMail:",6) == 0) {
        cf_str_char_set(&p->user.email,line+7,llen-8);
      }
      else if(cf_strncmp(line,"Category:",9) == 0) {
        cf_str_char_set(&p->category,line+10,llen-11);
      }
      else if(cf_strncmp(line,"Subject:",8) == 0) {
        cf_str_char_set(&p->subject,line+9,llen-10);
      }
      else if(cf_strncmp(line,"HomepageUrl:",12) == 0) {
        cf_str_char_set(&p->user.hp,line+13,llen-14);
      }
      else if(cf_strncmp(line,"ImageUrl:",9) == 0) {
        cf_str_char_set(&p->user.img,line+10,llen-11);
      }
      else if(cf_strncmp(line,"Body:",5) == 0) {
        cf_str_char_set(&p->content,line+6,llen-7);
      }
      else if(cf_strncmp(line,"Invisible:",10) == 0) {
        p->invisible = atoi(line+11);
      }
      else if(cf_strncmp(line,"RemoteAddr:",11) == 0) {
        cf_str_char_set(&p->user.ip,line+12,llen-13);
      }
      else if(cf_strncmp(line,"Flag:",5) == 0) {
        if((ptr = strstr(line+6,"=")) == NULL) {
          writen(sock,"500 Sorry\n",10);
          return 0;
        }

        flag.name = strndup(line+6,ptr-line-6);
        flag.val  = strdup(ptr+1);

        cf_list_append(&p->flags,&flag,sizeof(flag));
      }
      else {
        free(line);
        line = NULL;
      }

      if(line) free(line);
    }
    else cf_log(cfg,CF_ERR,__FILE__,__LINE__,"readline: %s\n",strerror(errno));
  } while(line);

  p->date = time(NULL);

  if(!p->user.name.len || !p->user.ip.len || !p->unid.len) {
    writen(sock,"500 Sorry\n",10);
    return 0;
  }

  CF_RW_WR(cfg,&forum->threads.lock);
  p->mid  = ++forum->threads.last_mid;
  CF_RW_UN(cfg,&forum->threads.lock);

  return 1;
}
/* }}} */

/* {{{ cf_remove_flags */
int cf_remove_flags(cf_cfg_config_t *cfg,int sockfd,rline_t *tsd,cf_posting_t *p1) {
  size_t len,i;
  u_char *line,**list;
  cf_posting_flag_t *flagp;
  cf_list_element_t *elem;

  if((line = readline(sockfd,tsd)) != NULL) {
    line[tsd->rl_len-1] = '\0';

    if(cf_strncmp(line,"Flags:",6) == 0) {
      len = cf_split(line+7,",",&list);
      for(i=0;i<len;++i) {
        cf_log(cfg,CF_DBG,__FILE__,__LINE__,"removing flag %s\n",list[i]);

        for(elem=p1->flags.elements;elem;elem=elem->next) {
          flagp = (cf_posting_flag_t *)elem->data;
          if(cf_strcmp(flagp->name,list[i]) == 0) {
            cf_list_delete(&p1->flags,elem);

            free(elem);
            free(flagp->name);
            free(flagp->val);
            free(flagp);
            break;
          }
        }

        free(list[i]);
      }

      free(list);
    }

    free(line);
  }

  return 0;
}
/* }}} */

/* {{{ cf_read_flags */
int cf_read_flags(cf_cfg_config_t *cfg,int sockfd,rline_t *tsd,cf_posting_t *p) {
  u_char *line,*ptr;
  cf_posting_flag_t flag,*flagp;

  while((line = readline(sockfd,tsd)) != NULL) {
    line[tsd->rl_len-1] = '\0';

    if(cf_strncmp(line,"Flag:",5) == 0) {
      if((ptr = strstr(line+6,"=")) == NULL) {
        writen(sockfd,"500 Sorry\n",10);
        cf_log(cfg,CF_ERR,__FILE__,__LINE__,"Bad request in flag reading phase\n");
        return -1;
      }

      flag.name = strndup(line+6,ptr-line-6);
      flag.val  = strdup(ptr+1);

      cf_log(cfg,CF_DBG,__FILE__,__LINE__,"Setting flag %s=%s\n",flag.name,flag.val);

      if((flagp = cf_get_flag_by_name(&p->flags,flag.name)) == NULL) {
        cf_log(cfg,CF_DBG,__FILE__,__LINE__,"Having it not, calling cf_list_append()\n");
        cf_list_append(&p->flags,&flag,sizeof(flag));
      }
      else {
        cf_log(cfg,CF_DBG,__FILE__,__LINE__,"Already have it\n");
        free(flagp->val);
        free(flag.name);
        flagp->val = flag.val;
      }
    }
    else {
      free(line);
      return 0;
    }

    free(line);
  }

  return 0;
}
/* }}} */

/* {{{ cf_destroy_flag */
void cf_destroy_flag(void *data) {
  cf_posting_flag_t *flag = (cf_posting_flag_t *)data;
  free(flag->name);
  free(flag->val);
}
/* }}} */

/* {{{ cf_generate_cache */
void cf_generate_cache(cf_cfg_config_t *cfg,cf_forum_t *forum) {
  cf_cfg_config_value_t *forums;
  size_t i = 0;

#ifndef CF_SHARED_MEM
  cf_string_t str1,str2;

  /* {{{ make cache for only one forum */
  if(forum) {
    cf_str_init(&str1);
    cf_str_init(&str2);

    cf_generate_list(cfg,forum,&str1,0);
    cf_generate_list(cfg,forum,&str2,1);

    CF_RW_WR(cfg,&forum->lock);

    if(forum->cache.invisible.content) free(forum->cache.invisible.content);
    if(forum->cache.visible.content) free(forum->cache.visible.content);

    forum->cache.invisible.content = str2.content;
    forum->cache.visible.content   = str1.content;
    forum->cache.invisible.len     = str2.len;
    forum->cache.visible.len       = str1.len;

    forum->date.visible            = time(NULL);
    forum->date.invisible          = time(NULL);

    forum->cache.fresh = 1;

    CF_RW_UN(cfg,&forum->lock);
  }
  /* }}} */
  /* {{{ make cache for all forums */
  else {
    forums = cf_cfg_get_value(cfg,"Forums");

    for(i=0;i<forums->alen;i++) {
      if((forum = cf_hash_get(head.forums,forums->avals[i].sval,strlen(forums->avals[i].sval))) != NULL) {
        cf_str_init(&str1);
        cf_str_init(&str2);

        cf_generate_list(cfg,forum,&str1,0);
        cf_generate_list(cfg,forum,&str2,1);

        CF_RW_WR(cfg,&forum->lock);

        if(forum->cache.invisible.content) free(forum->cache.invisible.content);
        if(forum->cache.visible.content) free(forum->cache.visible.content);

        forum->cache.invisible.content = str2.content;
        forum->cache.visible.content   = str1.content;
        forum->cache.invisible.len     = str2.len;
        forum->cache.visible.len       = str1.len;

        forum->date.visible            = time(NULL);
        forum->date.invisible          = time(NULL);

        forum->cache.fresh = 1;

        CF_RW_UN(cfg,&forum->lock);
      }
    }
  }
  /* }}} */
  #else
  if(forum) cf_generate_shared_memory(cfg,forum);
  else {
    forums = cf_cfg_get_value(cfg,"Forums");

    for(i=0;i<forums->alen;i++) {
      if((forum = cf_hash_get(head.forums,forums->avals[i].sval,strlen(forums->avals[i].sval))) != NULL) cf_generate_shared_memory(cfg,forum);
    }
  }
  #endif
}
/* }}} */

/* {{{ cf_generate_list */
void cf_generate_list(cf_cfg_config_t *cfg,cf_forum_t *forum,cf_string_t *str,int del) {
  int did = 0;
  cf_thread_t *t,*t1;
  int first;
  cf_posting_t *p;

  cf_list_element_t *elem;
  cf_posting_flag_t *flag;

  cf_str_chars_append(str,"200 Ok\n",7);

  CF_RW_RD(cfg,&forum->threads.lock);
  t1 = forum->threads.list;
  CF_RW_UN(cfg,&forum->threads.lock);

  while(t1) {
    first = 1;

    t = t1;

    CF_RW_RD(cfg,&t->lock);

    for(p = t->postings;p;p = p->next) {
      if(p->invisible && !del) {
        for(;p && p->invisible;p=p->next);
        if(!p) break;
      }

      did = 1;

      /* thread/posting header */
      if(first) {
        first = 0;
        cf_str_chars_append(str,"THREAD t",8);
        cf_uint64_to_str(str,t->tid);
      }
      else cf_str_chars_append(str,"MSG",3);

      cf_str_chars_append(str," m",2);
      cf_uint64_to_str(str,p->mid);
      cf_str_char_append(str,'\n');

      /* {{{ serialize flags */
      for(elem=p->flags.elements;elem;elem=elem->next) {
        flag = (cf_posting_flag_t *)elem->data;
        cf_str_chars_append(str,"Flag:",5);
        cf_str_chars_append(str,flag->name,strlen(flag->name));
        cf_str_char_append(str,'=');
        cf_str_chars_append(str,flag->val,strlen(flag->val));
        cf_str_char_append(str,'\n');
      }
      /* }}} */

      /* author */
      cf_str_chars_append(str,"Author:",7);
      cf_str_chars_append(str,p->user.name.content,p->user.name.len);

      /* subject */
      cf_str_chars_append(str,"\nSubject:",9);
      cf_str_chars_append(str,p->subject.content,p->subject.len);

      /* category */
      if(p->category.len) {
        cf_str_chars_append(str,"\nCategory:",10);
        cf_str_chars_append(str,p->category.content,p->category.len);
      }

      cf_str_chars_append(str,"\nRemote-Addr:",13);
      cf_str_str_append(str,&p->user.ip);

      /* date */
      cf_str_chars_append(str,"\nDate:",6);
      cf_uint32_to_str(str,(u_int32_t)p->date);
      cf_str_char_append(str,'\n');

      /* level */
      cf_str_chars_append(str,"Level:",6);
      cf_uint16_to_str(str,p->level);
      cf_str_char_append(str,'\n');

      cf_str_chars_append(str,"Visible:",8);
      cf_uint16_to_str(str,(u_int16_t)(p->invisible == 0));
      cf_str_char_append(str,'\n');
    }

    if(did) cf_str_chars_append(str,"END\n",4);
    did = 0;

    t1   = t->next;
    CF_RW_UN(cfg,&t->lock);
  }

  cf_str_char_append(str,'\n');
}
/* }}} */

#ifdef CF_SHARED_MEM
/* {{{ cf_shmdt */
/**
 * Wrapper function of the shmdt function. Used to log access
 * to the shared memory segments
 * \param ptr The pointer to the shared memory segment
 * \return Returns 0 or -1
 */
int cf_shmdt(cf_cfg_config_t *cfg,void *ptr) {
  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"shmdt: detaching %p\n",ptr);
  return shmdt(ptr);
}
/* }}} */

/* {{{ cf_shmat */
/**
 * Wrapper function to the shmat() function. Used to
 * log access to the shared memory segments
 * \param shmid The shared memory id
 * \param addr The start address
 * \param shmflag Flags to the shmat() function
 * \return NULL on failure, pointer on success
 */
void *cf_shmat(cf_cfg_config_t *cfg,int shmid,void *addr,int shmflag) {
  void *ptr = shmat(shmid,addr,shmflag);

  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"shmat: attatching segment %d (%p)\n",shmid,ptr);

  return ptr;
}
/* }}} */

/* {{{ cf_shm_flags ------DEEP MAGIC!-------- */
void cf_shm_flags(cf_posting_t *p,cf_mem_pool_t *pool) {
  cf_list_element_t *elem;
  u_int32_t val = 0;
  cf_posting_flag_t *flag;

  for(elem=p->flags.elements;elem;elem=elem->next,++val);
  cf_mem_append(pool,&val,sizeof(val));

  for(elem=p->flags.elements;elem;elem=elem->next) {
    flag = (cf_posting_flag_t *)elem->data;

    val = strlen(flag->name);
    cf_mem_append(pool,&val,sizeof(val));
    cf_mem_append(pool,flag->name,val);

    val = strlen(flag->val);
    cf_mem_append(pool,&val,sizeof(val));
    cf_mem_append(pool,flag->val,val);
  }
}
/* }}} */

/* {{{ cf_generate_shared_memory */
void cf_generate_shared_memory(cf_cfg_config_t *cfg,cf_forum_t *forum) {
  cf_mem_pool_t pool;
  cf_thread_t *t,*t1;
  cf_posting_t *p;
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  u_int32_t val;
  time_t tm = time(NULL);
  unsigned short semval;

  cf_mem_init(&pool);

  CF_RW_RD(cfg,&forum->threads.lock);
  t1 = t = forum->threads.list;
  CF_RW_UN(cfg,&forum->threads.lock);

  /* {{{ CAUTION! Deep magic begins here! */
  cf_mem_append(&pool,&tm,sizeof(t));

  for(;t;t=t1) {
    CF_RW_RD(cfg,&t->lock);

    /* we only need thread id and postings */
    cf_mem_append(&pool,&(t->tid),sizeof(t->tid));
    cf_mem_append(&pool,&(t->posts),sizeof(t->posts));

    for(p=t->postings;p;p=p->next) {
      cf_mem_append(&pool,&(p->mid),sizeof(p->mid));

      /* we have to count the number of flags... */
      cf_shm_flags(p,&pool);

      val = p->subject.len + 1;
      cf_mem_append(&pool,&val,sizeof(val));
      cf_mem_append(&pool,p->subject.content,val);

      val = p->category.len + 1;
      if(val > 1) {
        cf_mem_append(&pool,&val,sizeof(val));
        cf_mem_append(&pool,p->category.content,val);
      }
      else {
        val = 0;
        cf_mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.ip.len + 1;
      cf_mem_append(&pool,&val,sizeof(val));
      cf_mem_append(&pool,p->user.ip.content,val);

      val = p->content.len + 1;
      cf_mem_append(&pool,&val,sizeof(val));
      cf_mem_append(&pool,p->content.content,val);

      cf_mem_append(&pool,&(p->date),sizeof(p->date));
      cf_mem_append(&pool,&(p->level),sizeof(p->level));
      cf_mem_append(&pool,&(p->invisible),sizeof(p->invisible));
      cf_mem_append(&pool,&(p->votes_good),sizeof(p->votes_good));
      cf_mem_append(&pool,&(p->votes_bad),sizeof(p->votes_bad));

      val = p->user.name.len + 1;
      cf_mem_append(&pool,&val,sizeof(val));
      cf_mem_append(&pool,p->user.name.content,val);

      val = p->user.email.len + 1;
      if(val > 1) {
        cf_mem_append(&pool,&val,sizeof(val));
        cf_mem_append(&pool,p->user.email.content,val);
      }
      else {
        val = 0;
        cf_mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.hp.len + 1;
      if(val > 1) {
        cf_mem_append(&pool,&val,sizeof(val));
        cf_mem_append(&pool,p->user.hp.content,val);
      }
      else {
        val = 0;
        cf_mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.img.len + 1;
      if(val > 1) {
        cf_mem_append(&pool,&val,sizeof(val));
        cf_mem_append(&pool,p->user.img.content,val);
      }
      else {
        val = 0;
        cf_mem_append(&pool,&val,sizeof(val));
      }
    }

    t1 = t->next;
    CF_RW_UN(cfg,&t->lock);
    t = t1;
  }
  /*
   * Phew. Deep magic ends
   */
  /* }}} */

  /* lets go surfin' */
  CF_LM(cfg,&forum->shm.lock);

  if(cf_sem_getval(forum->shm.sem,0,1,&semval) == -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"cf_sem_getval: %s\n",strerror(errno));
    exit(-1);
  }

  /* semval contains now the number of the shared memory segment *not* used */
  if(semval != 0 && semval != 1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"hu? what happened? semval is %d\n",semval);
    semval = 0;
  }

  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"shm_ids[%d]: %ld\n",semval,forum->shm.ids[semval]);

  /* {{{ does the segment already exists? */
  if(forum->shm.ids[semval] != -1) {
    /* yeah, baby, yeah! */

    cf_log(cfg,CF_DBG,__FILE__,__LINE__,"shm_ptrs[%d]: %p\n",semval,forum->shm.ptrs[semval]);

    /* oh behave! detach the memory */
    if(forum->shm.ptrs[semval]) {
      if(cf_shmdt(cfg,forum->shm.ptrs[semval]) != 0) {
        cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmdt: %s (semval: %d)\n",strerror(errno),semval);
        CF_UM(cfg,&forum->shm.lock);
        cf_mem_cleanup(&pool);
        return;
      }
    }

    /* delete the segment */
    if(shmctl(forum->shm.ids[semval],IPC_RMID,NULL) != 0) {
      cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
      CF_UM(cfg,&forum->shm.lock);
      cf_mem_cleanup(&pool);
      return;
    }
  }
  /* }}} */

  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"shmget(%lu,%lu,%u)\n",v->avals[semval].ival,pool.len,IPC_EXCL|IPC_CREAT|CF_SHARED_MODE);
  if((forum->shm.ids[semval] = shmget(v->avals[semval].ival,pool.len,IPC_EXCL|IPC_CREAT|CF_SHARED_MODE)) == -1) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmget: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  if((forum->shm.ptrs[semval] = cf_shmat(cfg,forum->shm.ids[semval],NULL,0)) == NULL) {
    cf_log(cfg,CF_ERR,__FILE__,__LINE__,"shmat: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  memcpy(forum->shm.ptrs[semval],pool.content,pool.len);

  cf_mem_cleanup(&pool);

  if(semval == 1) CF_SEM_DOWN(forum->shm.sem,0);
  else CF_SEM_UP(forum->shm.sem,0);
  CF_UM(cfg,&forum->shm.lock);

  cf_log(cfg,CF_DBG,__FILE__,__LINE__,"generated shared memory segment %d\n",semval);
}
/* }}} */
#endif

/* eof */

