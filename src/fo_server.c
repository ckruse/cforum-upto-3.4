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
#include <getopt.h>

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
  cf_log(CF_STD|CF_FLSH,__FILE__,__LINE__,"flushing file handles...\n");
  cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"flushing file handles...\n");
}
/* }}} */

/* {{{ terminate */
void terminate(int sig) {
  cf_log(CF_STD|CF_FLSH,__FILE__,__LINE__,"got SIGTERM, terminating\n");
  RUN = 0;
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

t_head head;
int RUN = 1;

static const u_char *wanted[] = {
  "fo_default", "fo_server"
};

int main(int argc,char *argv[]) {
  pid_t pid;

  int c,
      j,
      ret,
      sock,
      connfd,
      status,
      error = 1,
      daemonize = 0,
      max_threads = 0,
      start_threads = 0,
      spare_threads = 0;

  size_t i,
         size;

  fd_set rfds;

  u_char *pidfile = NULL,
         *fname;

  t_array *cfgfiles;

  t_configfile conf,
               dconf;

  t_name_value *pidfile_nv,
               *forums,
               *threads;

  t_forum *actforum;

  t_internal_config *icfg;

  t_cf_list_element *elem;
  t_server *srv;
  pthread_attr_t thread_attr;
  pthread_t thread;
  struct sockaddr_un addr;
  struct timeval timeout;
  struct sched_param param;

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

  /* \todo write cleanup code */
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
  while((c = getopt_long(argc,argv,"p:c:d:h",server_cmdline_options,NULL)) > 0) {
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
  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    fprintf(stderr,"You should set CF_CONF_DIR or use the --config-directory option!\n");
    return EXIT_FAILURE;
  }

  cfg_init();
  init_modules();

  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_server_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&conf,NULL,CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }

  array_destroy(cfgfiles);
  free(cfgfiles);
  /* }}} */

  /* {{{ check if all forum contexts are present */
  forums = cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  for(i=0;i<forums->valnum;i++) {
    status = 0;
    for(elem=fo_server_conf.forums.elements;elem;elem=elem->next) {
      icfg = (t_internal_config *)elem->data;
      if(cf_strcmp(icfg->name,forums->values[i]) == 0) {
        status = 1;
        break;
      }
    }

    if(status == 0) {
      printf("Could not find context for forum %s! Exiting!\n",forums->values[i]);
      exit(-1);
    }
  }
  /* }}} */

  if(!pidfile) {
    pidfile_nv = cfg_get_first_value(&fo_server_conf,NULL,"PIDFile");
    pidfile    = strdup(pidfile_nv->values[0]);
  }

  /* be sure that only one instance runs */
  setup_server_environment(pidfile);

  /* {{{ ok, go through each forum, register it and read it's data...
   *
   * this can take a while, maybe we should send a message to the user?
   */
  forums = cfg_get_first_value(&fo_server_conf,NULL,"Forums");
  for(i=0;i<forums->valnum;i++) {
    if((actforum = cf_register_forum(forums->values[i])) == NULL) {
      cf_log(CF_ERR,__FILE__,__LINE__,"could not register forum %s\n",forums->values[i]);
      continue;
    }

    if(cf_load_data(actforum) == -1) {
      cf_log(CF_ERR,__FILE__,__LINE__,"could not load data for forum %s!\n",forums->values[i]);
      error = 1;
      break;
    }

    cf_log(CF_STD|CF_FLSH,__FILE__,__LINE__,"Loaded data for forum %s\n",forums->values[i]);
  }
  /* }}} */

  /* {{{ become a deamon */
  /* shall we daemonize? */
  if(daemonize) {
    /* we daemonize... */
    switch(pid = fork()) {
      case -1:
        perror("fork");
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

  /* {{{ we need no standard streams any longer */
  if(daemonize) {
    fclose(stdout);
    fclose(stderr);
    fclose(stdin);
  }
  /* }}} */

  /* go and load shared memory/cache data */
  cf_generate_cache(NULL);

  /* {{{ more initialization (some threading options, starting of the worker threads, etc, pp) */
  if(error == 0) {
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
    threads = cfg_get_first_value(&fo_server_conf,NULL,"MinThreads");
    start_threads = atoi(threads->values[0]);

    for(j=0;j<start_threads;j++) {
      if((status = pthread_create(&thread,&thread_attr,cf_worker,NULL)) != 0) {
        cf_log(CF_ERR,__FILE__,__LINE__,"error creating worker thread %u: %s\n",j,strerror(errno));
        exit(-1);
      }

      cf_rw_list_append(&head.workers.list,&thread,sizeof(thread));
      head.workers.num++;
    }

    /* needed later */
    threads = cfg_get_first_value(&fo_server_conf,NULL,"SpareThreads");
    spare_threads = atoi(threads->values[0]);

                threads = cfg_get_first_value(&fo_server_conf,NULL,"MaxThreads");
                max_threads = atoi(threads->values[0]);
  }
  /* }}} */

  sock = cf_setup_socket(&addr);
  cf_push_server(sock,(struct sockaddr *)&addr,sizeof(addr),cf_cftp_handler);

  cf_log(CF_STD|CF_FLSH,__FILE__,__LINE__,"Read config, load data, set up socket and generated caches. Now listening...\n");

  /* {{{ main loop */
  while(RUN) {
    /* set the fdset */
    FD_ZERO(&rfds);

    /* {{{ fill the fdset with the server sockets */
    CF_LM(&head.servers.lock);

    for(elem = head.servers.list.elements;elem;elem = elem->next) {
      srv = (t_server *)elem->data;

      if(sock < srv->sock) sock = srv->sock;
      FD_SET(srv->sock,&rfds);
    }

    CF_UM(&head.servers.lock);
    /* }}} */

    /*
     * since linux developers had the silly idea to modify
     * the timeout struct of select(), we have to re-initialize
     * it in each loop
     */
    memset(&timeout,0,sizeof(struct timeval));

    /* check every 10 seconds if we shall exit */
    timeout.tv_sec = 10;

    /* wait for incoming connections */
    ret = select(sock+1,&rfds,NULL,NULL,&timeout);

    /* timeout? */
    if(ret > 0) {
      /* {{{ get the connection */
      CF_LM(&head.servers.lock);

      for(elem=head.servers.list.elements;elem;elem=elem->next) {
        srv = (t_server *)elem->data;

        if(FD_ISSET(srv->sock,&rfds)) {
          size   = srv->size;
          connfd = accept(srv->sock,srv->addr,&size);

          /* accept-error? */
          if(connfd <= 0) {
            cf_log(CF_ERR|CF_FLSH,__FILE__,__LINE__,"accept: %s\n",strerror(errno));
            continue;
          }

          cf_push_client(connfd,srv->worker,spare_threads,max_threads,&thread_attr);
        }
      }

      CF_UM(&head.servers.lock);
      /* }}} */
    }

  }
  /* }}} */


  /* \todo cleanup code */

  /* cleanup */
  remove(pidfile);

  /* also the config */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_server_conf);

  cfg_cleanup_file(&dconf);
  cfg_cleanup_file(&conf);

  return EXIT_SUCCESS;
}

/* eof */
