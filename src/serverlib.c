/**
 * \file serverlib.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Server library functions and datatypes
 *
 * This file contains the functions used by the server
 * program and the server plugins, so it contains the
 * server library.
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
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include <gdome.h>

#ifdef CF_SHARED_MEM
#  ifdef MAX
#    undef MAX /* max has been defined in the glib */
#  endif
#  ifdef MIN
#    undef MIN /* min has been defined in the glib */
#  endif

#  include <sys/param.h>
#  include <sys/ipc.h>
#  include <sys/shm.h>
#  include <sys/sem.h>
#endif

#ifdef CF_SHARED_MEM
#  include "semaphores.h"
#  include "shm_locking.h"
#endif

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"
#include "fo_server.h"
#include "serverlib.h"
#include "xml_handling.h"
#include "charconvert.h"
#include "archiver.h"

/* }}} */

/* {{{ cf_register_protocol_handler */
int cf_register_protocol_handler(u_char *handler_hook,t_server_protocol_handler handler) {
  CF_RW_WR(&head.lock);

  if(head.protocol_handlers == NULL) {
    if((head.protocol_handlers = cf_hash_new(NULL)) == NULL) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"cf_hash_new: %s\n",strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if(cf_hash_set_static(head.protocol_handlers,handler_hook,strlen(handler_hook),handler) == 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"cf_hash_set: %s\n",strerror(errno));
    return -1;
  }

  CF_RW_UN(&head.lock);

  return 0;
}
/* }}} */

/* {{{ cf_register_thread */
void cf_register_thread(t_thread *t) {
  u_char buff[50];
  size_t len;

  len = snprintf(buff,50,"t%lld",t->tid);

  CF_RW_WR(&head.threads_lock);

  if(!head.threads) head.threads = cf_hash_new(NULL);

  cf_hash_set(head.threads,buff,len,&t,sizeof(t));

  CF_RW_UN(&head.threads_lock);
}
/* }}} */

/* {{{ cf_unregister_thread */
void cf_unregister_thread(t_thread *t) {
  u_char buff[50];
  size_t len;

  len = snprintf(buff,50,"t%lld",t->tid);

  cf_log(LOG_DBG,__FILE__,__LINE__,"unregistering thread %lld...\n",t->tid);

  CF_RW_WR(&head.threads_lock);

  cf_hash_entry_delete(head.threads,buff,len);

  CF_RW_UN(&head.threads_lock);
}
/* }}} */

/* {{{ cf_push_server
 * Returns:    -1 on failure, 0 on success
 * Parameters:
 *   - int sockfd             The server socket
 *   - struct sockaddr *addr  The address structure for the socket
 *   - int size               The size of the address structure
 *   - t_worker handler       The handler function for this server socket
 *
 * This function inserts a server socket into the server queque
 */
int cf_push_server(int sockfd,struct sockaddr *addr,int size,t_worker handler) {
  t_server *cl;

  CF_LM(&head.server_lock);

  if(!head.servers) {
    head.servers         = fo_alloc(NULL,1,sizeof(t_server),FO_ALLOC_MALLOC);
    head.servers->worker = handler;
    head.servers->sock   = sockfd;
    head.servers->addr   = addr;
    head.servers->size   = size;
    head.servers->next   = NULL;
  }
  else {
    for(cl=head.servers;cl->next;cl=cl->next) {
      if(cl->sock == sockfd) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"push_server: plugin tried to register server socket twicely (sock %d)\n",sockfd);
        CF_UM(&head.server_lock);
        return -1;
      }
    }

    if(cl->sock == sockfd) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"push_server: plugin tried to register server socket twicely (sock %d)\n",sockfd);
      CF_UM(&head.server_lock);
      return -1;
    }

    cl->next         = fo_alloc(NULL,1,sizeof(t_server),FO_ALLOC_MALLOC);
    cl->next->sock   = sockfd;
    cl->next->worker = handler;
    cl->next->addr   = addr;
    cl->next->size   = size;
    cl->next->next   = NULL;
  }


  cf_log(LOG_STD,__FILE__,__LINE__,"push_server: registered server socket %d\n",sockfd);
  CF_UM(&head.server_lock);

  return 0;
}
/* }}} */

/* {{{ cf_push_client
 * Returns:    -1 on failure, 0 on success
 * Parameters:
 *   - int connfd         The connection descriptor
 *   - t_worker handler   the handler function
 *
 * This function inserts a client into the client queque
 */
