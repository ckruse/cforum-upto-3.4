/**
 * \file fo_server.c
 * \brief Classic Forum server program
 *
 * This file contains the main program of the forum server.
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
#include "readline.h"
#include "archiver.h"

/* }}} */

/* definition of the global variables */
int    RUN; /**< Run while this variable is != 0 */
t_head head; /**< contains all necessary data */

/* {{{ flsh */
/**
 * This function flushes the file handles of the logfiles. Useful for getting actual information
 * about the state of the server. Activated by sending SIGINT or SIGHUP.
 * \param n The signal received
 */
void flsh(int n) {
  int status;

  cf_log(LOG_STD,__FILE__,__LINE__,"flushing file handles\n");

  if((status = pthread_mutex_lock(&head.log_lock)) != 0) {
    fprintf(stderr,"pthread_mutex_lock: %s\n",strerror(status));
    return;
  }

  if(head.std) {
    fflush(head.std);
  }
  if(head.err) {
    fflush(head.err);
  }

  pthread_mutex_unlock(&head.log_lock);
}
/* }}} */

/* {{{ terminate */
void terminate(int n) {
  cf_log(LOG_STD,__FILE__,__LINE__,"got signal SIG%d, going down\n",n);
  RUN = 0;
}
/* }}} */

/* {{{ archiver_and_writer */
/**
 * This function runs the archiver and disk writer in a regular intervall
 * \param arg Dummy argument (for pthread_create)
 * \return Returns always NULL
 */
void *archiver_and_writer(void *arg) {
  t_name_value *v = cfg_get_first_value(&fo_server_conf,"RunArchiver");
  int val = atoi(v->values[0]);
#ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  struct sched_param param;

  /* the archiver has a priority lower than the server and two priorities lower than the workers */
  memset(&param,0,sizeof(struct sched_param));
  param.sched_priority = (sched_get_priority_min(SCHEDULING) + sched_get_priority_max(SCHEDULING)) / 2;
  pthread_setschedparam(pthread_self(),SCHEDULING,&param);
#endif

  if(!v) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"FATAL ERROR: could not get archiver intervall!\n");
    RUN = 0;
    return NULL;
  }

  while(RUN) {
    sleep(val); /* sleep breaks when we get SIGTERM */

    if(RUN) {
      /* run archiver and write to disk... */
      cf_log(LOG_STD,__FILE__,__LINE__,"running archiver...\n");
      cf_run_archiver_and_write_to_disk();
    }
  }

  return NULL;
}
/* }}} */

/* {{{ setup_server_infos */
/**
 * This function checks if another fo_server process is running in the actual configuration
 * and writes its PID into the PID file. If a server is already running, it terminates execution
 */
void setup_server_infos(void) {
  FILE *fpid;
  t_name_value *pidfile;
  pid_t pid;
  u_char buff[50];

  /*
   * First, lets get the path to the pid file
   */
  pidfile = cfg_get_first_value(&fo_server_conf,"PIDFile");
  if(!pidfile) {
    fprintf(stderr,"I need a pid file!\n");
    exit(-1);
  }

  /*
   * then, try to open it
   */
  fpid = fopen(pidfile->values[0],"r");
  if(fpid) {
    /*
     * after that, try to read the pid
     */
    if(!fread(buff,1,50,fpid)) {
      fprintf(stderr,"I could not read the pid file!\n");
      exit(-1);
    }

    /*
     * then, check if the pid is a valid pid
     */
    pid = atoi(buff);
    if(kill(pid,0) != -1) {
      fprintf(stderr,"Server seems to run already at pid %d\n",pid);
      exit(-1);
    }
    else {
      /*
       * if the error is a permission problem, there may be a server
       * running. So go sure and exit
       */
      if(errno == EPERM) {
        fprintf(stderr,"Server seems to run already at pid %d\n",pid);
        exit(-1);
      }
    }

    /*
     * ok, everything finished. Close file
     */
    fclose(fpid);
  }
  else {
    /*
     * pid file cannot be found. Lets be on the secure site and exit
     */
    if(errno != ENOENT) {
      fprintf(stderr,"error opening pid file: %s",strerror(errno));
      exit(-1);
    }
  }

  /*
   * Now, open the file writeable and try to write the
   * pid of this process into the file
   */
  fpid = fopen(pidfile->values[0],"w");
  if(fpid) {
    pid = getpid();

    fprintf(fpid,"%d",pid);
    fclose(fpid);
  }
  else {
    /*
     * if the pid file cannot be opened for writing, die
     */
    fprintf(stderr,"could not open pid file '%s'!\n",pidfile->values[0]);
    exit(-1);
  }
}
/* }}} */

