/**
 * \file fo_view.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum viewer program
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
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>
#include <signal.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "fo_view.h"
/* }}} */

/**
 * Dummy function, for ignoring unknown directives
 */
int ignre(t_configfile *cf,u_char **args,int argnum) {
  return 0;
}

/* {{{ print_thread_structure
 * Returns: nothing
 * Parameters:
 *   - t_cl_thread *thr  the thread structure
 *
 * this function prints a thread list
 *
 */
void print_thread_structure(t_cl_thread *thread,t_cf_hash *head) {
  t_message *msg;
  int level = 0,len = 0;
  u_char *date,*link;
  int printed = 0,rc = 0;
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  for(msg=thread->messages;msg;msg=msg->next) {
    if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
      rc = handle_thread_list_posting(msg,head,thread->tid,0);
      if(ShowInvisible == 0 && ((rc != FLT_DECLINE && rc != FLT_OK) || msg->may_show == 0)) continue;

      printed = 1;

      date = get_time(&fo_view_conf,"DateFormatThreadList",&len,&msg->date);
      link = get_link(thread->tid,msg->mid);

      cf_set_variable(&msg->tpl,cs,"author",msg->author,strlen(msg->author),1);
      cf_set_variable(&msg->tpl,cs,"title",msg->subject,strlen(msg->subject),1);

      if(msg->category) {
        cf_set_variable(&msg->tpl,cs,"category",msg->category,strlen(msg->category),1);
      }

      if(date) {
        cf_set_variable(&msg->tpl,cs,"time",date,len,1);
        free(date);
      }

      if(link) {
        cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
        free(link);
      }

      if(msg->level < level) {
        for(;level>msg->level;level--) {
          printf("</ul></li>");
        }
      }

      level = msg->level;

      if(msg->next && has_answers(msg)) { /* this message has at least one answer */
        printf("<li>");
        tpl_cf_parse(&msg->tpl);
        printf("<ul>");

        level++;
      }
      else {
        printf("<li>");
        tpl_cf_parse(&msg->tpl);
        printf("</li>");
      }
    }
  }

  for(;level>0;level--) {
    printf("</ul></li>");
  }

  if(printed) {
    printf("\n<li>&nbsp;</li>\n");
  }
}
/* }}} */

/* {{{ send_posting
 * Returns: nothing
 * Parameters:
 *   - int sock                 the socket handle
 *   - unsigned long long tid   the thread-id
 *   - unsigned long long mid   the message-id
 *
 * this function sends a posting to an user
 *
 */
