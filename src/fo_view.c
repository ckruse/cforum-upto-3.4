/**
 * \file fo_view.c
 * \author Christian Kruse, <cjk@wwwtech.de>
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include <inttypes.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_view.h"
/* }}} */

/* {{{ show_xmlhttp_thread */
#ifndef CF_SHARED_MEM
void show_xmlhttp_thread(cf_cfg_config_t *cfg,cf_hash_t *head,int sock,u_int64_t tid,u_int64_t mid)
#else
void show_xmlhttp_thread(cf_cfg_config_t *cfg,cf_hash_t *head,void *shm_ptr,u_int64_t tid,u_int64_t mid)
#endif
{
  int ret;
  u_char *content_type = cf_hash_get(GlobalValues,"OutputContentType",17);
  u_char fo_thread_tplname[256],buff[512],*line = NULL,*UserName = cf_hash_get(GlobalValues,"UserName",8),*fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  cf_cfg_config_value_t *fo_thread_tpl,*cs,*tplmode,*lang;
  int show_invi = cf_hash_get(GlobalValues,"ShowInvisible",13) != NULL;
  cf_cl_thread_t thread;
  cf_string_t str;
  size_t len;
  rline_t tsd;
  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  #ifdef CF_SHARED_MEM
  int sock;
  cf_cfg_config_value_t *sockpath = cf_cfg_get_value(cfg,"DF:SocketName");
  #endif

  memset(&tsd,0,sizeof(tsd));

  tplmode       = cf_cfg_get_value(cfg,"DF:TemplateMode");
  lang          = cf_cfg_get_value(cfg,"DF:Language");
  fo_thread_tpl = cf_cfg_get_value(cfg,"TemplateForumThread");
  cs            = cf_cfg_get_value(cfg,"DF:ExternCharset");

  cf_gen_tpl_name(fo_thread_tplname,256,tplmode->sval,lang->sval,fo_thread_tpl->sval);

  #ifdef CF_SHARED_MEM
  if((sock = cf_socket_setup(sockpath->sval)) == -1) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    cf_error_message(cfg,"E_NO_CONN",NULL,strerror(errno));
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
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    cf_error_message(cfg,"E_DATA_FAILURE",NULL);
    return;
  }

  free(line);
  ret = cf_get_message_through_sock(sock,&tsd,&thread,tid,0,show_invi ? CF_KEEP_DELETED : CF_KILL_DELETED);

  if(ret == -1) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    if(*ErrorString) cf_error_message(cfg,ErrorString,NULL);
    else cf_error_message(cfg,"E_DATA_FAILURE",NULL);
    return;
  }

  thread.threadmsg = thread.messages;

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(cfg,head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(cfg,head,sock,&tsd,&thread);
  #endif
  #endif

  cf_str_init(&str);
  if(cf_gen_threadlist(cfg,&thread,head,&str,rm->threadlist_thread_tpl,"full",rm->posting_uri[UserName?1:0],CF_MODE_THREADLIST) != FLT_EXIT) {
    printf("Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    fwrite(str.content,1,str.len,stdout);
    cf_str_cleanup(&str);
  }
  else {
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    cf_error_message(cfg,"E_DATA_FAILURE",NULL);
  }

  cf_cleanup_thread(&thread);
}
/* }}} */

/* {{{ show_posting */
#ifndef CF_SHARED_MEM
void show_posting(cf_cfg_config_t *cfg,cf_hash_t *head,int sock,u_int64_t tid,u_int64_t mid)
#else
void show_posting(cf_cfg_config_t *cfg,cf_hash_t *head,void *shm_ptr,u_int64_t tid,u_int64_t mid)
#endif
{
  cf_cl_thread_t thread;

  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #endif

  int uname;

  u_char buff[256],*tmp,*content_type = cf_hash_get(GlobalValues,"OutputContentType",17);

  cf_cfg_config_value_t *tm  = cf_cfg_get_value(cfg,"ThreadMode"),
    *rm             = cf_cfg_get_value(cfg,"ReadMode"),
    *cs             = cf_cfg_get_value(cfg,"DF:ExternCharset"),
    *fbase          = NULL,
    *name           = cf_cfg_get_value(cfg,"Name"),
    *email          = cf_cfg_get_value(cfg,"EMail"),
    *hpurl          = cf_cfg_get_value(cfg,"HomepageUrl"),
    *imgurl         = cf_cfg_get_value(cfg,"ImageUrl"),
    *ps = NULL,
    *reg = NULL;

  cf_template_t tpl;

  cf_readmode_t *rmi = cf_hash_get(GlobalValues,"RM",2);

  size_t len;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  #ifdef CF_SHARED_MEM
  cf_cfg_config_value_t *shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  int shmids[3] = { shminf->avals[0].ival,shminf->avals[1].ival,shminf->avals[2].ival };
  #endif

  memset(&thread,0,sizeof(thread));

  if(mid == 0) {
    free(rm->sval);
    rm->sval = strdup(tm->sval);
    if(cf_run_readmode_collectors(cfg,head,rmi) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
      cf_error_message(cfg,"E_CONFIG_ERR",NULL);
      return;
    }
  }

  /* {{{ init and get message from server */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(cf_tpl_init(&tpl,rmi->thread_tpl) != 0) {
    cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
    return;
  }

  #ifndef CF_SHARED_MEM
  if(cf_get_message_through_sock(sock,&tsd,&thread,tid,mid,del) == -1)
  #else
  if(cf_get_message_through_shm(shmids,shm_ptr,&thread,tid,mid,del) == -1)
  #endif
  {
    if(cf_strcmp(ErrorString,"E_FO_404") == 0) {
      if(cf_run_404_handlers(cfg,head,tid,mid) != FLT_EXIT) {
        printf("Status: 404 Not Found\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
        cf_error_message(cfg,ErrorString,NULL);
      }
    }
    else {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
      cf_error_message(cfg,ErrorString,NULL);
    }
    return;
  }
  /* }}} */

  if(mid == 0) thread.threadmsg = thread.messages;

  /* {{{ set standard variables */
  cf_tpl_setvalue(&tpl,"charset",TPL_VARIABLE_STRING,cs->sval,strlen(cs->sval));

  uname = cf_hash_get(GlobalValues,"UserName",8) != 0;
  fbase = cf_cfg_get_value(cfg,"DF:BaseURL");
  ps    = cf_cfg_get_value(cfg,"DF:PostScript");
  reg   = cf_cfg_get_value(cfg,"UserManage");

  tmp = cf_get_link(fbase->avals[uname].sval,0,0);
  cf_set_variable(&tpl,cs->sval,"forum-base-uri",tmp,strlen(tmp),1);
  free(tmp);

  cf_set_variable(&tpl,cs->sval,"postscript",ps->avals[uname].sval,strlen(ps->avals[uname].sval),1); //TODO: new-posting-uri
  cf_set_variable(&tpl,cs->sval,"userconfig-uri",reg->avals[uname].sval,strlen(ps->avals[uname].sval),1);
  cf_set_variable(&tpl,cs->sval,"regscript",reg->avals[2].sval,strlen(reg->avals[2].sval),1); //TODO: usermanage-uri

  len = snprintf(buff,256,"%" PRIu64,thread.tid);
  cf_set_variable(&tpl,cs->sval,"thread-id",buff,len,0);
  len = snprintf(buff,256,"%" PRIu64,thread.threadmsg->mid);
  cf_set_variable(&tpl,cs->sval,"message-id",buff,len,0);

  /* user values */
  if(name && *name->sval) cf_set_variable(&tpl,cs->sval,"aname",name->sval,strlen(name->sval),1); //TODO: user-name
  if(email && *email->sval) cf_set_variable(&tpl,cs->sval,"aemail",email->sval,strlen(email->sval),1); //TODO: user-email
  if(hpurl && *hpurl->sval) cf_set_variable(&tpl,cs->sval,"aurl",hpurl->sval,strlen(hpurl->sval),1); // TODO: user-url
  if(imgurl && *imgurl->sval) cf_set_variable(&tpl,cs->sval,"aimg",imgurl->sval,strlen(imgurl->sval),1); //TODO: user-img

  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"start",TPL_VARIABLE_INT,1); //TODO: start-message
  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"msgnum",TPL_VARIABLE_INT,thread.msg_len); // TODO: message-count
  cf_tpl_hashvar_setvalue(&thread.messages->hashvar,"answers",TPL_VARIABLE_INT,thread.msg_len-1); // TODO: message-answers
  if(uname) cf_tpl_setvalue(&tpl,"authed",TPL_VARIABLE_INT,1); // TODO: user-is-authed
  cf_tpl_setvalue(&tpl,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION));
  /* }}} */

  printf("Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);

  #ifndef CF_NO_SORTING
  #ifdef CF_SHARED_MEM
  cf_run_thread_sorting_handlers(cfg,head,shm_ptr,&thread);
  #else
  cf_run_thread_sorting_handlers(cfg,head,sock,&tsd,&thread);
  #endif
  #endif

  if(cf_run_posting_handlers(cfg,head,&thread,&tpl) != FLT_EXIT) cf_tpl_parse(&tpl);
  cf_tpl_finish(&tpl);

  cf_cleanup_thread(&thread);
}