#ifdef CF_SHARED_MEM
/* {{{ shared memory functions */

/**
 * This function creates the Shared memory locking semaphore set.
 * \param shm The information about the Shared Memory segment and Semaphore IDs
 */
int create_shm_sem(t_name_value *shm) {
  union semun smn;
  unsigned short x = 0;

  if((head.shm_sem = semget(atoi(shm->values[2]),1,S_IRWXU|S_IRWXG|S_IRWXO|IPC_CREAT)) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"semget: %s\n",strerror(errno));
    return -1;
  }

  smn.array = &x;
  if(semctl(head.shm_sem,0,SETALL,smn) == -1) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));
    return -1;
  }

  return 0;
}

/* }}} */
#endif


/**
 * This function is the starting function for the fo_server program. It takes no
 * arguments.
 * \param argc The argument count
 * \param argv The arguments vectore
 * \return It returns EXIT_SUCCESS on success and EXIT_FAILURE on failure.
 */
int main(int argc,char *argv[]) {
  int i,
      size,
      sockfd,
      status,
      connfd,
      ret = 0;

  u_char *fname;
  t_array *fnames;
  struct sockaddr_un *addr = fo_alloc(NULL,1,sizeof(struct sockaddr_un),FO_ALLOC_CALLOC);
  t_configfile default_conf,
               server_conf;
  pthread_t thr;
  t_name_value *pidfile;
  fd_set rfds;
  struct timeval timeout;
  pthread_attr_t attr;
  size_t hi;
  t_handler_config *handler;
  t_server_init_filter fkt;

  static const u_char *wanted[] = {
    "fo_default", "fo_server"
  };

  #ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  struct sched_param param;
  #endif

  #ifdef CF_SHARED_MEM
  t_name_value *shm;
  #endif

  /* {{{ Initialization */

  #ifndef DEBUG
  pid_t pid;

  /* we daemonize... */
  pid = fork();
  switch(pid) {
  case -1:
    perror("fork");
    exit(-1);

  case 0:
    if(setsid() == -1) {
      cf_log(LOG_ERR,__FILE__,__LINE__,"setsid: %s\n",strerror(errno));
      exit(-1);
    }

   break;

  default:
    printf("server forked. It's pid is: %d\n",pid);
    exit(0);
  }
  #endif

  init_modules();
  cfg_init();

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

  /* signal handlers */
  signal(SIGPIPE,SIG_IGN);
  signal(SIGINT,flsh);
  signal(SIGHUP,flsh);
  signal(SIGTERM,terminate);

  /* }}} */

  /* {{{ Configuration */
  /* initialization finished, get config files */
  if((fnames = get_conf_file(wanted,2)) == NULL) {
    return EXIT_FAILURE;
  }

  /* init the configuration file structure */
  fname = *((u_char **)array_element_at(fnames,0));
  cfg_init_file(&default_conf,fname);
  cfg_register_options(&default_conf,default_options);
  free(fname);

  fname = *((u_char **)array_element_at(fnames,1));
  cfg_init_file(&server_conf,fname);
  cfg_register_options(&server_conf,fo_server_options);
  free(fname);

  array_destroy(fnames);
  free(fnames);

  /* init configuration structures and parse config */
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
  /*
   * let's go sure that no other server is running at this configuration
   */
  setup_server_infos();
  pidfile = cfg_get_first_value(&fo_server_conf,"PIDFile");

  #ifdef CF_SHARED_MEM
  shm      = cfg_get_first_value(&fo_default_conf,"SharedMemIds");

  if(create_shm_sem(shm) != 0) return EXIT_FAILURE;

  cf_mutex_init("head.shm_lock",&head.shm_lock);
  #endif

  /*
   * now we don't longer need stdout, stdin and stderr
   */
  #ifndef DEBUG
  close(fileno(stdin));
  close(fileno(stdout));
  close(fileno(stderr));
  #endif

  #ifdef sun
  /*
   * On Solaris 2.5, on a uniprocessor machine threads run not
   * asynchronously by default. So we increase the thread concurrency
   * level that threads can run asynchronous. In fact, in concurrency
   * level six, six threads can run "simultanously".
   */
  thr_setconcurrency(6);
  #endif

  pthread_attr_init(&attr);

  /*
   * on very high traffic, the server does accept more and more
   * connections, but does not serve these connection in an
   * acceptable time. So we experience a little bit with thread
   * scheduling...
   */
  #ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  memset(&param,0,sizeof(struct sched_param));

  param.sched_priority = (sched_get_priority_min(SCHEDULING) + sched_get_priority_max(SCHEDULING)) / 2;

  pthread_setschedparam(pthread_self(),SCHEDULING,&param);

  param.sched_priority++;
  pthread_attr_setschedparam(&attr,&param);
  pthread_attr_setinheritsched(&attr,PTHREAD_INHERIT_SCHED);
  #endif

  /* create the forum tree */
  make_forumtree(&fo_default_conf,&head);

  /* and now create the socket */
  sockfd = cf_set_us_up_the_socket(addr);
  size   = sizeof(addr);

  cf_push_server(sockfd,(struct sockaddr *)addr,size,cf_handle_request);

  /* generate the cache */
  cf_generate_cache(NULL);

  /*
   * the archiver will run in a defined rythm. archiver_and_write will do this, and
   * it has its own thread.
   */
  if((status = pthread_create(&thr,&attr,archiver_and_writer,NULL)) != 0) {
    cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_create: %s",strerror(status));
    RUN = 0;
  }
  pthread_detach(thr);

  /* run initialization plugins */
  if(Modules[INIT_HANDLER].elements) {
    ret = FLT_OK;

    for(hi=0;hi<Modules[INIT_HANDLER].elements && (ret == FLT_DECLINE || ret == FLT_OK);hi++) {
      handler = array_element_at(&Modules[INIT_HANDLER],hi);
      fkt     = (t_server_init_filter)handler->func;
      ret     = fkt(sockfd);
    }
  }

  /* }}} */

  if(ret != FLT_EXIT) {
    t_server *srv;

    CF_RW_WR(&head.lock);

    /* set up the workers... */
    for(i=0;i<INITIAL_WORKERS_NUM;i++) {
      if((status = pthread_create(&head.workers[i],&attr,cf_worker,NULL)) != 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"pthread_create: %s\n",strerror(status));
        RUN = 0;
        break;
      }

      cf_log(LOG_STD,__FILE__,__LINE__,"created worker %d\n",i);
      head.clients.workers++;
    }

    CF_RW_UN(&head.lock);


    /* and now, enter the main loop */
    cf_log(LOG_STD,__FILE__,__LINE__,"Read config, parsed xml, set up the socket and generated cache. Now listening...\n");

    /* run the main loop */
    while(RUN) {
      /* set the fdset */
      FD_ZERO(&rfds);

      /* fill the fdset with the server sockets */
      CF_LM(&head.server_lock);

      for(srv = head.servers;srv;srv = srv->next) {
        if(sockfd < srv->sock) sockfd = srv->sock;
        FD_SET(srv->sock,&rfds);
      }

      CF_UM(&head.server_lock);

      /*
       * since linux developers had the silly idea to modify
       * the timeout struct of select(), we have to re-initialize
       * it in each loop
       */
      memset(&timeout,0,sizeof(struct timeval));

      /* check every 10 seconds if we shall exit */
      timeout.tv_sec = 10;

      /* wait for connections */
      ret = select(sockfd+1,&rfds,NULL,NULL,&timeout);

      /* connection or timeout? */
      if(ret > 0) {
        /* get the connection */
        CF_LM(&head.server_lock);

        for(srv=head.servers;srv;srv=srv->next) {
          if(FD_ISSET(srv->sock,&rfds)) {
            size   = srv->size;
            connfd = accept(srv->sock,srv->addr,&size);

            /* accept-error? */
            if(connfd <= 0) {
              cf_log(LOG_ERR,__FILE__,__LINE__,"accept: %s\n",strerror(errno));
              continue;
            }

            cf_push_client(connfd,srv->worker);
          }
        }

        CF_UM(&head.server_lock);
      }
    }
  }

  /* close server sockets */
  cf_log(LOG_STD,__FILE__,__LINE__,"closing server sockets...\n");
  CF_LM(&head.server_lock);

  if(1) {
    t_server *srv,*srv1;

    for(srv=head.servers;srv;srv=srv1) {
      close(srv->sock);
      srv1 = srv->next;
      free(srv);
    }
  }

  CF_UM(&head.server_lock);

  free(addr);

  /* run cleanup code of the modules (if neccessary) */
  cf_log(LOG_STD,__FILE__,__LINE__,"cleaning up modules...\n");
  cleanup_modules(Modules);

  /* write threads to disk */
  cf_log(LOG_STD,__FILE__,__LINE__,"running archiver and writing threads to disk...\n");
  cf_run_archiver_and_write_to_disk();

  /* cleanup code */
  cf_log(LOG_STD,__FILE__,__LINE__,"running cleanup code...\n");
  cleanup_forumtree();

  #ifdef CF_SHARED_MEM
  cf_log(LOG_STD,__FILE__,__LINE__,"destroying shared memory and semaphores...\n");

  if(head.shm_sem >= 0) if(semctl(head.shm_sem,0,IPC_RMID,NULL) == -1) cf_log(LOG_ERR,__FILE__,__LINE__,"semctl: %s\n",strerror(errno));

  for(i=0;i<2;i++) {
    if(head.shm_ids[i] >= 0) {
      if(head.shm_ptrs[i])
        if(shmdt(head.shm_ptrs[i]) < 0) cf_log(LOG_ERR,__FILE__,__LINE__,"shmdt: %s\n",strerror(errno));

      if(shmctl(head.shm_ids[i],IPC_RMID,0) < 0) cf_log(LOG_ERR,__FILE__,__LINE__,"shmctl: %s\n",strerror(errno));
    }
  }
  #endif

  /* destroy workers */
  CF_LM(&head.clients.lock);

  size              = head.clients.workers;
  head.clients.down = 1;
  CF_UM(&head.clients.lock);

  pthread_cond_broadcast(&head.clients.cond);

  for(i=0;i<size;i++) {
    pthread_join(head.workers[i],NULL);
  }

  /*
   * destroy waiting connections
   */
  if(head.clients.clients) {
    t_client *cli,*cli1;

    CF_LM(&head.clients.lock);

    for(cli=head.clients.clients;cli;cli=cli1) {
      writen(cli->sock,"506 Server is going down\n",25);
      close(cli->sock);
      cli1 = cli->next;
      free(cli);
    }

    CF_UM(&head.clients.lock);
  }

  if(head.threads) cf_hash_destroy(head.threads);
  if(head.protocol_handlers) cf_hash_destroy(head.protocol_handlers);
  cf_hash_destroy(head.unique_ids);

  /* close logfiles */
  cf_log(LOG_STD,__FILE__,__LINE__,"closing logfiles... bye!\n");
  pthread_mutex_lock(&head.log_lock);
  if(head.err) {
    fclose(head.err);
  }
  if(head.std) {
    fclose(head.std);
  }
  pthread_mutex_unlock(&head.log_lock);

  cf_rwlock_destroy(&head.lock);
  cf_rwlock_destroy(&head.threads_lock);

  pthread_mutex_destroy(&head.log_lock);

  cf_mutex_destroy(&head.clients.cond_lock);
  cf_mutex_destroy(&head.clients.lock);
  cf_mutex_destroy(&head.server_lock);
  cf_mutex_destroy(&head.unique_ids_mutex);

  pthread_cond_destroy(&head.clients.cond);

  /* the pid file is not longer needed */
  unlink(pidfile->values[0]);

  /* also the config */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_server_conf);

  cfg_cleanup_file(&default_conf);
  cfg_cleanup_file(&server_conf);

  return EXIT_SUCCESS;
}

/* eof */
