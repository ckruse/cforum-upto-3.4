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
#include "config.h"
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

#include "serverlib.h"
#include "fo_server.h"
#include "archiver.h"
/* }}} */


/* {{{ rw list functions */
/* {{{ cf_rw_list_init */
void cf_rw_list_init(const u_char *name,t_cf_rw_list_head *head) {
  cf_rwlock_init(name,&head->lock);
  cf_list_init(&head->head);
}
/* }}} */

/* {{{ cf_rw_list_append */
void cf_rw_list_append(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_append_static */
void cf_rw_list_append_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_append_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend */
void cf_rw_list_prepend(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_prepend_static */
void cf_rw_list_prepend_static(t_cf_rw_list_head *head,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_prepend_static(&head->head,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_insert */
void cf_rw_list_insert(t_cf_rw_list_head *head,t_cf_list_element *prev,void *data,size_t size) {
  CF_RW_WR(&head->lock);
  cf_list_insert(&head->head,prev,data,size);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_search */
void *cf_rw_list_search(t_cf_rw_list_head *head,void *data,int (*compare)(const void *data1,const void *data2)) {
  void *tmp;

  CF_RW_RD(&head->lock);
  tmp = cf_list_search(&head->head,data,compare);
  CF_RW_UN(&head->lock);

  return tmp;
}
/* }}} */

/* {{{ cf_rw_list_delete */
void cf_rw_list_delete(t_cf_rw_list_head *head,t_cf_list_element *elem) {
  CF_RW_WR(&head->lock);
  cf_list_delete(&head->head,elem);
  CF_RW_UN(&head->lock);
}
/* }}} */

/* {{{ cf_rw_list_destroy */
void cf_rw_list_destroy(t_cf_rw_list_head *head,void (*destroy)(void *data)) {
  CF_RW_WR(&head->lock);
  cf_list_destroy(&head->head,destroy);
  CF_RW_UN(&head->lock);

  cf_rwlock_destroy(&head->lock);
}
/* }}} */
/* }}} */

/* {{{ cf_setup_shared_mem */
void cf_setup_shared_mem(t_forum *forum) {
  union semun smn;
  unsigned short x = 0;
  t_name_value *v = cfg_get_first_value(&fo_default_conf,forum->name,"SharedMemIds");

  if((forum->shm.sem = semget(atoi(v->values[2]),1,S_IRWXU|S_IRWXG|S_IRWXO|IPC_CREAT)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"semget: %s\n",strerror(errno));
    exit(-1);
  }

  smn.array = &x;
  if(semctl(forum->shm.sem,0,SETALL,smn) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));
    exit(-1);
  }

  /* check if there are shared memory segments and, if there are, report an error */
  if(shmget(atoi(v->values[0]),0,0) != -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"The shared memory segments for forum %s already exist! This can be because of a crash, but this can also be because of a running server with this ids. We exit, please repair!\n",forum->name);
    exit(-1);
  }

  if(shmget(atoi(v->values[1]),0,0) != -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"The shared memory segments for forum %s already exist! This can be because of a crash, but this can also be because of a running server with this ids. We exit, please repair!\n",forum->name);
    exit(-1);
  }
}
/* }}} */

/* {{{ cf_register_forum */
t_forum *cf_register_forum(const u_char *name) {
  t_forum *forum = fo_alloc(NULL,1,sizeof(*forum),FO_ALLOC_MALLOC);
  t_name_value *nv;

  forum->name = strdup(name);
  forum->fresh = forum->locked = 0;

  str_init(&forum->cache.visible);
  str_init(&forum->cache.invisible);

  forum->date.visible = forum->date.invisible = 0;

  #ifdef CF_SHARED_MEM
  nv = cfg_get_first_value(&fo_default_conf,name,"SharedMemIds");
  forum->shm.ids[0] = atoi(nv->values[0]);
  forum->shm.ids[1] = atoi(nv->values[1]);
  forum->shm.sem    = atoi(nv->values[2]);

  cf_mutex_init("forum.shm.lock",&forum->shm.lock);

  cf_setup_shared_mem(forum);
  #endif

  cf_rwlock_init("forum.lock",&forum->lock);

  forum->threads.last_tid = forum->threads.last_mid = 0;
  forum->threads.threads  = cf_hash_new(NULL);
  forum->threads.list     = NULL;

  cf_rwlock_init("forum.threads.lock",&forum->threads.lock);

  forum->uniques.ids = cf_hash_new(NULL);
  cf_mutex_init("forum.uniques.lock",&forum->uniques.lock);

  cf_hash_set_static(head.forums,(u_char *)name,strlen(name),forum);
  return forum;
}
/* }}} */

void cf_destroy_forum(t_forum *forum) {
  int i;

  cf_rwlock_destroy(&forum->lock);
  cf_rwlock_destroy(&forum->threads.lock);
  cf_mutex_destroy(&forum->uniques.lock);

  free(forum->name);
  str_cleanup(&forum->cache.visible);
  str_cleanup(&forum->cache.invisible);

  #ifdef CF_SHARED_MEM
  cf_mutex_destroy(&forum->shm.lock);

  if(forum->shm.sem >= 0) if(semctl(forum->shm.sem,0,IPC_RMID,NULL) == -1) cf_log(CF_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));

  for(i=0;i<2;++i) {
    if(forum->shm.ids[i] >= 0) {
      if(forum->shm.ptrs[i])
        if(shmdt(forum->shm.ptrs[i]) < 0) cf_log(CF_ERR,__FILE__,__LINE__,"shmdt: %s\n",strerror(errno));

      if(shmctl(forum->shm.ids[i],IPC_RMID,0) < 0) cf_log(CF_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
    }
  }
  #endif

  cf_hash_destroy(forum->threads.threads);
  cf_hash_destroy(forum->uniques.ids);

  if(forum->threads.list) cf_cleanup_forumtree(forum);
}

/* {{{ cf_log */
void cf_log(int mode,const u_char *file,unsigned int line,const u_char *format, ...) {
  u_char str[300];
  int status;
  t_name_value *v;
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
    v = cfg_get_first_value(&fo_server_conf,NULL,"StdLog");
    head.log.std = fopen(v->values[0],"a");
  }
  if(!head.log.err) {
    v = cfg_get_first_value(&fo_server_conf,NULL,"ErrorLog");
    head.log.err = fopen(v->values[0],"a");
  }

  sz = snprintf(str,300,"[%4d-%02d-%02d %02d:%02d:%02d]:%s:%d ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ptr1,line);

  va_start(ap, format);
  if(mode & CF_ERR) {
    fwrite(str,sz,1,head.log.err);
    vfprintf(head.log.err,format,ap);

    if(mode & CF_FLSH) fflush(head.log.err);

    #ifdef DEBUG
    fwrite(str,sz,1,stderr);
    vfprintf(stderr,format,ap);
    fflush(stderr);
    #endif
  }
  else if(mode & CF_STD) {
    fwrite(str,sz,1,head.log.std);
    vfprintf(head.log.std,format,ap);

    if(mode & CF_FLSH) fflush(head.log.std);

    #ifdef DEBUG
    fwrite(str,sz,1,stdout);
    vfprintf(stdout,format,ap);
    fflush(stdout);
    #endif
  }
  #ifdef DEBUG
  else {
    fwrite(str,sz,1,head.log.std);
    fwrite(str,sz,1,stdout);
    fwrite("DEBUG: ",7,1,head.log.std);
    fwrite("DEBUG: ",7,1,stdout);
    vfprintf(head.log.std,format,ap);
    vfprintf(stdout,format,ap);
    if(mode & CF_FLSH) fflush(head.log.std);
    fflush(stdout);
  }
  #endif
  va_end(ap);

  pthread_mutex_unlock(&head.log.lock);
}
/* }}} */

/* {{{ cf_load_data */
int cf_load_data(t_forum *forum) {
  int ret;
  size_t i;
  t_data_loading_filter fkt;
  t_handler_config *handler;

  /* all references to this thread are released, so run the archiver plugins */
  if(Modules[DATA_LOADING_HANDLER].elements) {
    ret = FLT_DECLINE;

    for(i=0;i<Modules[DATA_LOADING_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[DATA_LOADING_HANDLER],i);
      fkt     = (t_data_loading_filter)handler->func;
      ret     = fkt(forum);
    }
  }

  return ret;
}
/* }}} */

/* {{{ cf_cleanup_forumtree */
void cf_cleanup_forumtree(t_forum *forum) {
  t_thread *t,*t1;
  t_posting *p,*p1;

  for(t=forum->threads.list;t;t=t1) {
    for(p=t->postings;p;p=p1) {
      str_cleanup(&p->user.name);
      str_cleanup(&p->subject);
      str_cleanup(&p->unid);
      str_cleanup(&p->user.ip);
      str_cleanup(&p->content);

      if(p->category.len)   str_cleanup(&p->category);
      if(p->user.email.len) str_cleanup(&p->user.email);
      if(p->user.hp.len)    str_cleanup(&p->user.hp);
      if(p->user.img.len)   str_cleanup(&p->user.img);

      p1 = p->next;
      free(p);
    }

    cf_rwlock_destroy(&t->lock);

    t1 = t->next;
    free(t);
  }

  /** \todo write more cleanup code */
}
/* }}} */

/* {{{ cf_worker */
void *cf_worker(void *arg) {
  t_cf_list_element *selfelem,*lclient;
  pthread_t thread,self;
  int run = 1;
  struct timespec timeout;
  t_cf_client *client;


  /* {{{ get our list element */
  self = pthread_self();

  do {
    /* give main thread time to append us to the worker list */
    usleep(50);

    CF_RW_RD(&head.workers.list.lock);
    for(selfelem=head.workers.list.head.elements;selfelem;selfelem=selfelem->next) {
      thread = (pthread_t)selfelem->data;
      if(thread == self) break;
    }
    CF_RW_UN(&head.workers.list.lock);
  } while(selfelem == NULL);
  /* }}} */


  while(run) {
    /*
     * First, we look, if there are already entries in the queque. Therefore
     * we have to lock it.
     */
    CF_LM(&head.clients.lock);

    while(!head.clients.list.elements && run) {
      /*  ok, no clients seem to exist in the queque. Lets wait for them. */
      CF_UM(&head.clients.lock);

      timeout.tv_sec = time(NULL) + 5;
      timeout.tv_nsec = 0;

      if(cf_cond_timedwait(&head.clients.cond,&timeout) != 0) {
        /* we got a timeout; go and try it again */
        CF_LM(&head.clients.lock);
        continue;
      }

      /* regular signal, go and look for client */
      CF_LM(&head.clients.lock);
    }

    if(run) {
      lclient = head.clients.list.elements;

      if(lclient) {
        head.clients.list.elements = lclient->next;
        head.clients.num--;

        if(!head.clients.list.elements) head.clients.list.last = NULL;

        CF_UM(&head.clients.lock);

        client = (t_cf_client *)lclient->data;
        if(client->worker) client->worker(client->sock);
        else {
          cf_log(CF_ERR,__FILE__,__LINE__,"Client without worker?!\n");
          close(client->sock);
        }

        free(client);
        free(lclient);
      }
      else CF_UM(&head.clients.lock);
    }
    else CF_UM(&head.clients.lock);
  }

  return NULL;

}
/* }}} */

/* {{{ cf_setup_socket */
int cf_setup_socket(struct sockaddr_un *addr) {
  int sock;
  t_name_value *sockpath = cfg_get_first_value(&fo_default_conf,NULL,"SocketName");

  if((sock = socket(AF_LOCAL,SOCK_STREAM,0)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"socket: %s\n",strerror(errno));
    RUN = 0;
    return sock;
  }

  unlink(sockpath->values[0]);

  memset(addr,0,sizeof(*addr));
  addr->sun_family = AF_LOCAL;
  strncpy(addr->sun_path,sockpath->values[0],104);

  if(bind(sock,(struct sockaddr *)addr,sizeof(*addr)) < 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"bind(%s): %s\n",sockpath->values[0],strerror(errno));
    RUN = 0;
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"listen: %s\n",strerror(errno));
    RUN = 0;
    return -1;
  }

  /*
   * we don't care for errors in this chmod() call; if it
   * fails, the administrator has to do it by hand
   */
  chmod(sockpath->values[0],S_IRWXU|S_IRWXG|S_IRWXO);

  return sock;
}
/* }}} */