/* }}} */

/* {{{ show_threadlist */
#ifndef CF_SHARED_MEM
void show_threadlist(cf_cfg_config_t *cfg,int sock,cf_hash_t *head)
#else
void show_threadlist(cf_cfg_config_t *cfg,void *shm_ptr,cf_hash_t *head)
#endif
{
  size_t len;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line,buff[128];
  int ret;
  #else
  void *ptr,*ptr1;
  #endif

  u_char *ltime,*content_type = cf_hash_get(GlobalValues,"OutputContentType",17);

  #ifndef CF_SHARED_MEM
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  #endif

  int uname = cf_hash_get(GlobalValues,"UserName",8) != NULL;

  cf_cfg_config_value_t *cs = cf_cfg_get_value(cfg,"DF:ExternCharset"),
    *fbase         = NULL,
    *pbase         = NULL,
    *time_fmt      = cf_cfg_get_value(cfg,"DateFormatLoadTime"),
    *time_lc       = cf_cfg_get_value(cfg,"DF:DateLocale");

  cf_template_t tpl_begin,tpl_end;

  time_t tm;
  cf_cl_thread_t thread;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  cf_string_t tlist;

  cf_readmode_t *rm = cf_hash_get(GlobalValues,"RM",2);

  #ifndef CF_NO_SORTING
  size_t i;
  cf_cl_thread_t *threadp;
  cf_array_t threads;
  #endif

  #ifdef CF_SHARED_MEM
  cf_cfg_config_value_t *shminf = cf_cfg_get_value(cfg,"DF:SharedMemIds");
  int shmids[3] = { shminf->avals[0].ival,shminf->avals[1].ival,shminf->avals[2].ival };
  #endif

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
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);

    if(line) {
      ret = snprintf(buff,128,"E_FO_%d",atoi(line));
      cf_error_message(cfg,buff,NULL);
      free(line);
    }
    else cf_error_message(cfg,"E_NO_THREADLIST",NULL);
  }
  #else
  if(!ptr) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
    cf_error_message(cfg,"E_NO_CONN",NULL,strerror(errno));
  }
  #endif
  /* }}} */

  /*
   * Request of shm segment/threadlist wen't through,
   * go on with work
   */
  else {
    /* {{{ more initialization */
    fbase    = cf_cfg_get_value(cfg,"DF:BaseURL");
    pbase    = cf_cfg_get_value(cfg,"DF:PostScript");

    if(cf_tpl_init(&tpl_begin,rm->pre_threadlist_tpl) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);

      cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
      return;
    }
    if(cf_tpl_init(&tpl_end,rm->post_threadlist_tpl) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);

      cf_error_message(cfg,"E_TPL_NOT_FOUND",NULL);
      return;
    }

    cf_set_variable(&tpl_begin,cs->sval,"forumbase",fbase->avals[uname].sval,strlen(fbase->avals[uname].sval),1); //TODO: forum-base-uri
    cf_set_variable(&tpl_end,cs->sval,"forumbase",fbase->avals[uname].sval,strlen(fbase->avals[uname].sval),1); //TODO: forum-base-uri
    cf_set_variable(&tpl_begin,cs->sval,"postscript",pbase->avals[uname].sval,strlen(pbase->avals[uname].sval),1); //TODO: new-posting-uri
    cf_set_variable(&tpl_end,cs->sval,"postscript",pbase->avals[uname].sval,strlen(pbase->avals[uname].sval),1); //TODO: new-posting-uri
    cf_tpl_setvalue(&tpl_begin,"charset",TPL_VARIABLE_STRING,cs->sval,strlen(cs->sval));
    cf_tpl_setvalue(&tpl_end,"charset",TPL_VARIABLE_STRING,cs->sval,strlen(cs->sval));
    cf_tpl_setvalue(&tpl_begin,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION),1);
    cf_tpl_setvalue(&tpl_end,"cf_version",TPL_VARIABLE_STRING,CF_VERSION,strlen(CF_VERSION),1);
    /* }}} */

    /* run some plugins */
    cf_run_view_init_handlers(cfg,head,&tpl_begin,&tpl_end);

    #ifndef CF_SHARED_MEM
    free(line);
    #endif

    thread.tid      = 0;
    thread.messages = NULL;
    thread.last     = NULL;
    thread.msg_len  = 0;

    tm    = time(NULL);
    ltime = cf_general_get_time(time_fmt->sval,time_lc->sval,&len,&tm);
    if(ltime) {
      cf_set_variable(&tpl_begin,cs->sval,"LOAD_TIME",ltime,len,1); //TODO: forum-load-time
      free(ltime);
    }

    /* ok, seems to be all right, send headers */
    printf("Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);

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
          if(cf_gen_threadlist(cfg,&thread,head,&tlist,rm->threadlist_thread_tpl,"full",rm->posting_uri[uname],CF_MODE_THREADLIST) != FLT_EXIT) fwrite(tlist.content,1,tlist.len,stdout);
          cf_str_cleanup(&tlist);
          cf_cleanup_thread(&thread);
        }
      }
    }

    /* sorting algorithms are allowed */
    #else

    #ifdef CF_SHARED_MEM
    if(cf_get_threadlist(&threads,shmids,ptr1) == -1)
    #else
    if(cf_get_threadlist(&threads,sock,&tsd) == -1)
    #endif
    {
      if(*ErrorString) cf_error_message(cfg,ErrorString,NULL);
      else cf_error_message(cfg,"E_NO_THREADLIST",NULL);

      return;
    }
    else {
      #ifdef CF_SHARED_MEM
      cf_run_sorting_handlers(cfg,head,ptr,&threads);
      #else
      cf_run_sorting_handlers(cfg,head,sock,&tsd,&threads);
      #endif

      for(i=0;i<threads.elements;++i) {
        threadp = cf_array_element_at(&threads,i);

        if((threadp->messages->invisible == 0 && threadp->messages->may_show) || del == CF_KEEP_DELETED) {
          cf_str_init(&tlist);
          if(cf_gen_threadlist(cfg,threadp,head,&tlist,rm->threadlist_thread_tpl,"full",rm->posting_uri[uname],CF_MODE_THREADLIST) != FLT_EXIT) fwrite(tlist.content,1,tlist.len,stdout);
          cf_str_cleanup(&tlist);
        }
      }

      cf_array_destroy(&threads);
    }
    #endif

    #ifndef CF_SHARED_MEM
    if(*ErrorString) {
      cf_error_message(cfg,ErrorString,NULL);
      return;
    }
    #else
    if(ptr1 == NULL && *ErrorString) {
      cf_error_message(cfg,ErrorString,NULL);
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
  #ifndef CF_SHARED_MEM
  int sock;
  cf_cfg_config_value_t *sockpath;
  #else
  void *sock;
  int shmids[3];
  cf_cfg_config_value_t *shminf;
  #endif

  static const u_char *wanted[] = {
    "fo_default", "fo_view"
  };

  int ret;
  u_char  *ucfg,*UserName,*content_type;
  cf_hash_t *head;
  cf_cfg_config_value_t *cs = NULL,*pt,*uconfpath;
  u_char *forum_name = NULL;

  cf_readmode_t rm_infos;

  u_int64_t tid = 0,mid = 0;

  cf_string_t *m  = NULL,*t = NULL,*mode = NULL;

  cf_cfg_config_t cfg;

  /* {{{ set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);
  /* }}} */

  /* {{{ initialization */
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
  if(cf_cfg_get_conf(&cfg,wanted,2) != 0) {
    cf_fini();
    fprintf(stderr,"config file error\n");
    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ ensure that CF_FORUM_NAME is set and we have got a context in every file */
  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    cf_fini();
    fprintf(stderr,"Could not get forum name!");
    return EXIT_FAILURE;
  }
  /* }}} */

  uconfpath = cf_cfg_get_value(&cfg,"DF:ConfigDirectory");
  pt        = cf_cfg_get_value(&cfg,"ParamType");
  head      = cf_cgi_new();
  if(*pt->sval == 'P') cf_cgi_parse_path_info_nv(head);

  /* first action: authorization modules */
  ret = cf_run_auth_handlers(&cfg,head);

  /* {{{ read user configuration */
  if(ret != FLT_EXIT && (UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    if((ucfg = cf_get_uconf_name(uconfpath->sval,UserName)) != NULL) {
      if(cf_cfg_read_conffile(&cfg,ucfg) != 0) {
        free(ucfg);
        fprintf(stderr,"config file error!\n");

        cf_cfg_config_destroy(&cfg);
        cf_fini();

        return EXIT_FAILURE;
      }

      free(ucfg);
    }
  }
  /* }}} */

  /* run init handlers */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(&cfg,head);

  cs = cf_cfg_get_value(&cfg,"DF:ExternCharset");
  content_type = cf_hash_get(GlobalValues,"OutputContentType",17);

  /* {{{ get readmode information */
  if(ret != FLT_EXIT) {
    memset(&rm_infos,0,sizeof(rm_infos));
    if((ret = cf_run_readmode_collectors(&cfg,head,&rm_infos)) != FLT_OK) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: %s; charset=%s\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
      fprintf(stderr,"cf_run_readmode_collectors() returned %d!\n",ret);
      cf_error_message(&cfg,"E_CONFIG_ERR",NULL);
      ret = FLT_EXIT;
    }
    else cf_hash_set(GlobalValues,"RM",2,&rm_infos,sizeof(rm_infos));
  }
  /* }}} */

  if(ret != FLT_EXIT) {
    /* {{{ now, we need a socket connection/shared mem pointer */
    #ifndef CF_SHARED_MEM
    sockpath = cf_cfg_get_value(&cfg,"SocketPath");
    if((sock = cf_socket_setup(sockpath->sval)) < 0) {
      printf("Content-Type: %s; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
      cf_error_message(&cfg,"E_NO_SOCK",NULL,strerror(errno));
      exit(0);
    }
    #else
    shminf = cf_cfg_get_value(&cfg,"DF:SharedMemIds");
    shmids[0] = shminf->avals[0].ival;
    shmids[1] = shminf->avals[1].ival;
    shmids[2] = shminf->avals[2].ival;
    if((sock = cf_get_shm_ptr(shmids)) == NULL) {
      printf("Content-Type: %s; charset=%s\015\012Status: 500 Internal Server Error\015\012\015\012",content_type?content_type:(u_char *)"text/html",cs->sval);
      cf_error_message(&cfg,"E_NO_CONN",NULL,strerror(errno));
      exit(0);
    }
    #endif
    /* }}} */

    /* run connect init handlers */
    ret = cf_run_connect_init_handlers(&cfg,head,sock);

    if(ret != FLT_EXIT) {
      /* after that, look for m= and t= */
      if(head) {
        t    = cf_cgi_get(head,"t");
        m    = cf_cgi_get(head,"m");
        mode = cf_cgi_get(head,"mode");
      }

      if(t) tid = cf_str_to_uint64(t->content);
      if(m) mid = cf_str_to_uint64(m->content);

      if(mode && cf_strcmp(mode->content,"xmlhttp") == 0 && t) show_xmlhttp_thread(&cfg,head,sock,tid,mid);
      else {
        if(tid) show_posting(&cfg,head,sock,tid,mid);
        else    show_threadlist(&cfg,sock,head);
      }
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cf_fini();
  cf_cfg_config_destroy(&cfg);

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}

/* eof */
