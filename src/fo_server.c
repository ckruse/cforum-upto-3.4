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
 *
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
//#include "fo_server.h"
#include "readline.h"
/* }}} */

/* {{{ setup_server_environment */
void setup_server_environment(const u_char *pidfile) {
  struct stat st;
  FILE *fd;
  pid_t pid;
  u_char buff[50];
  size_t len;

  if(stat(pidfile,&st) == 0) {
    fprintf(stderr,"the PID file (%s) exists! Maybe there is already an instance running\n" \
      "or the server crashed. However, if there is no instance running you\n" \
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
  fwrite(fd,buff,len,1,fd);
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

/* {{{ struct option server_cmdline_options[] */
static struct option server_cmdline_options[] = {
  { "pid-file",         1, NULL, 'p' },
  { "config-directory", 1, NULL, 'c' },
  { "daemonize",        0, NULL, 'd' },
  { "help",             0, NULL, 'h' },
  { NULL,               0, NULL, 0   }
};
/* }}} */

static const u_char *wanted[] = {
  "fo_default", "fo_server"
};

int main(int argc,char *argv[]) {
  pid_t pid;

  int c,
      error = 1,
      daemonize = 0,
      start_threads = 0,
      spare_threads = 0;

  size_t i;

  u_char *pidfile = NULL,
         *cfgfile;

  t_array *cfgfiles;

  t_configfile conf,
               dconf;

  t_name_value *pidfile_nv,
               *forums,
               *threads;

  t_forum *actforum;

  pthread_attr_t attr;
  pthread_t thread;

  /* {{{ initialize  variables */
  cf_rwlock_init("head.lock",&head.lock);
  cf_rw_list_init("head.workers",&head.workers);

  cf_mutex_init("head.log.lock",&head.log.lock);
  head.log.std = NULL;
  head.log.err = NULL;

  cf_rw_list_init("head.clients.list",&head.clients.list);
  cf_cond_init("head.clients.cond",&head.clients.cond);

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

  if(!pidfile) {
    pidfile_nv = cfg_get_first_value(&fo_server_conf,NULL,"PIDFile");
    pidfile    = strdup(pidfile_nv->values[0]);
  }

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

  /* be sure that only one instance runs */
  setup_server_environment(pidfile);

  /* {{{ we need no standard streams any longer */
  if(daemonize) {
    fclose(stdout);
    fclose(stderr);
    fclose(stdin);
  }
  /* }}} */

  #ifdef sun
  /*
   * On Solaris 2.5, on a uniprocessor machine threads run not
   * asynchronously by default. So we increase the thread concurrency
   * level that threads can run asynchronous. In fact, in concurrency
   * level six, six threads can run "simultanously".
   */
  thr_setconcurrency(6);
  #endif

  /* {{{ ok, go through each forum, register it and read it's data... */
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

    cf_log(CF_STD,__FILE__,__LINE__,"Loaded data for forum %s\n",forums->values[i]);
  }
  /* }}} */

  /* {{{ more initialization (some threading options, starting of the worker threads, etc, pp) */
  if(error == 0) {
    pthread_attr_init(&thread_attr);

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

    /* ok, start worker threads */
    threads = cfg_get_first_value(&fo_server_conf,NULL,"MinThreads");
    start_threads = atoi(threads->values[0]);

    for(i=0;i<start_threads;i++) {
      if((status = pthread_create(&thread,&attr,cf_worker,NULL)) != 0) {
        cf_log(LOG_ERR,__FILE__,__LINE__,"error creating worker thread %d: %s\n",i,strerror(errno));
        exit(-1);
      }

      cf_rw_list_append(&head.workers,&thread,sizeof(thread));
    }

    /* needed later */
    threads = cfg_get_first_value(&fo_server_conf,NULL,"SpareThreads")
    spare_threads = atoi(threads->values[0]);
  }
  /* }}} */


  /* cleanup */
  remove(pidfile);

  /* also the config */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_server_conf);

  cfg_cleanup_file(&default_conf);
  cfg_cleanup_file(&server_conf);

  return EXIT_SUCCESS;
}

/* eof */