/* {{{ cf_push_server */
void cf_push_server(int sockfd,struct sockaddr *addr,size_t size,t_worker handler) {
  t_server srv;

  srv.sock   = sockfd;
  srv.size   = size;
  srv.worker = handler;
  srv.addr   = memdup(addr,size);

  CF_LM(&head.servers.lock);
  cf_list_append(&head.servers.list,&srv,sizeof(srv));
  CF_UM(&head.servers.lock);
}
/* }}} */

/* {{{ cf_push_client */
int cf_push_client(int connfd,t_worker handler,int spare_threads,int max_threads,pthread_attr_t *attr) {
  int status,num;
  pthread_t thread;
  t_cf_client client;

  client.worker = handler;
  client.sock   = connfd;

  /*
   * we don't care for access synchronization of head.workers.num
   * 'cause we are the only one accessing this value
   */

  CF_LM(&head.clients.lock);

  /* {{{ are there enough workers? */
  while(head.workers.num < head.clients.num + spare_threads && head.workers.num < max_threads) {
    if((status = pthread_create(&thread,attr,cf_worker,NULL)) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"pthread_create: %s\n",strerror(status));
      RUN = 0;
      return -1;
    }

    cf_rw_list_append(&head.workers.list,&thread,sizeof(thread));
    cf_log(CF_STD,__FILE__,__LINE__,"created worker %d now!\n",++head.workers.num);
  }
  /* }}} */

  /* {{{ are we in very high traffic? */
  if(head.clients.num >= max_threads) {
    CF_UM(&head.clients.lock);

    cf_log(CF_STD,__FILE__,__LINE__,"handling request directly...\n");
    handler(connfd);
  }
  /* }}} */
  else {
    cf_list_append(&head.clients.list,&client,sizeof(client));
    num = ++head.clients.num;

    CF_UM(&head.clients.lock);

    /*
     * we broadcast instead of signaling to get more than one worker
     * working
     */
    cf_cond_broadcast(&head.clients.cond);

    /* uh, high traffic, go sleeping for 20ms (this gives the workers time to work) */
    if(num >= max_threads - spare_threads * 2) usleep(20);
  }

  return 0;
}
/* }}} */

