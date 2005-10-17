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
#include "htmllib.h"
#include "fo_view.h"
/* }}} */

/**
 * Dummy function, for ignoring unknown directives
 */
int ignre(cf_configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t len) {
  return 0;
}

/* {{{ show_xmlhttp_thread */
#ifndef CF_SHARED_MEM
void show_xmlhttp_thread(cf_hash_t *head,int sock,u_int64_t tid,u_int64_t mid)
#else
void show_xmlhttp_thread(cf_hash_t *head,void *shm_ptr,u_int64_t tid,u_int64_t mid)
#endif
{
  int ret;
  u_char fo_thread_tplname[256],buff[512],*line = NULL,*UserName = cf_hash_get(GlobalValues,"UserName",8);
  cf_name_value_t *fo_thread_tpl,*cs;
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  int show_invi = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cl_thread_t thread;
  cf_string_t str;
  size_t len;
  rline_t tsd;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  #ifdef CF_SHARED_MEM
  int sock;
  #endif

  memset(&tsd,0,sizeof(tsd));

  fo_thread_tpl = cf_cfg_get_first_value(&fo_view_conf,fn,"TemplateForumThread");
  cs            = cf_cfg_get_first_value(&fo_default_conf,fn,"ExternCharset");

  cf_gen_tpl_name(fo_thread_tplname,256,fo_thread_tpl->values[0]);

  #ifdef CF_SHARED_MEM
  if((sock = cf_socket_setup()) == -1) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=UTF-8\015\012\015\012");
    cf_error_message("E_NO_CONN",NULL,strerror(errno));
    return;
  }
  #endif

  len = snprintf(buff,512,"SELECT %s\n",fn);
  writen(sock,buff,len);

  if((line = readline(sock,&tsd)) == NULL || cf_strncmp(line,"200",3) != 0) {
    if(line) {
      fprintf(stderr,"fo_view: xmlhttp: Server returned: %s\n",line);
      free(line);
    }
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=UTF-8\015\012\015\012");
    cf_error_message("E_DATA_FAILURE",NULL);
    return;
  }

  free(line);
  ret = cf_get_message_through_sock(sock,&tsd,&thread,tid,0,show_invi ? CF_KEEP_DELETED : CF_KILL_DELETED);

  if(ret == -1) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    if(*ErrorString) cf_error_message(ErrorString,NULL);
    else cf_error_message("E_DATA_FAILURE",NULL);
    return;
  }

  thread.threadmsg = thread.messages;

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(head,sock,&tsd,&thread);
  #endif
  #endif

  cf_str_init(&str);
  if(cf_gen_threadlist(&thread,head,&str,rm->threadlist_thread_tpl,"full",rm->posting_uri[UserName?1:0],CF_MODE_THREADLIST) != FLT_EXIT) {
    printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    fwrite(str.content,1,str.len,stdout);
    cf_str_cleanup(&str);
  }
  else {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html\015\012\015\012");
    cf_error_message("E_DATA_FAILURE",NULL);
  }

  cf_cleanup_thread(&thread);
}
/* }}} */