int cf_push_client(int connfd,t_worker handler) {
  int status,
      percentage;

  /*
   * lock the queque
   */
  CF_LM(&head.clients.lock);

  /* get usage percentage */
  percentage = (100 * (head.clients.clientnum + 1)) / head.clients.workers;

  /*
   * check if there are enough workers; the percentage of
   * usage when to create new workers is defined in
   * fo_server.h
   */
  while(head.clients.workers == 0 || (percentage >= CREATE_WORKERS_NUM && head.clients.workers <= MAX_WORKERS_NUM)) {
    /* create workers if there are not enough */
    if((status = pthread_create(&head.workers[head.clients.workers],NULL,cf_worker,NULL)) != 0) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_create: %s\n",strerror(status));
      RUN = 0;
      return -1;
    }

    cf_log(LOG_STD,__FILE__,__LINE__,"created worker %d now!\n",++head.clients.workers);
    percentage = (100 * (head.clients.clientnum + 1)) / head.clients.workers;
  }

  /* is there very high traffic? */
  if(percentage > MAX_CLIENT_NUM) {
    CF_UM(&head.clients.lock);

    cf_log(LOG_STD,__FILE__,__LINE__,"handling request directly...\n");
    handler(connfd);
  }
  else {
    /* insert the client into the queque */
    if(!head.clients.last) {
      head.clients.last         = head.clients.clients = fo_alloc(NULL,1,sizeof(t_client),FO_ALLOC_CALLOC);
      head.clients.last->sock   = connfd;
      head.clients.last->worker = handler;
      head.clients.last->next   = NULL;
    }
    else {
      head.clients.last->next         = fo_alloc(NULL,1,sizeof(t_client),FO_ALLOC_MALLOC);
      head.clients.last->next->sock   = connfd;
      head.clients.last->next->worker = handler;
      head.clients.last->next->next   = NULL;
      head.clients.last               = head.clients.last->next;
    }

    head.clients.clientnum++;

    CF_UM(&head.clients.lock);

    /* lock the conditional */
    CF_LM(&head.clients.cond_lock);

    /* wake up workers */
    pthread_cond_signal(&head.clients.cond);

    /* unlock conditional */
    CF_UM(&head.clients.cond_lock);

    CF_LM(&head.clients.lock);

    /* check for priority settings */
    if(percentage > CLIENT_PRIORITY_NUM) {
      CF_UM(&head.clients.lock);

      cf_log(LOG_STD,__FILE__,__LINE__,"yielding server thread...\n");
      pthread_yield();
    }
    else {
      CF_UM(&head.clients.lock);
    }
  }

  return 0;
}
/* }}} */

/* {{{ cf_log
 * Returns: nothing
 * Parameters:
 *   - int mode            the mode to log (LOG_ERR or LOG_STD)
 *   - int line            the line number (0 if no number is given)
 *   - const u_char *format the format
 *   - ...                 the format args
 *
 * This function writes log entries to file
 *
 */
void cf_log(int mode,const u_char *file,int line,const u_char *format,...) {
  u_char str[300];
  int status;
  t_name_value *v;
  time_t t;
  struct tm *tm;
  int sz;
  va_list ap;
  register u_char *ptr,*ptr1;

#ifndef DEBUG
  if(mode == LOG_DBG) return;
#endif

  for(ptr1=ptr=(u_char *)file;*ptr;ptr++) {
    if(*ptr == '/') ptr1 = ptr + 1;
  }

  t  = time(NULL);
  tm = localtime(&t);

  if((status = pthread_mutex_lock(&head.log_lock)) != 0) {
    fprintf(stderr,"pthread_mutex_lock: %s\n",strerror(status));
    return;
  }

  if(!head.std) {
    v = cfg_get_first_value(&fo_server_conf,NULL,"StdLog");
    head.std = fopen(v->values[0],"a");
  }
  if(!head.err) {
    v = cfg_get_first_value(&fo_server_conf,NULL,"ErrorLog");
    head.err = fopen(v->values[0],"a");
  }

  sz = snprintf(str,300,"[%4d-%02d-%02d %02d:%02d:%02d]:%s:%d ",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec,ptr1,line);

  va_start(ap, format);
  if(mode == LOG_ERR) {
    fwrite(str,sz,1,head.err);
    vfprintf(head.err,format,ap);
    fflush(head.err);

    #ifdef DEBUG
    fwrite(str,sz,1,stderr);
    vfprintf(stderr,format,ap);
    #endif
  }
  else if(mode == LOG_STD) {
    fwrite(str,sz,1,head.std);
    vfprintf(head.std,format,ap);

    /*
     * we want fflush() only if debugging is enabled
     * if not, we want to avoid system calls so fflush()
     * is silly and buffering is ok (stdlog is not critical,
     * stdlog contains only non-critical infos)
     */
    #ifdef DEBUG
    fflush(head.std);

    fwrite(str,sz,1,stdout);
    vfprintf(stdout,format,ap);
    #endif
  }
  #ifdef DEBUG
  else {
    fwrite(str,sz,1,head.std);
    fwrite(str,sz,1,stdout);
    fwrite("DEBUG: ",7,1,head.std);
    fwrite("DEBUG: ",7,1,stdout);
    vfprintf(head.std,format,ap);
    vfprintf(stdout,format,ap);
    fflush(head.std);
    fflush(stdout);
  }
  #endif
  va_end(ap);

  pthread_mutex_unlock(&head.log_lock);
}
/* }}} */

