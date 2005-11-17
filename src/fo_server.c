/**
 * \file fo_server.c
 * \author Christian Kruse
 *
 * Forum server; holds all data in RAM
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
#include <grp.h>
#include <pwd.h>
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
#include "fo_server.h"
/* }}} */

/* {{{ setup_server_environment */
void setup_server_environment(const u_char *pidfile) {
  struct stat st;
  FILE *fd;
  pid_t pid;
  u_char buff[50];
  size_t len;

  if(stat(pidfile,&st) == 0) {
    fprintf(stderr,"the PID file (%s) exists! Maybe there is already an instance running" \
      "or the server crashed. However, if there is no instance running you" \
      "should remove the file. Sorry, but I have to exit\n",
      pidfile
    );

    exit(-1);
  }

  if((fd = fopen(pidfile,"w")) == NULL) {
    fprintf(stderr,"Could not open PID file (%s)!\n",strerror(errno));
    exit(-1);
  }

  pid = getpid();
  len = snprintf(buff,50,"%d",pid);
  fwrite(buff,len,1,fd);
  fclose(fd);
}
/* }}} */

/* {{{ usage */
void usage(void) {
  fprintf(stderr,"Usage:\n" \
    "[CF_CONF_DIR=\"/path/to/config\"] fo_server [options]\n\n" \
    "where options are:\n" \
    "\t-p, --pid-file          Path to the pid file (optional)\n" \
    "\t-c, --config-directory  Path to the configuration directory\n" \
    "\t-d, --daemonize         Detach process from shell\n" \
    "\t-h, --help              Show this help screen\n\n" \
    "One of both must be set: config-directory option or CF_CONF_DIR\n" \
    "environment variable\n\n"
  );
  exit(-1);
}
/* }}} */

/* {{{ flsh */
void flsh(int sig) {
  /* you really should not give NULL as first argument to cf_log... */
  cf_log(NULL,CF_STD|CF_FLSH,__FILE__,__LINE__,"flushing file handles...\n");
  cf_log(NULL,CF_ERR|CF_FLSH,__FILE__,__LINE__,"flushing file handles...\n");
}
/* }}} */

/* {{{ terminate */
void terminate(int sig) {
  cf_log(NULL,CF_STD|CF_FLSH,__FILE__,__LINE__,"got SIGTERM, terminating\n");
  RUN = 0;
}
/* }}} */

/* {{{ cleanup_worker */
void cleanup_worker(void *data) {
  pthread_t *thread = (pthread_t *)data;
  pthread_join(*thread,NULL);
}
/* }}} */

/* {{{ destroy_client */
void destroy_client(void *data) {
  cf_client_t *client = (cf_client_t *)data;
  close(client->sock);
}
/* }}} */

/* {{{ destroy_server */
void destroy_server(void *data) {
  cf_server_t *srv = (cf_server_t *)data;
  free(srv->addr);
  close(srv->sock);
}
/* }}} */

/* {{{ logfile_worker */
void logfile_worker(cf_cfg_config_t *cfg) {
  cf_cfg_config_value_t *v = cf_cfg_get_value(cfg,"LogMaxSize");
  cf_cfg_config_value_t *log;
  off_t size = v->ival;
  struct stat st;
  u_char buff[256];
  struct tm tm;
  time_t t;
  cf_string_t str;
  size_t len;

  if(RUN == 0) return;

  time(&t);
  localtime_r(&t,&tm);

  log = cf_cfg_get_value(cfg,"StdLog");
  if(stat(log->sval,&st) == 0) {
    if(st.st_size >= size) {
      pthread_mutex_lock(&head.log.lock);
      fclose(head.log.std);

      len = strftime(buff,256,".%Y-%m-%d-%T",&tm);
      cf_str_init(&str);
      cf_str_char_set(&str,log->sval,strlen(log->sval));
      cf_str_chars_append(&str,buff,len);

      rename(log->sval,str.content);

      head.log.std = NULL;

      cf_str_cleanup(&str);
      pthread_mutex_unlock(&head.log.lock);
    }
  }

  log = cf_cfg_get_value(cfg,"ErrorLog");
  if(stat(log->sval,&st) == 0) {
    if(st.st_size >= size) {
      pthread_mutex_lock(&head.log.lock);
      fclose(head.log.err);

      len = strftime(buff,256,".%Y-%m-%d-%T",&tm);
      cf_str_init(&str);
      cf_str_char_set(&str,log->sval,strlen(log->sval));
      cf_str_chars_append(&str,buff,len);

      rename(log->sval,str.content);

      head.log.err = NULL;

      cf_str_cleanup(&str);
      pthread_mutex_unlock(&head.log.lock);
    }
  }
}
/* }}} */