/* {{{ cf_register_protocol_handler */
int cf_register_protocol_handler(u_char *handler_hook,t_server_protocol_handler handler) {
  CF_RW_WR(&head.lock);

  if(head.protocol_handlers == NULL) {
    if((head.protocol_handlers = cf_hash_new(NULL)) == NULL) {
      cf_log(CF_ERR,__FILE__,__LINE__,"cf_hash_new: %s\n",strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if(cf_hash_set_static(head.protocol_handlers,handler_hook,strlen(handler_hook),handler) == 0) {
    cf_log(CF_ERR,__FILE__,__LINE__,"cf_hash_set: %s\n",strerror(errno));
    return -1;
  }

  CF_RW_UN(&head.lock);

  return 0;
}
/* }}} */

/* {{{ cf_register_thread */
void cf_register_thread(t_forum *forum,t_thread *t) {
  CF_RW_WR(&forum->threads.lock);
  if(!forum->threads.threads) forum->threads.threads = cf_hash_new(NULL);

  cf_hash_set(forum->threads.threads,(u_char *)&t->tid,sizeof(t->tid),&t,sizeof(t));

  CF_RW_UN(&forum->threads.lock);
}
/* }}} */

/* {{{ cf_unregister_thread */
void cf_unregister_thread(t_forum *forum,t_thread *t) {
  cf_log(CF_DBG,__FILE__,__LINE__,"unregistering thread %llu...\n",t->tid);

  CF_RW_WR(&forum->threads.lock);
  cf_hash_entry_delete(forum->threads.threads,(u_char *)&t->tid,sizeof(t->tid));
  CF_RW_UN(&forum->threads.lock);
}
/* }}} */

/* {{{ cf_remove_thread */
void cf_remove_thread(t_forum *forum,t_thread *t) {
  CF_RW_WR(&t->lock);
  if(t->prev) {
    CF_RW_WR(&t->prev->lock);
    t->prev->next = t->next;

    if(t->next) {
      CF_RW_WR(&t->next->lock);
      t->next->prev = t->prev;
      CF_RW_UN(&t->next->lock);
    }

    CF_RW_UN(&t->prev->lock);
  }
  else {
    CF_RW_WR(&forum->threads.lock);
    forum->threads.list = t->next;

    if(t->next) {
      CF_RW_WR(&t->next->lock);
      t->next->prev = NULL;
      CF_RW_UN(&t->next->lock);
    }

    CF_RW_UN(&forum->threads.lock);
  }
  CF_RW_UN(&t->lock);
}
/* }}} */

/* {{{ cf_tokenize */
int cf_tokenize(u_char *line,u_char ***tokens) {
  int n = 0,reser = 5;
  register u_char *ptr,*prev;

  *tokens = fo_alloc(NULL,5,sizeof(*tokens),FO_ALLOC_MALLOC);

  for(prev=ptr=line;*ptr;ptr++) {
    if(n >= reser) {
      reser += 5;
      *tokens = fo_alloc(*tokens,reser,sizeof(*tokens),FO_ALLOC_REALLOC);
    }

    if(isspace(*ptr) || *ptr == ':') {
      if(ptr - 1 == prev || ptr == prev) {
        prev = ptr;
        continue;
      }

      *ptr = 0;
      (*tokens)[n++] = memdup(prev,ptr-prev+1);
      prev = ptr+1;
    }
  }

  if(prev != ptr && prev-1 != ptr && *ptr) (*tokens)[n++] = memdup(prev,ptr-prev);

  return n;
}
/* }}} */

/* {{{ cf_handle_request */
void cf_cftp_handler(int sockfd) {
  int           shallRun = 1,i;
  rline_t      *tsd        = fo_alloc(NULL,1,sizeof(*tsd),FO_ALLOC_CALLOC);
  u_char *line  = NULL,**tokens;
  int   locked = 0,tnum = 0;
  t_forum *forum = NULL;
  t_server_protocol_handler handler;
  int ret;


  while(shallRun) {
    line = readline(sockfd,tsd);
    cf_log(CF_DBG,__FILE__,__LINE__,"%s",line?line:(u_char *)"(NULL)\n");

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
            CF_RW_RD(&head.lock);
            forum = cf_hash_get(head.forums,tokens[1],strlen(tokens[1]));
            CF_RW_UN(&head.lock);

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
              cf_log(CF_ERR,__FILE__,__LINE__,"declined archivation because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            if(tnum < 3) {
              writen(sockfd,"500 Sorry\n",10);
              cf_log(CF_ERR,__FILE__,__LINE__,"Bad request\n");
            }
            else {
              tid      = strtoull(tokens[2]+1,NULL,10);
              cf_log(CF_ERR,__FILE__,__LINE__,"archiving thread %lld by user %s",tid,ln);

              writen(sockfd,"200 Ok\n",7);

              cf_archive_thread(forum,tid);
              cf_generate_cache(forum);
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ delete a thread */
          else if(cf_strcmp(tokens[0],"DELETE") == 0) {
            u_int64_t tid,mid;
            t_thread *t;
            t_posting *p;
            int lvl;
            u_char *ln = readline(sockfd,tsd);

            if(ln == NULL) {
              cf_log(CF_ERR,__FILE__,__LINE__,"declined deletion because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            else if(tnum != 3) {
              writen(sockfd,"501 Thread id or message id missing\n",36);
            }
            else {
              tid = strtoull(tokens[1]+1,NULL,10);
              mid = strtoull(tokens[2]+1,NULL,10);

              t = cf_get_thread(forum,tid);

              if(!t) {
                writen(sockfd,"404 Thread Not Found\n",21);
                cf_log(CF_ERR,__FILE__,__LINE__,"Thread not found\n");
              }
              else {
                p = cf_get_posting(t,mid);

                if(!p) {
                  writen(sockfd,"404 Message Not Found\n",22);
                  cf_log(CF_ERR,__FILE__,__LINE__,"Message not found\n");
                }
                else {
                  cf_log(CF_ERR,__FILE__,__LINE__,"Deleted posting %lld in thread %lld by user %s",mid,tid,ln);

                  CF_RW_WR(&t->lock);

                  lvl          = p->level;
                  p->invisible = 1;

                  for(p=p->next;p && p->level > lvl;p=p->next) {
                    p->invisible = 1;
                  }

                  writen(sockfd,"200 Ok\n",7);

                  CF_RW_UN(&t->lock);

                  /* we need new caches if a message has been deleted */
                  cf_generate_cache(forum);
                }
              }
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ undelete a thread */
          else if(cf_strcmp(tokens[0],"UNDELETE") == 0) {
            u_int64_t tid,mid;
            t_thread *t;
            t_posting *p;
            int lvl;
            u_char *ln = readline(sockfd,tsd);

            if(ln == NULL) {
              cf_log(CF_ERR,__FILE__,__LINE__,"declined undelete because user name not present\n");
              writen(sockfd,"403 Access Denied\n",18);
            }
            if(tnum < 3) {
              writen(sockfd,"501 Thread id or message id missing\n",36);
            }
            else {
              tid = strtoull(tokens[1]+1,NULL,10);
              mid = strtoull(tokens[2]+1,NULL,10);

              t = cf_get_thread(forum,tid);

              if(!t) {
                writen(sockfd,"404 Thread Not Found\n",21);
                cf_log(CF_ERR,__FILE__,__LINE__,"Thread not found\n");
              }
              else {
                p = cf_get_posting(t,mid);
                cf_log(CF_ERR,__FILE__,__LINE__,"Undelete posting %lld in posting %lld by user %lld\n",tid,mid,ln);

                if(!p) {
                  writen(sockfd,"404 Message Not Found\n",22);
                  cf_log(CF_ERR,__FILE__,__LINE__,"Message not found\n");
                }
                else {
                  CF_RW_WR(&t->lock);

                  lvl          = p->level;
                  p->invisible = 0;

                  for(p=p->next;p && p->level > lvl;p=p->next) p->invisible = 0;

                  CF_RW_UN(&t->lock);

                  writen(sockfd,"200 Ok\n",7);

                  /* we need new caches if a message has been undeleted */
                  cf_generate_cache(forum);
                }
              }
            }

            if(ln) free(ln);
          }
          /* }}} */
          /* {{{ unlock a forum */
          else if(cf_strcmp(line,"UNLOCK") == 0) {
            CF_RW_WR(&forum->lock);
            forum->locked = 0;
            CF_RW_UN(&forum->lock);

            writen(sockfd,"200 Ok\n",7);
          }
          /* }}} */
          /* {{{ lock forum */
          else if(cf_strcmp(tokens[0],"LOCK") == 0) {
            CF_RW_WR(&forum->lock);
            forum->locked = 1;
            CF_RW_UN(&forum->lock);

            writen(sockfd,"200 Ok\n",7);
          }
          /* }}} */
          /* {{{ everything else may only be done when the forum is *not* locked */
          else {
            CF_RW_RD(&forum->lock);
            locked = forum->locked;
            CF_RW_UN(&forum->lock);

            if(locked == 0) {
              /* {{{ handler plugin or 500 */
              CF_RW_RD(&head.lock);

              if(head.protocol_handlers) {
                CF_RW_UN(&head.lock);

                handler = cf_hash_get(head.protocol_handlers,tokens[0],strlen(tokens[0]));
                if(handler == NULL) writen(sockfd,"500 What's up?\n",15);
                else {
                  ret = handler(sockfd,(const u_char *)forum,(const u_char **)tokens,tnum,tsd);
                  if(ret == FLT_DECLINE) writen(sockfd,"500 What's up?\n",15);
                }
              }
              else {
                CF_RW_UN(&head.lock);
                writen(sockfd,"500 What's up?\n",15);
              }
              /* }}} */
            }
            else writen(sockfd,"600 Forum locked\n",17);
          }
          /* }}} */
        }
        /* }}} */
        else {
          writen(sockfd,"508 No forum selected\n",22);
        }

        free(line);
        line = NULL;

        for(i=0;i<tnum;i++) free(tokens[i]);
        free(tokens);
      }
      else {
        writen(sockfd,"500 What's up?\n",15);
      }
    }
    else {
      shallRun = 0; /* connection broke */
    }
  }

  if(line) free(line);

  free(tsd);
  close(sockfd);
}
/* }}} */

/* {{{ cf_get_posting */
t_posting *cf_get_posting(t_thread *t,u_int64_t mid) {
  t_posting *p;

  CF_RW_RD(&t->lock);

  for(p=t->postings;p && p->mid != mid;p=p->next);

  CF_RW_UN(&t->lock);

  return p;  /* this returns NULL or the posting */
}
/* }}} */

/* {{{ cf_get_thread */
t_thread *cf_get_thread(t_forum *forum,u_int64_t tid) {
  t_thread **t = NULL;

  CF_RW_RD(&forum->threads.lock);
  if(forum->threads.threads) t = cf_hash_get(forum->threads.threads,(u_char *)&tid,sizeof(tid));
  CF_RW_UN(&forum->threads.lock);

  return t ? *t : NULL;
}
/* }}} */

/* {{{ cf_destroy_flag */
void cf_destroy_flag(void *data) {
  t_posting_flag *flag = (t_posting_flag *)data;
  free(flag->name);
  free(flag->val);
}
/* }}} */

/* {{{ cf_generate_cache */
void *cf_generate_cache(void *arg) {
  t_forum *forum = (t_forum *)arg;
  t_name_value *forums;
  size_t i = 0;

#ifndef CF_SHARED_MEM
  t_string str1,str2;

  /* {{{ make cache for only one forum */
  if(forum) {
    str_init(&str1);
    str_init(&str2);

    cf_generate_list(forum,&str1,0);
    cf_generate_list(forum,&str2,1);

    CF_RW_WR(&forum->lock);

    if(forum->cache.invisible.content) free(forum->cache.invisible.content);
    if(forum->cache.visible.content) free(forum->cache.visible.content);

    forum->cache.invisible.content = str2.content;
    forum->cache.visible.content   = str1.content;
    forum->cache.invisible.len     = str2.len;
    forum->cache.visible.len       = str1.len;

    forum->date.visible            = time(NULL);
    forum->date.invisible          = time(NULL);

    forum->fresh = 1;

    CF_RW_UN(&forum->lock);
  }
  /* }}} */
  /* {{{ make cache for all forums */
  else {
    forums = cfg_get_first_value(&fo_server_conf,"Forums");

    for(i=0;i<forums->vallen;i++) {
      if((forum = cf_hash_get(head.forums,forums->values[i],strlen(forums->values[i]))) != NULL) {
        str_init(&str1);
        str_init(&str2);

        cf_generate_list(forum,&str1,0);
        cf_generate_list(forum,&str2,1);

        CF_RW_WR(&forum->lock);

        if(forum->cache.invisible.content) free(forum->cache.invisible.content);
        if(forum->cache.visible.content) free(forum->cache.visible.content);

        forum->cache.invisible.content = str2.content;
        forum->cache.visible.content   = str1.content;
        forum->cache.invisible.len     = str2.len;
        forum->cache.visible.len       = str1.len;

        forum->date.visible            = time(NULL);
        forum->date.invisible          = time(NULL);

        forum->fresh = 1;

        CF_RW_UN(&forum->lock);
      }
    }
  }
  /* }}} */
  #else
  if(forum) cf_generate_shared_memory(forum);
  else {
    forums = cfg_get_first_value(&fo_server_conf,NULL,"Forums");

    for(i=0;i<forums->valnum;i++) {
      if((forum = cf_hash_get(head.forums,forums->values[i],strlen(forums->values[i]))) != NULL) cf_generate_shared_memory(forum);
    }
  }
  #endif

  return NULL;
}
/* }}} */

/* {{{ cf_generate_list */
void cf_generate_list(t_forum *forum,t_string *str,int del) {
  int n;
  u_char buff[500];
  t_thread *t,*t1;
  int first;
  t_posting *p;

  str_chars_append(str,"200 Ok\n",7);

  CF_RW_RD(&forum->threads.lock);
  t1 = forum->threads.list;
  CF_RW_UN(&forum->threads.lock);

  while(t1) {
    first = 1;

    t = t1;

    CF_RW_RD(&t->lock);

    for(p = t->postings;p;p = p->next) {
      if(p->invisible && !del) {
        for(;p && p->invisible;p=p->next);
        if(!p) break;
      }


      /* thread/posting header */
      if(first) {
        first = 0;
        str_chars_append(str,"THREAD t",8);
        u_int64_to_str(str,t->tid);
      }
      else str_chars_append(str,"MSG m",5);

      u_int64_to_str(str,p->mid);
      str_char_append(str,'\n');

      /* author */
      str_chars_append(str,"Author:",7);
      str_chars_append(str,p->user.name.content,p->user.name.len);

      /* subject */
      str_chars_append(str,"\nSubject:",9);
      str_chars_append(str,p->subject.content,p->subject.len);

      /* category */
      if(p->category.len) {
        str_chars_append(str,"\nCategory:",10);
        str_chars_append(str,p->category.content,p->category.len);
      }

      /* date */
      n = snprintf(buff,256,"\nDate:%ld\n",p->date);
      str_chars_append(str,buff,n);

      /* level */
      n = snprintf(buff,256,"Level:%d\n",p->level);
      str_chars_append(str,buff,n);

      n = snprintf(buff,256,"Visible:%d\n",p->invisible == 0);
      str_chars_append(str,buff,n);
    }

    str_chars_append(str,"END\n",4);

    t1   = t->next;
    CF_RW_UN(&t->lock);
  }

  str_char_append(str,'\n');
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
int cf_shmdt(void *ptr) {
  cf_log(CF_DBG,__FILE__,__LINE__,"shmdt: detaching %p\n",ptr);
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
void *cf_shmat(int shmid,void *addr,int shmflag) {
  void *ptr = shmat(shmid,addr,shmflag);

  cf_log(CF_DBG,__FILE__,__LINE__,"shmat: attatching segment %d (%p)\n",shmid,ptr);

  return ptr;
}
/* }}} */

/* {{{ cf_generate_shared_memory */
void cf_generate_shared_memory(t_forum *forum) {
  t_mem_pool pool;
  t_thread *t,*t1;
  t_posting *p;
  t_name_value *v = cfg_get_first_value(&fo_default_conf,forum->name,"SharedMemIds");
  u_int32_t val;
  time_t tm = time(NULL);
  unsigned short semval;

  mem_init(&pool);

  CF_RW_RD(&forum->threads.lock);
  t1 = t = forum->threads.list;
  CF_RW_UN(&forum->threads.lock);

  /*
   * {{{ CAUTION! Deep magic begins here!
   */

  mem_append(&pool,&tm,sizeof(t));

  for(;t;t=t1) {
    CF_RW_RD(&t->lock);

    /* we only need thread id and postings */
    mem_append(&pool,&(t->tid),sizeof(t->tid));
    mem_append(&pool,&(t->posts),sizeof(t->posts));

    for(p=t->postings;p;p=p->next) {
      mem_append(&pool,&(p->mid),sizeof(p->mid));

      val = p->subject.len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->subject.content,val);

      val = p->category.len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->category.content,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->content.len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->content.content,val);

      mem_append(&pool,&(p->date),sizeof(p->date));
      mem_append(&pool,&(p->level),sizeof(p->level));
      mem_append(&pool,&(p->invisible),sizeof(p->invisible));

      val = p->user.name.len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->user.name.content,val);

      val = p->user.email.len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.email.content,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.hp.len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.hp.content,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.img.len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.img.content,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }
    }

    t1 = t->next;
    CF_RW_UN(&t->lock);
    t = t1;
  }

  /*
   * Phew. Deep magic ends
   * }}}
   */


  /* lets go surfin' */
  CF_LM(&forum->shm.lock);

  if(cf_sem_getval(forum->shm.sem,0,1,&semval) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"cf_sem_getval: %s\n",strerror(errno));
    exit(-1);
  }

  /* semval contains now the number of the shared memory segment *not* used */
  if(semval != 0 && semval != 1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"hu? what happened? semval is %d\n",semval);
    semval = 0;
  }

  cf_log(CF_DBG,__FILE__,__LINE__,"shm_ids[%d]: %ld\n",semval,forum->shm.ids[semval]);

  /* does the segment already exists? */
  if(forum->shm.ids[semval] != -1) {
    /* yeah, baby, yeah! */

    cf_log(CF_DBG,__FILE__,__LINE__,"shm_ptrs[%d]: %p\n",semval,forum->shm.ptrs[semval]);

    /* oh behave! detach the memory */
    if(forum->shm.ptrs[semval]) {
      if(cf_shmdt(forum->shm.ptrs[semval]) != 0) {
        cf_log(CF_ERR,__FILE__,__LINE__,"shmdt: %s (semval: %d)\n",strerror(errno),semval);
        CF_UM(&forum->shm.lock);
        mem_cleanup(&pool);
        return;
      }
    }

    /* delete the segment */
    if(shmctl(forum->shm.ids[semval],IPC_RMID,NULL) != 0) {
      cf_log(CF_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
      CF_UM(&forum->shm.lock);
      mem_cleanup(&pool);
      return;
    }
  }

  if((forum->shm.ids[semval] = shmget(atoi(v->values[semval]),pool.len,IPC_CREAT|CF_SHARED_MODE)) == -1) {
    cf_log(CF_ERR,__FILE__,__LINE__,"shmget: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  if((forum->shm.ptrs[semval] = cf_shmat(forum->shm.ids[semval],NULL,0)) == NULL) {
    cf_log(CF_ERR,__FILE__,__LINE__,"shmat: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  memcpy(forum->shm.ptrs[semval],pool.content,pool.len);

  mem_cleanup(&pool);

  if(semval == 1) CF_SEM_DOWN(forum->shm.sem,0);
  else CF_SEM_UP(forum->shm.sem,0);
  CF_UM(&forum->shm.lock);

  cf_log(CF_DBG,__FILE__,__LINE__,"generated shared memory segment %d\n",semval);
}
/* }}} */
#endif

/* eof */