/* {{{ cf_set_us_up_the_socket
 * Returns: int  the socket
 * Parameters:
 *   - struct sockaddr_un *addr    the sockaddr unix object
 *
 * this functions sets up the socket
 *
 */
int cf_set_us_up_the_socket(struct sockaddr_un *addr) {
  int sock;
  t_name_value *sockpath = cfg_get_first_value(&fo_default_conf,NULL,"SocketName");

  if(!sockpath) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"could not find socket path!\n");
    RUN = 0;
    return -1;
  }

  sock = socket(AF_LOCAL,SOCK_STREAM,0);

  if(sock == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"socket: %s\n",sock,strerror(errno));
    RUN = 0;
    return -1;
  }

  unlink(sockpath->values[0]);

  memset(addr,0,sizeof(struct sockaddr_un));
  addr->sun_family = AF_LOCAL;
  (void)strncpy(addr->sun_path,sockpath->values[0],104);

  if(bind(sock,(struct sockaddr *)addr,sizeof(struct sockaddr_un)) < 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"bind: %s\n",strerror(errno));
    RUN = 0;
    return -1;
  }

  if(listen(sock,LISTENQ) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"listen: %s\n",strerror(errno));
    RUN = 0;
    return -1;
  }

  chmod(sockpath->values[0],S_IRWXU|S_IRWXG|S_IRWXO);

  return sock;
}
/* }}} */

/* {{{ cf_worker
 * Returns: NULL
 * Parameters:
 *   - void *arg    will always be NULL
 *
 * this function is a worker
 *
 */
void *cf_worker(void *arg) {
  t_client *client;
  struct timespec timeout;
  int retries = 0,status;

  while(RUN) {
    /*
     * First, we look, if there are already entries in the queque. Therefore
     * we have to lock it.
     */
    CF_LM(&head.clients.lock);

    while(!head.clients.clients && RUN) {
      /*  ok, no clients seem to exist in the queque. Lets wait for them. */
      CF_UM(&head.clients.lock);

      /* lock conditional */
      CF_LM(&head.clients.cond_lock);

      /* wait time for the server (we don't trust the server, he could tell us to wait until neverneverday :) */
      timeout.tv_sec  = time(NULL) + 5; /* we wait 5 seconds maximum */
      timeout.tv_nsec = 0;

      /* ok, lets go sleeping... */
      if((status = pthread_cond_timedwait(&head.clients.cond,&head.clients.cond_lock.mutex,&timeout)) != 0) {
        CF_UM(&head.clients.cond_lock);

        /* uh, oh, fucking error or timeout? */
        if(status != ETIMEDOUT)  {
          cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_cond_timedwait: %s. We're going to retry it...\n",strerror(status));
          retries++;
        }
        else {
          retries = 0;
        }

        CF_LM(&head.clients.lock);

        if(retries < 3) {
          continue;
        }
        else {
          cf_log(LOG_ERR,__FILE__,__LINE__,"Worker tried it three times, and he got three times an error. He goes down...\n");
          head.clients.workers--;
          CF_UM(&head.clients.lock);
          return NULL;
        }
      }

      /* Lock no longer needed */
      CF_UM(&head.clients.cond_lock);

      /* we got an regular signal. Lock the queque and look, what happened. */
      CF_LM(&head.clients.lock);
    }

    /* shall we still run? */
    if(RUN) {
      /* ah, there seems to be something in the queque */
      client = head.clients.clients;

      if(client) {
        head.clients.clients = client->next;
        head.clients.clientnum--;

        if(!head.clients.clients) head.clients.last = NULL;

        CF_UM(&head.clients.lock);

        if(client->worker) {
          client->worker(client->sock);
        }
        else {
          /* uups. No worker defined. Waste the client... */
          cf_log(LOG_ERR,__FILE__,__LINE__,"Wasting client?!\n");
          close(client->sock);
        }

        free(client);
      }
      else {
        /* hu? What's going on? */
        CF_UM(&head.clients.lock);
      }
    }
    else {
      CF_UM(&head.clients.lock);
    }
  }

  return NULL;
}
/* }}} */