#ifndef CF_SHARED_MEM
void send_posting(t_cf_hash *head,int sock,u_int64_t tid,u_int64_t mid) {
#else
void send_posting(t_cf_hash *head,void *shm_ptr,u_int64_t tid,u_int64_t mid) {
#endif
  t_cl_thread thread;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #endif
  t_name_value *fo_thread_tpl  = cfg_get_first_value(&fo_view_conf,"TemplateForumThread");
  t_name_value *fo_posting_tpl = cfg_get_first_value(&fo_view_conf,"TemplatePosting");
  t_cf_template tpl;
  u_char fo_thread_tplname[256],fo_posting_tplname[256];
  size_t len;
  char buff[128];
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(!fo_thread_tpl || !fo_posting_tpl) {
    str_error_message("E_TPL_NOT_FOUND",NULL,15);
    return;
  }

  generate_tpl_name(fo_thread_tplname,256,fo_thread_tpl);
  generate_tpl_name(fo_posting_tplname,256,fo_posting_tpl);

  if(tpl_cf_init(&tpl,fo_posting_tplname) != 0) {
    str_error_message("E_TPL_NOT_FOUND",NULL,15);
    return;
  }

  #ifndef CF_SHARED_MEM
  if(cf_get_message_through_sock(sock,&tsd,&thread,fo_thread_tplname,tid,mid,del) == -1) {
  #else
  if(cf_get_message_through_shm(shm_ptr,&thread,fo_thread_tplname,tid,mid,del) == -1) {
  #endif
    str_error_message(ErrorString,NULL,strlen(ErrorString));
    return;
  }

  len = snprintf(buff,128,"%d",thread.msg_len);
  tpl_cf_setvar(&thread.messages->tpl,"msgnum",buff,len,0);

  len = snprintf(buff,128,"%d",thread.msg_len-1);
  tpl_cf_setvar(&thread.messages->tpl,"answers",buff,len,0);

  if(handle_posting_filters(head,&thread,&tpl) != FLT_EXIT) {
    tpl_cf_parse(&tpl);
  }

  tpl_cf_finish(&tpl);

  cleanup_struct(&thread);
}
/* }}} */

/* {{{ send_threadlist
 * Returns: nothing
 * Parameters:
 *   - int sock         the socket
 *   - t_cf_hash *head  the list of the cgi lib
 *
 * this function sends a threadlist to an user
 *
 */
#ifndef CF_SHARED_MEM
void send_threadlist(int sock,t_cf_hash *head) {
#else
void send_threadlist(void *shm_ptr,t_cf_hash *head) {
#endif
  int ret,len;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line,*tmp;
  #else
  void *ptr,*ptr1;
  #endif

  t_name_value *fo_begin_tpl  = cfg_get_first_value(&fo_view_conf,"TemplateForumBegin");
  t_name_value *fo_end_tpl    = cfg_get_first_value(&fo_view_conf,"TemplateForumEnd");
  t_name_value *fo_thread_tpl = cfg_get_first_value(&fo_view_conf,"TemplateForumThread");
  t_name_value *cs            = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_cf_template tpl_begin,tpl_end;
  u_char fo_begin_tplname[256],fo_end_tplname[256],fo_thread_tplname[256],buff[128],*ltime;
  time_t tm;
  t_cl_thread thread;
  size_t i;
  t_handler_config *handler;
  t_filter_init_view fkt;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  /* initialization work */

  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(!fo_begin_tpl || !fo_end_tpl || !fo_thread_tpl) {
    str_error_message("E_TPL_NOT_FOUND",NULL,15);
    return;
  }

  generate_tpl_name(fo_begin_tplname,256,fo_begin_tpl);
  generate_tpl_name(fo_end_tplname,256,fo_end_tpl);
  generate_tpl_name(fo_thread_tplname,256,fo_thread_tpl);

  /*
   * if not in shm mode, request the threadlist from
   * the forum server. If in shm mode, request the
   * shm pointer
   */

  #ifndef CF_SHARED_MEM
  len = snprintf(buff,128,"GET THREADLIST invisible=%d\n",del);
  writen(sock,buff,len);
  line = readline(sock,&tsd);
  #else
  ptr = shm_ptr;
  ptr1 = ptr + sizeof(time_t);
  #endif

  /*
   * Check if request was ok. If not, send error message.
   */
  #ifndef CF_SHARED_MEM
  if(!line || cf_strcmp(line,"200 Ok\n")) {
    if(line) {
      ret = snprintf(buff,128,"E_FO_%d",atoi(line));
      str_error_message(buff,NULL,ret);
      free(line);
    }
    else {
      str_error_message("E_NO_THREADLIST",NULL,15);
    }
  }
  #else
  if(!ptr) {
    str_error_message("E_NO_CONN",NULL,9,strerror(errno));
  }
  #endif

  /*
   * Request of shm segment/threadlist wen't through,
   * go on with work
   */
  else {
    tm    = time(NULL);
    ltime = get_time(&fo_view_conf,"DateFormatLoadTime",&len,&tm);

    if(tpl_cf_init(&tpl_begin,fo_begin_tplname) != 0) {
      str_error_message("E_TPL_NOT_FOUND",NULL,15);
      return;
    }
    if(tpl_cf_init(&tpl_end,fo_end_tplname) != 0) {
      str_error_message("E_TPL_NOT_FOUND",NULL,15);
      return;
    }

    tpl_cf_setvar(&tpl_begin,"charset",cs->values[0],strlen(cs->values[0]),0);

    /* run some plugins */
    if(Modules[VIEW_INIT_HANDLER].elements) {
      for(i=0;i<Modules[VIEW_INIT_HANDLER].elements;i++) {
        handler = array_element_at(&Modules[VIEW_INIT_HANDLER],i);
        fkt     = (t_filter_init_view)handler->func;
        fkt(head,&fo_default_conf,&fo_view_conf,&tpl_begin,&tpl_end);
      }
    }

    #ifndef CF_SHARED_MEM
    free(line);
    #endif

    thread.tid      = 0;
    thread.messages = NULL;
    thread.last     = NULL;
    thread.msg_len  = 0;

    if(ltime) {
      cf_set_variable(&tpl_begin,cs,"LOAD_TIME",ltime,len,1);
      free(ltime);
    }

    tpl_cf_parse(&tpl_begin);
    tpl_cf_finish(&tpl_begin);

    #ifndef CF_SHARED_MEM
    while(cf_get_next_thread_through_sock(sock,&tsd,&thread,fo_thread_tplname) == 0) {
    #else
    while((ptr1 = cf_get_next_thread_through_shm(ptr1,&thread,fo_thread_tplname)) != NULL) {
    #endif
      if((thread.messages->invisible == 0 && thread.messages->may_show) || del == CF_KEEP_DELETED) {
        tpl_cf_setvar(&thread.messages->tpl,"start","1",1,0);

        len = snprintf(buff,128,"%d",thread.msg_len);
        tpl_cf_setvar(&thread.messages->tpl,"msgnum",buff,len,0);

        len = snprintf(buff,128,"%d",thread.msg_len-1);
        tpl_cf_setvar(&thread.messages->tpl,"answers",buff,len,0);

        ret = handle_thread(&thread,head,0);
        if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) print_thread_structure(&thread,head);

        cleanup_struct(&thread);
      }
    }

    #ifndef CF_SHARED_MEM
    if(*ErrorString) {
      str_error_message(ErrorString,NULL,strlen(ErrorString));
      return;
    }
    #else
    if(ptr1 == NULL && *ErrorString) {
      str_error_message(ErrorString,NULL,strlen(ErrorString));
      return;
    }
    #endif

    tpl_cf_parse(&tpl_end);
    tpl_cf_finish(&tpl_end);
  }
}
/* }}} */

/* {{{ signal handler for bad signals */
void sighandler(int segnum) {
  FILE *fd = fopen(PROTOCOL_FILE,"a");
  u_char buff[10],*uname = NULL,*qs = NULL;

  if(fd) {
    qs    = getenv("QUERY_STRING");
    if(GlobalValues) uname = cf_hash_get(GlobalValues,"UserName",8);
    
    switch(segnum) {
      case SIGSEGV:
        snprintf(buff,10,"SIGSEGV");
        break;
      case SIGILL:
        snprintf(buff,10,"SIGILL");
        break;
      case SIGFPE:
        snprintf(buff,10,"SIGFPE");
        break;
      case SIGBUS:
        snprintf(buff,10,"SIGBUS");
        break;
      default:
        snprintf(buff,10,"UKNOWN");
        break;
    }

    fprintf(fd,"Got signal %s!Username: %s\nQuery-String: %s\n----\n",buff,uname?uname:"(null)",qs?qs:"(null)");
    flcose(fd);
  }

}
/* }}} */

/**
 * The main function of the forum viewer. No command line switches used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  #ifndef CF_SHARED_MEM
  int sock;
  #else
  void *sock;
  #endif

  static const u_char *wanted[] = {
    "fo_default", "fo_view"
  };

  int ret;
  u_char  *ucfg,*m  = NULL,*t = NULL;
  t_array *cfgfiles;
  t_cf_hash *head;
  t_configfile conf,dconf,uconf;
  t_name_value *cs = NULL;
  u_char *UserName;
  u_char *fname;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    return EXIT_FAILURE;
  }

  head = cf_cgi_new();

  cf_init();
  init_modules();
  cfg_init();

  #ifndef CF_SHARED_MEM
  sock = 0;
  #else
  sock = NULL;
  #endif

  ret  = FLT_OK;

  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_view_options);

  if(read_config(&dconf,NULL) != 0 || read_config(&conf,NULL) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }

  /* first action: authorization modules */
  if(Modules[AUTH_HANDLER].elements) {
    size_t i;
    t_filter_begin exec;
    t_handler_config *handler;

    ret = FLT_DECLINE;

    for(i=0;i<Modules[AUTH_HANDLER].elements && ret == FLT_DECLINE;i++) {
      handler = array_element_at(&Modules[AUTH_HANDLER],i);

      exec = (t_filter_begin)handler->func;
      ret = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    ucfg = get_uconf_name(UserName);
    if(ucfg) {
      free(conf.filename);
      conf.filename = ucfg;

      if(read_config(&conf,ignre) != 0) {
        fprintf(stderr,"config file error!\n");

        cfg_cleanup_file(&conf);
        cfg_cleanup_file(&dconf);
        cfg_cleanup_file(&uconf);

        return EXIT_FAILURE;
      }
    }
  }

  /* first state: let the begin-filters run! :-) */
  if(ret != FLT_EXIT && Modules[INIT_HANDLER].elements) {
    size_t i;
    t_handler_config *handler;
    t_filter_begin exec;

    for(i=0;i<Modules[INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[INIT_HANDLER],i);
      exec    = (t_filter_begin)handler->func;
      ret     = exec(head,&fo_default_conf,&fo_view_conf);
    }
  }

  cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");

  if(ret != FLT_EXIT) {
    /* now, we need a socket connection */
    // main source

    #ifndef CF_SHARED_MEM
    if((sock = set_us_up_the_socket()) <= 0) {
      printf("Content-Type: text/html; charset=%s\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      str_error_message("E_NO_SOCK",NULL,9,strerror(errno));
      exit(0);
    }
    #else
    if((sock = get_shm_ptr()) == NULL) {
      printf("Content-Type: text/html; charset=%s\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      str_error_message("E_NO_CONN",NULL,9,strerror(errno));
      exit(0);
    }
    #endif

    if(Modules[CONNECT_INIT_HANDLER].elements) {
      size_t i;
      t_filter_connect exec;
      t_handler_config *handler;

      for(i=0;i<Modules[CONNECT_INIT_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
        handler = array_element_at(&Modules[CONNECT_INIT_HANDLER],i);
        exec    = (t_filter_connect)handler->func;
        ret     = exec(head,&fo_default_conf,&fo_view_conf,sock);
      }
    }

    if(ret != FLT_EXIT) {
      printf("Content-Type: text/html; charset=%s\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      /* after that, look for m= and t= */
      if(head) {
        t = cf_cgi_get(head,"t");
        m = cf_cgi_get(head,"m");
      }

      if(t && m) {
        send_posting(head,sock,strtoull(t,NULL,10),strtoull(m,NULL,10));
      }
      else {
        send_threadlist(sock,head);
      }
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);
  cfg_cleanup(&fo_view_conf);
  cfg_cleanup_file(&conf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  cleanup_modules(Modules);
  cf_fini();

  if(head) {
    cf_hash_destroy(head);
  }

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}

/* eof */