/* {{{ show_posting */
#ifndef CF_SHARED_MEM
void show_posting(cf_hash_t *head,int sock,u_int64_t tid,u_int64_t mid)
#else
void show_posting(cf_hash_t *head,void *shm_ptr,u_int64_t tid,u_int64_t mid)
#endif
{
  cl_thread_t thread;

  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #endif

  u_char buff[256],
    *tmp,
    *UserName = cf_hash_get(GlobalValues,"UserName",8),
    *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  cf_name_value_t *tm  = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ThreadMode"),
    *rm             = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ReadMode"),
    *cs             = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
    *fbase          = NULL,
    *name           = cf_cfg_get_first_value(&fo_view_conf,NULL,"Name"),
    *email          = cf_cfg_get_first_value(&fo_view_conf,NULL,"EMail"),
    *hpurl          = cf_cfg_get_first_value(&fo_view_conf,NULL,"HomepageUrl"),
    *imgurl         = cf_cfg_get_first_value(&fo_view_conf,NULL,"ImageUrl"),
    *ps = NULL,
    *reg = NULL;

  cf_template_t tpl;

  cf_readmode_t *rmi = cf_hash_get(GlobalValues,"RM",2);

  size_t len;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  memset(&thread,0,sizeof(thread));

  if(mid == 0) {
    free(rm->values[0]);
    rm->values[0] = strdup(tm->values[0]);
    if(cf_run_readmode_collectors(head,&fo_view_conf,rmi) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message("E_CONFIG_ERR",NULL);
      return;
    }
  }

  /* {{{ init and get message from server */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(cf_tpl_init(&tpl,rmi->thread_tpl) != 0) {
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  #ifndef CF_SHARED_MEM
  if(cf_get_message_through_sock(sock,&tsd,&thread,tid,mid,del) == -1)
  #else
  if(cf_get_message_through_shm(shm_ptr,&thread,tid,mid,del) == -1)
  #endif
  {
    if(cf_strcmp(ErrorString,"E_FO_404") == 0) {
      if(cf_run_404_handlers(head,tid,mid) != FLT_EXIT) {
        printf("Status: 404 Not Found\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        cf_error_message(ErrorString,NULL);
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message(ErrorString,NULL);
    }
    return;
  }
  /* }}} */

  if(mid == 0) thread.threadmsg = thread.messages;

  /* {{{ set standard variables */
  cf_tpl_setvalue(&tpl,"charset",TPL_VARIABLE_STRING,cs->values[0],strlen(cs->values[0]));

  UserName = cf_hash_get(GlobalValues,"UserName",8);
  if(UserName) {
    fbase = cf_cfg_get_first_value(&fo_default_conf,forum_name,"UBaseURL");
    ps = cf_cfg_get_first_value(&fo_default_conf,forum_name,"UPostScript");
    reg = cf_cfg_get_first_value(&fo_default_conf,forum_name,"UserConfig");
  }
  else {
    fbase = cf_cfg_get_first_value(&fo_default_conf,forum_name,"BaseURL");
    ps = cf_cfg_get_first_value(&fo_default_conf,forum_name,"PostScript");
    reg = cf_cfg_get_first_value(&fo_default_conf,forum_name,"UserRegister");
  }

  tmp = cf_get_link(fbase->values[0],0,0);
  cf_set_variable(&tpl,cs,"forumbase",tmp,strlen(tmp),1);
  free(tmp);

  cf_set_variable(&tpl,cs,"postscript",ps->values[0],strlen(ps->values[0]),1);
  cf_set_variable(&tpl,cs,"regscript",reg->values[0],strlen(reg->values[0]),1);

  len = snprintf(buff,256,"%llu",thread.tid);
  cf_set_variable(&tpl,cs,"tid",buff,len,0);
  len = snprintf(buff,256,"%llu",thread.threadmsg->mid);
  cf_set_variable(&tpl,cs,"mid",buff,len,0);

  /* user values */
  if(name && *name->values[0]) cf_set_variable(&tpl,cs,"aname",name->values[0],strlen(name->values[0]),1);
  if(email && *email->values[0]) cf_set_variable(&tpl,cs,"aemail",email->values[0],strlen(email->values[0]),1);
  if(hpurl && *hpurl->values[0]) cf_set_variable(&tpl,cs,"aurl",hpurl->values[0],strlen(hpurl->values[0]),1);
  if(imgurl && *imgurl->values[0]) cf_set_variable(&tpl,cs,"aimg",imgurl->values[0],strlen(imgurl->values[0]),1);

  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"start",TPL_VARIABLE_INT,1);
  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"msgnum",TPL_VARIABLE_INT,thread.msg_len);
  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"answers",TPL_VARIABLE_INT,thread.msg_len-1);
  if(UserName) cf_tpl_setvalue(&tpl,"authed",TPL_VARIABLE_INT,1);
  cf_tpl_setvalue(&tpl,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION));
  /* }}} */

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(head,sock,&tsd,&thread);
  #endif
  #endif

  if(cf_run_posting_handlers(head,&thread,&tpl,&fo_view_conf) != FLT_EXIT) cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);

  cf_cleanup_thread(&thread);
}