/* {{{ get_gid */
gid_t get_gid(const u_char *gname) {
  struct group *gr = getgrnam(gname);

  if(gr) return gr->gr_gid;
  perror("getgrnam");

  return 0;
}
/* }}} */

/* {{{ get_uid */
uid_t get_uid(const u_char *uname) {
  struct passwd *pwd = getpwnam(uname);

  if(pwd) return pwd->pw_uid;
  return 0;
}
/* }}} */

/* {{{ struct option server_cmdline_options[] */
static struct option server_cmdline_options[] = {
  { "pid-file",         1, NULL, 'p' },
  { "config-directory", 1, NULL, 'c' },
  { "daemonize",        0, NULL, 'd' },
  { "help",             0, NULL, 'h' },
  { NULL,               0, NULL, 0   }
};
/* }}} */

head_t head;
int RUN = 1;

static const u_char *wanted[] = {
  "fo_default", "fo_server"
};

int main(int argc,char *argv[]) {
  pid_t pid;

  int c,
      j,
      ret = 0,
      sock,
      connfd,
      status,
      daemonize = 0,
      max_threads = 0,
      start_threads = 0,
      spare_threads = 0;

  gid_t gid;
  uid_t uid;

  size_t i;
  socklen_t size;

  fd_set rfds;

  u_char *pidfile = NULL;

  cf_cfg_config_value_t *pidfile_nv,
               *forums,
               *threads,
               *run_archiver,
               *usergroup,
               *chrootv;

  cf_forum_t *actforum;

  cf_list_element_t *elem;
  cf_server_t *srv;
  pthread_attr_t thread_attr;
  pthread_t thread;
  struct sockaddr_un addr;
  struct timeval timeout;
  struct sched_param param;

  cf_server_init_filter_t fkt;
  cf_handler_config_t *handler;

  cf_periodical_t per;

  cf_cfg_config_t cfg;


  /* set signal handlers */
  signal(SIGPIPE,SIG_IGN);
  signal(SIGINT,flsh);
  signal(SIGHUP,flsh);
  signal(SIGTERM,terminate);


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
  while((c = getopt_long(argc,argv,"p:c:dh",server_cmdline_options,NULL)) > 0) {
    switch(c) {
      case 'p':
        if(!optarg) usage();
        pidfile = strdup(optarg);
        break;
      case 'c':
        if(!optarg) usage();
        setenv("CF_CONF_DIR",optarg,1);
        break;
      case 'd':
        daemonize = 1;
        break;
      default:
        usage();
    }
  }
  /* }}} */

  /* {{{ prepare to read and read configuration */
  if(cf_cfg_get_conf(&cfg,wanted,2) != 0) {
    fprintf(stderr,"config file error\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ security handling... never run as root, give ability to chroot() somewhere */
  /* {{{ get GID and UID */
  if((usergroup = cf_cfg_get_value(&cfg,"UserGroup")) != NULL) {
    if(usergroup->avals[1].type == CF_ASM_ARG_NUM) gid = usergroup->avals[1].ival;
    else{
      if((gid = get_gid(usergroup->avals[1].sval)) == 0) {
        fprintf(stderr,"config error: cannot set gid! Config value: %s\n",usergroup->avals[1].sval);
        return EXIT_FAILURE;
      }
    }

    if(usergroup->avals[0].type == CF_ASM_ARG_NUM) uid = usergroup->avals[0].ival;
    else {
      if((uid = get_uid(usergroup->avals[0].sval)) == 0) {
        fprintf(stderr,"config error: cannot set uid! config value: %s\n",usergroup->avals[0].sval);
        return EXIT_FAILURE;
      }
    }
  }
  else {
    if((uid = getuid()) == 0) {
      fprintf(stderr,"You should not run this server as root! Set UserGroup in fo_server.cfcl to an appropriate value!\n");
      return EXIT_FAILURE;
    }

    if((gid = getgid()) == 0) {
      fprintf(stderr,"You should not run this server with gid 0! Set UserGroup in fo_server.cfcl to an appropriate value!\n");
      return EXIT_FAILURE;
    }
  }
  /* }}} */

  /* {{{ chroot() */
  if((chrootv = cf_cfg_get_value(&cfg,"Chroot")) != NULL) {
    if(chdir(chrootv->sval) == -1) {
      fprintf(stderr,"could not chdir to chroot dir '%s': %s\n",chrootv->sval,strerror(errno));
      return EXIT_SUCCESS;
    }

    if(chroot(chrootv->sval) == -1) {
      fprintf(stderr,"could not chroot to dir '%s': %s\n",chrootv->sval,strerror(errno));
      return EXIT_SUCCESS;
    }
  }
  /* }}} */

  /* {{{ set GID and UID */
  if(usergroup) {
    if(setgid(gid) == -1 || setregid(gid,gid) == -1) {
      if(usergroup->avals[1].type == CF_ASM_ARG_NUM) fprintf(stderr,"config error: cannot set gid! Config value: %d, error: %s\n",usergroup->avals[1].ival,strerror(errno));
      else fprintf(stderr,"config error: cannot set gid! Config value: %s, error: %s\n",usergroup->avals[1].sval,strerror(errno));
      return EXIT_FAILURE;
    }

    if(setuid(uid) == -1 || setreuid(uid,uid) == -1) {
      if(usergroup->avals[0].type == CF_ASM_ARG_NUM) fprintf(stderr,"config error: cannot set uid! config value: %d, error: %s\n",usergroup->avals[0].ival,strerror(errno));
      else fprintf(stderr,"config error: cannot set uid! config value: %s, error: %s\n",usergroup->avals[0].sval,strerror(errno));
      return EXIT_FAILURE;
    }
  }
  /* }}} */
  /* }}} */

  if(!pidfile) {
    pidfile_nv = cf_cfg_get_value(&cfg,"PIDFile");
    pidfile    = strdup(pidfile_nv->sval);
  }

  /* {{{ become a deamon */
  /* shall we daemonize? */
  if(daemonize) {
    /* we daemonize... */
    switch(pid = fork()) {
      case -1:
        fprintf(stderr,"fo_server: could not fork: %s\n",strerror(errno));
        exit(-1);

      case 0:
        if(setsid() == -1) {
          fprintf(stderr,"Could not detach from shell: %s\n",strerror(errno));
          exit(-1);
        }
       break;

      default:
        printf("server forked. It's pid is: %d\n",pid);
        exit(0);
    }
  }
   /* }}} */

  /* be sure that only one instance runs */
  setup_server_environment(pidfile);

  /* {{{ ok, go through each forum, register it and read it's data...
   *
   * this can take a while, maybe we should send a message to the user?
   */
  forums = cf_cfg_get_value(&cfg,"Forums");
  for(i=0;i<forums->alen;i++) {
    if((actforum = cf_register_forum(&cfg,forums->avals[i].sval)) == NULL) {
      cf_log(&cfg,CF_ERR,__FILE__,__LINE__,"could not register forum %s\n",forums->avals[i].sval);
      continue;
    }

    cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Loading data for forum %s...\n",forums->avals[i].sval);
    if(cf_load_data(&cfg,actforum) == -1) {
      cf_log(&cfg,CF_ERR,__FILE__,__LINE__,"could not load data for forum %s!\n",forums->avals[i].sval);
      exit(-1);
    }
    cf_log(&cfg,CF_STD,__FILE__,__LINE__,"Loaded data for forum %s\n",forums->avals[i].sval);
  }
  /* }}} */

  /* {{{ we need no standard streams any longer */
  if(daemonize) {
    fclose(stdout);
    fclose(stderr);
    fclose(stdin);
  }
  /* }}} */

  /* go and load shared memory/cache data */
  cf_generate_cache(&cfg,NULL);

  /* {{{ more initialization (some threading options, starting of the worker threads, etc, pp) */
  #ifdef sun
  /*
   * On Solaris 2.5, on a uniprocessor machine threads run not
   * asynchronously by default. So we increase the thread concurrency
   * level that threads can run asynchronous. In fact, in concurrency
   * level six, six threads can run "simultanously".
   */
  thr_setconcurrency(6);
  #endif

  pthread_attr_init(&thread_attr);

  /*
   * on very high traffic, the server does accept more and more
   * connections, but does not serve these connection in an
   * acceptable time. So we experience a little bit with thread
   * scheduling...
   */
  #ifdef _POSIX_THREAD_PRIORITY_SCHEDULING
  memset(&param,0,sizeof(param));

  param.sched_priority = (sched_get_priority_min(SCHEDULING) + sched_get_priority_max(SCHEDULING)) / 2;

  pthread_setschedparam(pthread_self(),SCHEDULING,&param);

  param.sched_priority++;
  pthread_attr_setschedparam(&thread_attr,&param);
  pthread_attr_setinheritsched(&thread_attr,PTHREAD_INHERIT_SCHED);
  #endif

  /* ok, start worker threads */
  threads = cf_cfg_get_value(&cfg,"MinThreads");
  start_threads = threads->ival;

  for(j=0;j<start_threads;++j) {
    if((status = pthread_create(&thread,&thread_attr,cf_worker,&cfg)) != 0) {
      cf_log(&cfg,CF_ERR,__FILE__,__LINE__,"error creating worker thread %u: %s\n",j,strerror(errno));
      exit(-1);
    }

    cf_rw_list_append(&cfg,&head.workers.list,&thread,sizeof(thread));
    head.workers.num++;
  }

  /* needed later */
  threads = cf_cfg_get_value(&cfg,"SpareThreads");
  spare_threads = threads->ival;

  threads = cf_cfg_get_value(&cfg,"MaxThreads");
  max_threads = threads->ival;

  /* {{{ register periodicals */
  run_archiver = cf_cfg_get_value(&cfg,"RunArchiver");
  per.periode = run_archiver->ival;
  per.worker = cf_io_worker;
  cf_list_append(&head.periodicals,&per,sizeof(per));

  per.periode = 1800;
  per.worker = logfile_worker;
  cf_list_append(&head.periodicals,&per,sizeof(per));
  /* }}} */

  /* start thread for periodical jobs */
  if((status = pthread_create(&thread,&thread_attr,cf_periodical_worker,&cfg)) != 0) {
    cf_log(&cfg,CF_ERR|CF_FLSH,__FILE__,__LINE__,"error creating I/O thread: %s\n",strerror(errno));
    exit(-1);
  }
  /* }}} */

  sock = cf_setup_socket(&cfg,&addr);
  cf_push_server(&cfg,sock,(struct sockaddr *)&addr,sizeof(addr),cf_cftp_handler);

  /* {{{ go through each init plugin and run it */
  if(cfg.modules[INIT_HANDLER].elements) {
    ret = FLT_OK;

    for(i=0;i<cfg.modules[INIT_HANDLER].elements && (ret == FLT_DECLINE || ret == FLT_OK);++i) {
      handler = cf_array_element_at(&cfg.modules[INIT_HANDLER],i);
      fkt     = (cf_server_init_filter_t)handler->func;
      ret     = fkt(&cfg,sock);
    }
  }
  /* }}} */

  /* give workers time to wait for conditional */
  sleep(1);

  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Read config, load data, set up socket and generated caches. Now listening...\n");

  /* {{{ main loop */
  while(RUN && ret != FLT_EXIT) {
    /* set the fdset */
    FD_ZERO(&rfds);

    /* {{{ fill the fdset with the server sockets */
    CF_LM(&cfg,&head.servers.lock);

    for(elem = head.servers.list.elements;elem;elem = elem->next) {
      srv = (cf_server_t *)elem->data;

      if(sock < srv->sock) sock = srv->sock;
      FD_SET(srv->sock,&rfds);
    }

    CF_UM(&cfg,&head.servers.lock);
    /* }}} */

    /*
     * since linux developers had the silly idea to modify
     * the timeout struct of select(), we have to re-initialize
     * it in each loop
     */
    memset(&timeout,0,sizeof(timeout));

    /* check every 10 seconds if we shall exit */
    timeout.tv_sec = 10;

    /* wait for incoming connections */
    ret = select(sock+1,&rfds,NULL,NULL,&timeout);

    /* timeout? */
    if(ret > 0) {
      /* {{{ get the connection */
      CF_LM(&cfg,&head.servers.lock);

      for(elem=head.servers.list.elements;elem;elem=elem->next) {
        srv = (cf_server_t *)elem->data;

        if(FD_ISSET(srv->sock,&rfds)) {
          size   = srv->size;
          connfd = accept(srv->sock,srv->addr,&size);

          /* accept-error? */
          if(connfd <= 0) {
            cf_log(&cfg,CF_ERR|CF_FLSH,__FILE__,__LINE__,"accept: %s\n",strerror(errno));
            continue;
          }

          cf_push_client(&cfg,connfd,srv->worker,spare_threads,max_threads,&thread_attr);
        }
      }

      CF_UM(&cfg,&head.servers.lock);
      /* }}} */
    }

  }
  /* }}} */

  RUN = 0;

  /*
   * Cleanup code follows
   */

  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Going down...\n");

  /* {{{ destroy workers */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Destroying workers...\n");
  CF_CD_BC(&cfg,&head.clients.cond);
  cf_rw_list_destroy(&cfg,&head.workers.list,cleanup_worker);
  /* }}} */

  /* {{{ destroy servers */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Destroying server sockets...\n");
  cf_list_destroy(&head.servers.list,destroy_server);
  /* }}} */

  /* {{{ destroy clients */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Destroying clients...\n");
  cf_list_destroy(&head.clients.list,destroy_client);
  /* }}} */

  /* {{{ ending I/O thread */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Ending I/O thread, please wait and be patient...\n");
  pthread_join(thread,NULL);
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"I/O thread ended!\n");
  /* }}} */

  /* {{{ destroy forums */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Destroying forums...\n");

  forums = cf_cfg_get_value(&cfg,"Forums");
  for(i=0;i<forums->alen;++i) {
    actforum = cf_hash_get(head.forums,forums->avals[i].sval,strlen(forums->avals[i].sval));
    cf_destroy_forum(&cfg,actforum);
    free(actforum);
  }

  cf_hash_destroy(head.forums);
  /* }}} */

  /* {{{ destroy locks and so on */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Destroying locks...\n");

  cf_rwlock_destroy(&head.lock);

  cf_cond_destroy(&head.clients.cond);
  cf_mutex_destroy(&head.clients.lock);

  cf_mutex_destroy(&head.servers.lock);
  /* }}} */

  cf_hash_destroy(head.protocol_handlers);

  /* {{{ destroy log */
  cf_log(&cfg,CF_STD|CF_FLSH,__FILE__,__LINE__,"Closing logfiles, bye bye...\n");
  pthread_mutex_destroy(&head.log.lock);

  fclose(head.log.std);
  fclose(head.log.err);
  /* }}} */

  remove(pidfile);
  free(pidfile);

  /* also the config */
  cf_cfg_config_destroy(&cfg);

  return EXIT_SUCCESS;
}

/* eof */