/* {{{ cf_tokenize */
int cf_tokenize(u_char *line,u_char ***tokens) {
  int n = 0,reser = 5;
  register u_char *ptr,*prev;

  *tokens = malloc(5 * sizeof(u_char **));
  if(!*tokens) return 0;

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

/* {{{ cf_handle_request
 * Returns: NULL
 * Parameters:
 *   - void *arg    This argument will be converted to
 *                  integer. It is the connection socket.
 *
 * this function handles a connection
 *
 */
void cf_handle_request(int sockfd) {
  int           shallRun = 1,i;
  rline_t      *tsd        = fo_alloc(NULL,1,sizeof(*tsd),FO_ALLOC_CALLOC);
  u_char *line  = NULL,**tokens;
  int   locked = 0,tnum = 0;

  while(shallRun) {
    line = readline(sockfd,tsd);
    cf_log(LOG_DBG,__FILE__,__LINE__,"%s",line?line:(u_char *)"(NULL)\n");

    if(line) {
      tnum = cf_tokenize(line,&tokens);

      if(tnum) {
        if(cf_strcmp(tokens[0],"QUIT") == 0) {
          if(tnum == 2) {
            if(cf_strcmp(tokens[1],"SERVER") == 0) {
              shallRun = 0;
              RUN      = 0;

              writen(sockfd,"200 Bye, bye\n",13);
            }
            else {
              writen(sockfd,"500 What do you mean?\n",22);
            }
          }
          else {
            shallRun = 0;
            writen(sockfd,"200 Bye, bye\n",13);
          }
        }
        else if(cf_strcmp(tokens[0],"ARCHIVE") == 0) {
          u_int64_t tid;
          u_char *ln = readline(sockfd,tsd);

          if(ln == NULL) {
            cf_log(LOG_ERR,__FILE__,__LINE__,"declined archivation because user name not present\n");
            writen(sockfd,"403 Access Denied\n",18);
          }
          if(tnum < 3) {
            writen(sockfd,"500 Sorry\n",10);
            cf_log(LOG_ERR,__FILE__,__LINE__,"Bad request\n");
          }
          else {
            tid      = strtoull(tokens[2]+2,NULL,10);
            cf_log(LOG_ERR,__FILE__,__LINE__,"archiving thread %lld by user %s",tid,ln);

            cf_archive_thread(sockfd,tid);
            cf_generate_cache(NULL);
          }

          if(ln) free(ln);
        }
        else if(cf_strcmp(tokens[0],"DELETE") == 0) {
          u_int64_t tid,mid;
          t_thread *t;
          t_posting *p;
          int lvl;
          u_char *ln = readline(sockfd,tsd);

          if(ln == NULL) {
            cf_log(LOG_ERR,__FILE__,__LINE__,"declined deletion because user name not present\n");
            writen(sockfd,"403 Access Denied\n",18);
          }
          else if(tnum != 3) {
            writen(sockfd,"501 Thread id or message id missing\n",36);
          }
          else {
            tid = strtoull(tokens[1]+2,NULL,10);
            mid = strtoull(tokens[2]+2,NULL,10);

            t = cf_get_thread(tid);

            if(!t) {
              writen(sockfd,"404 Thread Not Found\n",21);
              cf_log(LOG_ERR,__FILE__,__LINE__,"Thread not found\n");
            }
            else {
              p = cf_get_posting(t,mid);

              if(!p) {
                writen(sockfd,"404 Message Not Found\n",22);
                cf_log(LOG_ERR,__FILE__,__LINE__,"Message not found\n");
              }
              else {
                cf_log(LOG_ERR,__FILE__,__LINE__,"Deleted posting %lld in thread %lld by user %s",mid,tid,ln);

                CF_RW_WR(&t->lock);

                lvl          = p->level;
                p->invisible = 1;

                for(p=p->next;p && p->level > lvl;p=p->next) {
                  p->invisible = 1;
                }

                writen(sockfd,"200 Ok\n",7);

                CF_RW_UN(&t->lock);

                /* we need new caches if a message has been deleted */
                cf_generate_cache(NULL);
              }
            }
          }

          if(ln) free(ln);
        }
        else if(cf_strcmp(tokens[0],"UNDELETE") == 0) {
          u_int64_t tid,mid;
          t_thread *t;
          t_posting *p;
          int lvl;
          u_char *ln = readline(sockfd,tsd);

          if(ln == NULL) {
            cf_log(LOG_ERR,__FILE__,__LINE__,"declined undelete because user name not present\n");
            writen(sockfd,"403 Access Denied\n",18);
          }
          if(tnum < 3) {
            writen(sockfd,"501 Thread id or message id missing\n",36);
          }
          else {
            tid = strtoull(tokens[1]+2,NULL,10);
            mid = strtoull(tokens[2]+2,NULL,10);

            t = cf_get_thread(tid);

            if(!t) {
              writen(sockfd,"404 Thread Not Found\n",21);
              cf_log(LOG_ERR,__FILE__,__LINE__,"Thread not found\n");
            }
            else {
              p = cf_get_posting(t,mid);
              cf_log(LOG_ERR,__FILE__,__LINE__,"Undelete posting %lld in posting %lld by user %lld\n",tid,mid,ln);

              if(!p) {
                writen(sockfd,"404 Message Not Found\n",22);
                cf_log(LOG_ERR,__FILE__,__LINE__,"Message not found\n");
              }
              else {
                CF_RW_WR(&t->lock);

                lvl          = p->level;
                p->invisible = 0;

                for(p=p->next;p && p->level > lvl;p=p->next) {
                  p->invisible = 0;
                }

                writen(sockfd,"200 Ok\n",7);

                CF_RW_UN(&t->lock);

                /* we need new caches if a message has been undeleted */
                cf_generate_cache(NULL);
              }
            }
          }

          if(ln) free(ln);
        }
        else if(cf_strcmp(line,"UNLOCK") == 0) {
          if(tnum == 2 && cf_strcmp(tokens[1],"FORUM") == 0) {
            CF_RW_WR(&head.lock);
            head.locked = 0;
            CF_RW_UN(&head.lock);
          }
          else {
            writen(sockfd,"500 What's up?\n",15);
          }
        }
        else { /* everything else may only be done when the forum is *not* locked */
          CF_RW_RD(&head.lock);
          locked = head.locked;
          CF_RW_UN(&head.lock);

          if(locked == 0) {
            if(cf_strcmp(tokens[0],"LOCK") == 0) {
              if(tnum == 2 && cf_strcmp(tokens[1],"FORUM") == 0) {
                CF_RW_WR(&head.lock);
                head.locked = 1;
                CF_RW_UN(&head.lock);
              }
              else {
                writen(sockfd,"500 What's up?\n",15);
              }
            }
            else {
              /* run handlers */
              t_server_protocol_handler handler;
              int ret;

              CF_RW_RD(&head.lock);

              if(head.protocol_handlers) {
                CF_RW_UN(&head.lock);

                handler = cf_hash_get(head.protocol_handlers,tokens[0],strlen(tokens[0]));
                if(handler == NULL) {
                  writen(sockfd,"500 What's up?\n",15);
                }
                else {
                  ret = handler(sockfd,(const u_char **)tokens,tnum,tsd);

                  if(ret == FLT_DECLINE) {
                    writen(sockfd,"500 What's up?\n",15);
                  }
                }
              }
              else {
                writen(sockfd,"500 What's up?\n",15);
                CF_RW_UN(&head.lock);
              }
            }
          }
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

/* {{{ cf_read_posting
 * Returns: nothing
 * Parameters:
 *   - t_posting *p  the posting structure
 *   - int sock      the socket of the connection
 *
 * This function reads a posting from a client
 *
 */
int cf_read_posting(t_posting *p,int sock,rline_t *tsd) {
  u_char *line = NULL;
  unsigned long llen;
  p->user.ip = NULL;

  do {
    line = readline(sock,tsd);

    if(line) {
      llen = tsd->rl_len;
      line[llen-1] = '\0';

      cf_log(LOG_DBG,__FILE__,__LINE__,"read_posting: got line %s\n",line);

      if(cf_strncmp(line,"Unid:",5) == 0) {
        p->unid     = strdup(line+6);
        p->unid_len = llen - 7;
      }
      else if(cf_strncmp(line,"Author:",7) == 0) {
        p->user.name     = strdup(line+8);
        p->user.name_len = llen - 9;
      }
      else if(cf_strncmp(line,"EMail:",6) == 0) {
        p->user.email     = strdup(line+7);
        p->user.email_len = llen - 8;
      }
      else if(cf_strncmp(line,"Category:",9) == 0) {
        p->category     = strdup(line+10);
        p->category_len = llen - 11;
      }
      else if(cf_strncmp(line,"Subject:",8) == 0) {
        p->subject     = strdup(line+9);
        p->subject_len = llen - 10;
      }
      else if(cf_strncmp(line,"HomepageUrl:",12) == 0) {
        p->user.hp     = strdup(line+13);
        p->user.hp_len = llen - 14;
      }
      else if(cf_strncmp(line,"ImageUrl:",9) == 0) {
        p->user.img     = strdup(line+10);
        p->user.img_len = llen - 11;
      }
      else if(cf_strncmp(line,"Body:",5) == 0) {
        p->content     = strdup(line+6);
        p->content_len = llen - 7;
      }
      else if(cf_strncmp(line,"RemoteAddr:",11) == 0) {
        p->user.ip     = strdup(line+12);
        p->user.ip_len = llen - 13;
      }
      else {
        free(line);
        line = NULL;
      }

      if(line) free(line);
    }
    else {
      cf_log(LOG_ERR,__FILE__,__LINE__,"readline: %s\n",strerror(errno));
    }
  } while(line);

  p->date = time(NULL);

  if(!p->user.name || !p->user.ip || !p->unid) {
    writen(sock,"500 Sorry\n",10);
    return 0;
  }

  CF_RW_WR(&head.lock);
  p->mid  = ++head.mid;
  CF_RW_UN(&head.lock);

  return 1;
}
/* }}} */

/* {{{ cf_send_posting
 * Returns: nothing
 * Parameters:
 *   - int sock    the socket of the connection
 *   - u_int64_t tid     the thread id
 *   - u_int64_t mid     the message id
 *
 * this function sends a posting/thread to the client
 *
 */
void cf_send_posting(int sock,u_int64_t tid,u_int64_t mid,int invisible) {
  int n;
  u_char buff[500];
  t_thread *t = cf_get_thread(tid);
  t_posting *p = NULL;
  t_string bff;
  int first = 1;

  if(!t) {
    writen(sock,"404 Thread not found\n",21);
    return;
  }

  if(mid != 0) {
    p = cf_get_posting(t,mid);

    if(!p) {
      writen(sock,"404 Posting not found\n",22);
      return;
    }
  }

  str_init(&bff);

  str_chars_append(&bff,"200 Ok\n",7);

  CF_RW_RD(&t->lock);

  if(p) {
    if(p->invisible == 1 && !invisible) {
      writen(sock,"404 Posting not found\n",22);
      CF_RW_UN(&t->lock);
      return;
    }
  }

  p = t->postings;
  n = sprintf(buff,"THREAD t%lld m%lld\n",t->tid,p->mid);
  str_chars_append(&bff,buff,n);

  for(;p;p=p->next) {
    if(p->invisible && !invisible) {
      for(;p && p->invisible;p=p->next);
      if(!p) break;
    }

    if(!first) {
      n = sprintf(buff,"MSG m%lld\n",p->mid);
      str_chars_append(&bff,buff,n);
    }

    first = 0;

    str_chars_append(&bff,"Author:",7);
    str_chars_append(&bff,p->user.name,p->user.name_len);

    if(p->user.email) {
      str_chars_append(&bff,"\nEMail:",7);
      str_chars_append(&bff,p->user.email,p->user.email_len);
    }

    if(p->user.hp) {
      str_chars_append(&bff,"\nHomepage:",10);
      str_chars_append(&bff,p->user.hp,p->user.hp_len);
    }

    if(p->user.img) {
      str_chars_append(&bff,"\nImage:",7);
      str_chars_append(&bff,p->user.img,p->user.img_len);
    }

    str_chars_append(&bff,"\nSubject:",9);
    str_chars_append(&bff,p->subject,p->subject_len);

    if(p->category) {
      str_chars_append(&bff,"\nCategory:",10);
      str_chars_append(&bff,p->category,p->category_len);
    }

    n = sprintf(buff,"\nDate:%ld\n",p->date);
    str_chars_append(&bff,buff,n);

    n = sprintf(buff,"Level:%d\n",p->level);
    str_chars_append(&bff,buff,n);

    n = sprintf(buff,"Visible:%d\n",p->invisible == 0);
    str_chars_append(&bff,buff,n);

    str_chars_append(&bff,"Content:",8);
    str_chars_append(&bff,p->content,p->content_len);

    str_char_append(&bff,'\n');
  }

  CF_RW_UN(&t->lock);

  str_char_append(&bff,'\n');

  writen(sock,bff.content,bff.len);
  free(bff.content);
}
/* }}} */

/* {{{ cf_get_posting
 * Returns: t_posting *  the posting or NULL
 * Parameters:
 *   - t_thread *t       a pointer to the thread
 *   - u_int64_t mid           the message id
 *
 * this function searches for a posting in the thread
 *
 */
t_posting *cf_get_posting(t_thread *t,u_int64_t mid) {
  t_posting *p;

  CF_RW_RD(&t->lock);

  for(p=t->postings;p && p->mid != mid;p=p->next);

  CF_RW_UN(&t->lock);

  return p;  /* this returns NULL or the posting */
}
/* }}} */

/* {{{ cf_get_thread
 * Returns: t_thread *   a pointer to the thread
 * Parameters:
 *   - u_int64_t tid           the thread id
 *
 * this function searches for a thread in the tree
 *
 */
t_thread *cf_get_thread(u_int64_t tid) {
  t_thread **t = NULL;
  u_char buff[50];
  size_t len;

  len = snprintf(buff,50,"t%lld",tid);

  CF_RW_RD(&head.threads_lock);
  if(head.threads) t = cf_hash_get(head.threads,buff,len);
  CF_RW_UN(&head.threads_lock);

  return t ? *t : NULL;
}
/* }}} */

/* {{{ CF_SHARED_MEM */
#ifdef CF_SHARED_MEM

/* {{{ cf_shmdt */
/**
 * Wrapper function of the shmdt function. Used to log access
 * to the shared memory segments
 * \param ptr The pointer to the shared memory segment
 * \return Returns 0 or -1
 */
int cf_shmdt(void *ptr) {
  cf_log(LOG_DBG,__FILE__,__LINE__,"shmdt: detaching %ld\n",ptr);
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

  cf_log(LOG_DBG,__FILE__,__LINE__,"shmat: attatching segment %d (%ld)\n",shmid,ptr);

  return ptr;
}
/* }}} */

void cf_generate_shared_memory() {
  t_mem_pool pool;
  t_thread *t,*t1;
  t_posting *p;
  t_name_value *v = cfg_get_first_value(&fo_default_conf,NULL,"SharedMemIds");
  u_int32_t val;
  time_t tm = time(NULL);
  unsigned short semval;

  mem_init(&pool);

  CF_RW_RD(&head.lock);
  t1 = t = head.thread;
  CF_RW_UN(&head.lock);

  /*
   *
   * CAUTION! Deep magic begins here!
   *
   */

  mem_append(&pool,&tm,sizeof(t));

  for(;t;t=t1) {
    CF_RW_RD(&t->lock);

    /* we only need thread id and postings */
    mem_append(&pool,&(t->tid),sizeof(t->tid));
    mem_append(&pool,&(t->posts),sizeof(t->posts));

    for(p=t->postings;p;p=p->next) {
      mem_append(&pool,&(p->mid),sizeof(p->mid));

      val = p->subject_len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->subject,val);

      val = p->category_len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->category,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->content_len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->content,val);

      mem_append(&pool,&(p->date),sizeof(p->date));
      mem_append(&pool,&(p->level),sizeof(p->level));
      mem_append(&pool,&(p->invisible),sizeof(p->invisible));

      val = p->user.name_len + 1;
      mem_append(&pool,&val,sizeof(val));
      mem_append(&pool,p->user.name,val);

      val = p->user.email_len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.email,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.hp_len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.hp,val);
      }
      else {
        val = 0;
        mem_append(&pool,&val,sizeof(val));
      }

      val = p->user.img_len + 1;
      if(val > 1) {
        mem_append(&pool,&val,sizeof(val));
        mem_append(&pool,p->user.img,val);
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
   *
   * Phew. Deep magic ends
   *
   */


  /* lets go surfin' */
  CF_LM(&head.shm_lock);

  if(cf_sem_getval(head.shm_sem,0,1,&semval) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"cf_sem_getval: %s\n",strerror(errno));
    exit(-1);
  }

  /* semval contains now the number of the shared memory segment *not* used */
  if(semval != 0 && semval != 1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"hu? what happened? semval is %d\n",semval);
    semval = 0;
  }

  cf_log(LOG_DBG,__FILE__,__LINE__,"shm_ids[%d]: %ld\n",semval,head.shm_ids[semval]);

  /* does the segment already exists? */
  if(head.shm_ids[semval] != -1) {
    /* yeah, baby, yeah! */

    cf_log(LOG_DBG,__FILE__,__LINE__,"shm_ptrs[%d]: %ld\n",semval,head.shm_ptrs[semval]);

    /* oh behave! detach the memory */
    if(head.shm_ptrs[semval]) {
      if(cf_shmdt(head.shm_ptrs[semval]) != 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"shmdt: %s (semval: %d)\n",strerror(errno),semval);
        CF_UM(&head.shm_lock);
        mem_cleanup(&pool);
        return;
      }
    }

    /* delete the segment */
    if(shmctl(head.shm_ids[semval],IPC_RMID,NULL) != 0) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
      CF_UM(&head.shm_lock);
      mem_cleanup(&pool);
      return;
    }
  }

  if((head.shm_ids[semval] = shmget(atoi(v->values[semval]),pool.len,IPC_CREAT|CF_SHARED_MODE)) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"shmget: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  if((head.shm_ptrs[semval] = cf_shmat(head.shm_ids[semval],NULL,0)) == NULL) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"shmat: %s\n",strerror(errno));
    exit(EXIT_FAILURE);
  }

  memcpy(head.shm_ptrs[semval],pool.content,pool.len);

  mem_cleanup(&pool);

  if(semval == 1) {
    CF_SEM_DOWN(head.shm_sem,0);
  }
  else {
    CF_SEM_UP(head.shm_sem,0);
  }

  CF_UM(&head.shm_lock);

  cf_log(LOG_DBG,__FILE__,__LINE__,"generated shared memory segment %d\n",semval);
}

#endif
/* }}} */

/* {{{ cf_generate_cache
 * Returns: NULL
 * Parameters:
 *   - void *arg    should always be NULL
 *
 * this function generates the two cache lists
 *
 */
void *cf_generate_cache(void *arg) {
#ifndef CF_SHARED_MEM
  t_string str1,str2;

  str_init(&str1);
  str_init(&str2);

  cf_generate_list(&str1,0);
  cf_generate_list(&str2,1);

  CF_RW_WR(&head.lock);

  if(head.cache_invisible.content) {
    free(head.cache_invisible.content);
  }
  if(head.cache_visible.content) {
    free(head.cache_visible.content);
  }

  head.cache_invisible.content = str2.content;
  head.cache_visible.content   = str1.content;
  head.cache_invisible.len     = str2.len;
  head.cache_visible.len       = str1.len;

  head.date_visible            = time(NULL);
  head.date_invisible          = time(NULL);

  head.fresh = 1;

  CF_RW_UN(&head.lock);
#else
  cf_generate_shared_memory();
#endif

  return NULL;
}
/* }}} */

/* {{{ cf_generate_list
 * Returns: u_char *  the thread list
 * Parameters:
 *   - short del    defines if the invisible
 *                  messages should be shown or not
 *
 * this function generates a thread list
 *
 */
void cf_generate_list(t_string *str,int del) {
  int n;
  u_char buff[500];
  t_thread *t,*t1;
  int first;
  t_posting *p;

  str_chars_append(str,"200 Ok\n",7);

  CF_RW_RD(&head.lock);
  t1 = head.thread;
  CF_RW_UN(&head.lock);

  while(t1) {
    first = 1;

    t = t1;

    CF_RW_RD(&t->lock);

    for(p = t->postings;p;p = p->next) {
      if(p->invisible && !del) {
        for(;p && p->invisible;p=p->next);

        if(!p) {
          break;
        }
      }


      /* thread/posting header */
      if(first) {
        first = 0;
        n     = snprintf(buff,256,"THREAD t%lld m%lld\n",t->tid,p->mid);
      }
      else {
        n     = snprintf(buff,256,"MSG m%lld\n",p->mid);
      }

      str_chars_append(str,buff,n);

      /* author */
      str_chars_append(str,"Author:",7);
      str_chars_append(str,p->user.name,p->user.name_len);

      /* subject */
      str_chars_append(str,"\nSubject:",9);
      str_chars_append(str,p->subject,p->subject_len);

      /* category */
      if(p->category) {
        str_chars_append(str,"\nCategory:",10);
        str_chars_append(str,p->category,p->category_len);
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

/* {{{ cf_send_thread_list
 * Returns: nothing
 * Parameters:
 *   - int sockfd   the socket handle of
 *                  the connection
 *   - int del      a true-false switch. If
 *                  true, the list includes also
 *                  invisible threads
 *
 * this function handles a connection
 *
 */
void cf_send_thread_list(int sockfd,int del) {
  int n;
  u_char buff[500];
  t_thread *t,*t1;

  CF_RW_RD(&head.lock);
  t = head.thread;
  CF_RW_UN(&head.lock);

  writen(sockfd,"200 Ok\n",7);

  while(t) {
    int               first = 1;
    t_posting        *p;

    CF_RW_RD(&t->lock);

    for(p = t->postings;p;p = p->next) {
      if(p->invisible && !del) {
        for(;p && p->invisible;p=p->next);

        if(!p) {
          break;
        }
      }

      /* thread/posting header */
      if(first) {
        first = 0;
        n     = sprintf(buff,"THREAD t%lld m%lld\n",t->tid,p->mid);
      }
      else {
        n     = sprintf(buff,"MSG m%lld\n",p->mid);
      }

      if(writen(sockfd,buff,n) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }

      /* author */
      if(writen(sockfd,"Author:",7) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }
      if(writen(sockfd,p->user.name,p->user.name_len) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }

      /* subject */
      if(writen(sockfd,"\nSubject:",9) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }
      if(writen(sockfd,p->subject,p->subject_len) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }

      /* category */
      if(p->category) {
        if(writen(sockfd,"\nCategory:",10) <= 0) {
          cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
          CF_RW_UN(&t->lock);
          return;
        }
        if(writen(sockfd,p->category,p->category_len) <= 0) {
          cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
          CF_RW_UN(&t->lock);
          return;
        }
      }

      /* date */
      n = sprintf(buff,"\nDate:%ld\n",p->date);
      if(writen(sockfd,buff,n) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }

      /* level */
      n = sprintf(buff,"Level:%d\n",p->level);
      if(writen(sockfd,buff,n) <= 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"writen: %s\n",strerror(errno));
        CF_RW_UN(&t->lock);
        return;
      }
    }

    writen(sockfd,"END\n",4);

    t1 = t->next;
    CF_RW_UN(&t->lock);
    t  = t1;
  }

  writen(sockfd,"\n",1);
}
/* }}} */

/* eof */