/* }}} */

/* {{{ show_threadlist */
#ifndef CF_SHARED_MEM
void show_threadlist(int sock,cf_hash_t *head)
#else
void show_threadlist(void *shm_ptr,cf_hash_t *head)
#endif
{
  /* {{{ variables */
  int len;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line,buff[128];
  int ret;
  #else
  void *ptr,*ptr1;
  #endif

  u_char *ltime,
    *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10),
    *UserName = cf_hash_get(GlobalValues,"UserName",8);

  cf_name_value_t *cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
    *fbase         = NULL,
    *pbase         = NULL,
    *time_fmt      = cf_cfg_get_first_value(&fo_view_conf,forum_name,"DateFormatLoadTime"),
    *time_lc       = cf_cfg_get_first_value(&fo_default_conf,forum_name,"DateLocale");

  cf_template_t tpl_begin,tpl_end;

  time_t tm;
  cl_thread_t thread,*threadp;
  size_t i;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  cf_string_t tlist;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  #ifndef CF_NO_SORTING
  cf_array_t threads;
  #endif
  /* }}} */

  /* {{{ initialization work */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif
  /* }}} */

  /* {{{ if not in shm mode, request the threadlist from
   * the forum server. If in shm mode, request the
   * shm pointer
   */
  #ifndef CF_SHARED_MEM
  len = snprintf(buff,128,"SELECT %s\n",forum_name);
  writen(sock,buff,len);
  line = readline(sock,&tsd);

  if(line && cf_strncmp(line,"200 Ok",6) == 0) {
    free(line);

    len = snprintf(buff,128,"GET THREADLIST invisible=%d\n",del);
    writen(sock,buff,len);
    line = readline(sock,&tsd);
  }
  #else
  ptr = shm_ptr;
  ptr1 = ptr + sizeof(time_t);
  #endif
  /* }}} */

  /* {{{ Check if request was ok. If not, send error message. */
  #ifndef CF_SHARED_MEM
  if(!line || cf_strcmp(line,"200 Ok\n")) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

    if(line) {
      ret = snprintf(buff,128,"E_FO_%d",atoi(line));
      cf_error_message(buff,NULL);
      free(line);
    }
    else cf_error_message("E_NO_THREADLIST",NULL);
  }
  #else
  if(!ptr) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_NO_CONN",NULL,strerror(errno));
  }
  #endif
  /* }}} */

  /*
   * Request of shm segment/threadlist wen't through,
   * go on with work
   */
  else {
    /* {{{ more initialization */
    fbase    = cf_cfg_get_first_value(&fo_default_conf,forum_name,UserName ? "UBaseURL" : "BaseURL");
    pbase    = cf_cfg_get_first_value(&fo_default_conf,forum_name,UserName ? "UPostScript" : "PostScript");

    if(cf_tpl_init(&tpl_begin,rm->pre_threadlist_tpl) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

      cf_error_message("E_TPL_NOT_FOUND",NULL);
      return;
    }
    if(cf_tpl_init(&tpl_end,rm->post_threadlist_tpl) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

      cf_error_message("E_TPL_NOT_FOUND",NULL);
      return;
    }

    cf_set_variable(&tpl_begin,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);
    cf_set_variable(&tpl_end,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);
    cf_set_variable(&tpl_begin,cs,"postscript",pbase->values[0],strlen(pbase->values[0]),1);
    cf_set_variable(&tpl_end,cs,"postscript",pbase->values[0],strlen(pbase->values[0]),1);
    cf_tpl_setvalue(&tpl_begin,"charset",TPL_VARIABLE_STRING,cs->values[0],strlen(cs->values[0]));
    cf_tpl_setvalue(&tpl_end,"charset",TPL_VARIABLE_STRING,cs->values[0],strlen(cs->values[0]));
    cf_tpl_setvalue(&tpl_begin,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION),1);
    cf_tpl_setvalue(&tpl_end,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION),1);
    /* }}} */

    /* run some plugins */
    cf_run_view_init_handlers(head,&tpl_begin,&tpl_end);

    #ifndef CF_SHARED_MEM
    free(line);
    #endif

    thread.tid      = 0;
    thread.messages = NULL;
    thread.last     = NULL;
    thread.msg_len  = 0;

    tm    = time(NULL);
    ltime = cf_general_get_time(time_fmt->values[0],time_lc->values[0],&len,&tm);
    if(ltime) {
      cf_set_variable(&tpl_begin,cs,"LOAD_TIME",ltime,len,1);
      free(ltime);
    }

    /* ok, seems to be all right, send headers */
    printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

    cf_tpl_parse(&tpl_begin);
    cf_tpl_finish(&tpl_begin);

    #ifdef CF_NO_SORTING

    #ifndef CF_SHARED_MEM
    while(cf_get_next_thread_through_sock(sock,&tsd,&thread) == 0)
    #else
    while((ptr1 = cf_get_next_thread_through_shm(ptr1,&thread)) != NULL)
    #endif
    {
      if(thread.messages) {
        if((thread.messages->invisible == 0 && thread.messages->may_show) || del == CF_KEEP_DELETED) {
          cf_str_init(&tlist);
          if(cf_gen_threadlist(&thread,head,&tlist,rm->threadlist_thread_tpl,"full",rm->posting_uri[UserName?1:0],CF_MODE_THREADLIST) != FLT_EXIT) fwrite(tlist.content,1,tlist.len,stdout);
          cf_str_cleanup(&tlist);
          cf_cleanup_thread(&thread);
        }
      }
    }

    /* sorting algorithms are allowed */
    #else

    #ifdef CF_SHARED_MEM
    if(cf_get_threadlist(&threads,ptr1) == -1)
    #else
    if(cf_get_threadlist(&threads,sock,&tsd) == -1)
    #endif
    {
      if(*ErrorString) cf_error_message(ErrorString,NULL);
      else cf_error_message("E_NO_THREADLIST",NULL);

      return;
    }
    else {
      #ifdef CF_SHARED_MEM
      cf_run_sorting_handlers(head,ptr,&threads);
      #else
      cf_run_sorting_handlers(head,sock,&tsd,&threads);
      #endif

      for(i=0;i<threads.elements;++i) {
        threadp = cf_array_element_at(&threads,i);

        if((threadp->messages->invisible == 0 && threadp->messages->may_show) || del == CF_KEEP_DELETED) {
          cf_str_init(&tlist);
          if(cf_gen_threadlist(threadp,head,&tlist,rm->threadlist_thread_tpl,"full",rm->posting_uri[UserName?1:0],CF_MODE_THREADLIST) != FLT_EXIT) fwrite(tlist.content,1,tlist.len,stdout);
          cf_str_cleanup(&tlist);
        }
      }

      cf_array_destroy(&threads);
    }
    #endif

    #ifndef CF_SHARED_MEM
    if(*ErrorString) {
      cf_error_message(ErrorString,NULL);
      return;
    }
    #else
    if(ptr1 == NULL && *ErrorString) {
      cf_error_message(ErrorString,NULL);
      return;
    }
    #endif

    cf_tpl_parse(&tpl_end);
    cf_tpl_finish(&tpl_end);
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

    fprintf(fd,"fo_view: Got signal %s!\nUsername: %s\nQuery-String: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
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
  /* {{{ variables */
  #ifndef CF_SHARED_MEM
  int sock;
  #else
  void *sock;
  #endif

  static const u_char *wanted[] = {
    "fo_default", "fo_view"
  };

  int ret;
  u_char  *ucfg,*UserName,*fname;
  cf_array_t *cfgfiles;
  cf_hash_t *head;
  cf_configfile_t conf,dconf;
  cf_name_value_t *cs = NULL;
  cf_name_value_t *pt;
  u_char *forum_name = NULL;

  cf_readmode_t rm_infos;

  u_int64_t tid = 0,mid = 0;

  cf_string_t *m  = NULL,*t = NULL,*mode = NULL;
  /* }}} */

  /* {{{ set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);
  /* }}} */

  /* {{{ initialization */
  if((cfgfiles = cf_get_conf_file(wanted,2)) == NULL) {
    fprintf(stderr,"Could not find configuration files...\n");
    return EXIT_FAILURE;
  }

  cf_cfg_init();
  init_modules();
  cf_init();
  cf_htmllib_init();

  #ifndef CF_SHARED_MEM
  sock = 0;
  #else
  sock = NULL;
  #endif

  ret  = FLT_OK;
  /* }}} */

  /* {{{ read configuration */
  fname = *((u_char **)cf_array_element_at(cfgfiles,0));
  cf_cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)cf_array_element_at(cfgfiles,1));
  cf_cfg_init_file(&conf,fname);
  free(fname);

  cf_cfg_register_options(&dconf,default_options);
  cf_cfg_register_options(&conf,fo_view_options);

  if(cf_read_config(&dconf,NULL,CF_CFG_MODE_CONFIG) != 0 || cf_read_config(&conf,NULL,CF_CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ ensure that CF_FORUM_NAME is set and we have got a context in every file */
  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile") == NULL) {
    fprintf(stderr,"Have no context for forum %s in default configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cf_cfg_get_first_value(&fo_view_conf,forum_name,"ParamType") == NULL) {
    fprintf(stderr,"Have no context for forum %s in fo_view configuration file!\n",forum_name);

    cf_cfg_cleanup_file(&conf);
    cf_cfg_cleanup_file(&dconf);

    cf_cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }
  /* }}} */

  pt = cf_cfg_get_first_value(&fo_view_conf,forum_name,"ParamType");
  head = cf_cgi_new();
  if(*pt->values[0] == 'P') cf_cgi_parse_path_info_nv(head);

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(head);

  /* {{{ read user configuration */
  if(ret != FLT_EXIT && (UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    ucfg = cf_get_uconf_name(UserName);
    if(ucfg) {
      free(conf.filename);
      conf.filename = ucfg;

      if(cf_read_config(&conf,ignre,CF_CFG_MODE_USER) != 0) {
        fprintf(stderr,"config file error!\n");

        cf_cfg_cleanup_file(&conf);
        cf_cfg_cleanup_file(&dconf);

        cf_cfg_destroy();
        cf_fini();

        return EXIT_FAILURE;
      }
    }
  }
  /* }}} */

  /* run init handlers */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(head);

  cs = cf_cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  /* {{{ get readmode information */
  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    if((ret = cf_run_readmode_collectors(head,&fo_view_conf,&rm_infos)) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      fprintf(stderr,"cf_run_readmode_collectors() returned %d!\n",ret);
      cf_error_message("E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }
  /* }}} */

  if(ret != FLT_EXIT) {
    /* {{{ now, we need a socket connection/shared mem pointer */
    #ifndef CF_SHARED_MEM
    if((sock = cf_socket_setup()) < 0) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      cf_error_message("E_NO_SOCK",NULL,strerror(errno));
      exit(0);
    }
    #else
    if((sock = cf_get_shm_ptr()) == NULL) {
      printf("Content-Type: text/html; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      cf_error_message("E_NO_CONN",NULL,strerror(errno));
      exit(0);
    }
    #endif
    /* }}} */

    /* run connect init handlers */
    ret = cf_run_connect_init_handlers(head,sock);

    if(ret != FLT_EXIT) {
      /* after that, look for m= and t= */
      if(head) {
        t    = cf_cgi_get(head,"t");
        m    = cf_cgi_get(head,"m");
        mode = cf_cgi_get(head,"mode");
      }

      if(t) tid = cf_str_to_uint64(t->content);
      if(m) mid = cf_str_to_uint64(m->content);

      if(mode && cf_strcmp(mode->content,"xmlhttp") == 0 && t) show_xmlhttp_thread(head,sock,tid,mid);
      else {
        if(tid) show_posting(head,sock,tid,mid);
        else    show_threadlist(sock,head);
      }
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cf_cfg_cleanup_file(&dconf);
  cf_cfg_cleanup_file(&conf);

  cf_array_destroy(cfgfiles);
  free(cfgfiles);

  cf_cleanup_modules(Modules);
  cf_fini();
  cf_cfg_destroy();

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}

/* eof */
