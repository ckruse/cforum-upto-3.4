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
int ignre(t_configfile *cfile,const u_char *context,u_char *name,u_char **args,size_t len) {
  return 0;
}

/* {{{ print_thread_structure */
void print_thread_structure(t_cl_thread *thread,t_cf_hash *head) {
  t_message *msg;

  int level = 0,
      len = 0,
      printed = 0,
      rc = 0;

  u_char *date,
         *link,
         *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *cs  = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
               *dnv = cfg_get_first_value(&fo_view_conf,forum_name,"DateFormatThreadList"),
               *loc = cfg_get_first_value(&fo_default_conf,forum_name,"DateLocale"),
               *ot  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenThread"),
               *op  = cfg_get_first_value(&fo_view_conf,forum_name,"OpenPosting"),
               *ost = cfg_get_first_value(&fo_view_conf,forum_name,"OpenSubtree"),
               *cst = cfg_get_first_value(&fo_view_conf,forum_name,"CloseSubtree"),
               *cp  = cfg_get_first_value(&fo_view_conf,forum_name,"ClosePosting"),
               *ct  = cfg_get_first_value(&fo_view_conf,forum_name,"CloseThread");

  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  size_t ot_l  = strlen(ot->values[0]),
         op_l  = strlen(op->values[0]),
         ost_l = strlen(ost->values[0]),
         cst_l = strlen(cst->values[0]),
         cp_l  = strlen(cp->values[0]),
         ct_l  = strlen(ct->values[0]);

  for(msg=thread->messages;msg;msg=msg->next) {
    if((msg->may_show && msg->invisible == 0) || ShowInvisible == 1) {
      printed = 1;

      date = cf_general_get_time(dnv->values[0],loc->values[0],&len,&msg->date);
      if(date) {
        cf_set_variable(&msg->tpl,cs,"time",date,len,1);
        free(date);
      }

      cf_set_variable(&msg->tpl,cs,"author",msg->author.content,msg->author.len,1);
      cf_set_variable(&msg->tpl,cs,"title",msg->subject.content,msg->subject.len,1);

      if(msg->category.len) cf_set_variable(&msg->tpl,cs,"category",msg->category.content,msg->category.len,1);

      link = cf_get_link(NULL,forum_name,thread->tid,msg->mid);
      if(link) {
        cf_set_variable(&msg->tpl,cs,"link",link,strlen(link),1);
        free(link);
      }

      if(msg->level < level) {
        for(;level>msg->level;level--) {
          fwrite(cst->values[0],1,cst_l,stdout);
          fwrite(cp->values[0],1,cp_l,stdout);
        }
      }

      level = msg->level;

      if(msg->next && cf_msg_has_answers(msg)) { /* this message has at least one answer */
        /* if first messages, write OpenThread (ot), else write OpenPosting (op) */
        if(msg == thread->messages) fwrite(ot->values[0],1,ot_l,stdout);
        else fwrite(op->values[0],1,op_l,stdout);

        cf_tpl_parse(&msg->tpl);
        fwrite(ost->values[0],1,ost_l,stdout);

        ++level;
      }
      else {
        if(msg == thread->messages) fwrite(ot->values[0],1,ot_l,stdout);
        else fwrite(op->values[0],1,op_l,stdout);
        cf_tpl_parse(&msg->tpl);
        if(msg == thread->messages) fwrite(ct->values[0],1,ct_l,stdout);
        else fwrite(cp->values[0],1,cp_l,stdout);
      }
    }
  }

  for(;level>1;level--) {
    fwrite(cst->values[0],1,cst_l,stdout);
    fwrite(cp->values[0],1,cp_l,stdout);
  }

  /* last closing is for closing the thread */
  if(level == 1) {
    fwrite(cst->values[0],1,cst_l,stdout);
    fwrite(ct->values[0],1,ct_l,stdout);
  }
}
/* }}} */

/* {{{ show_posting */
#ifndef CF_SHARED_MEM
void show_posting(t_cf_hash *head,int sock,u_int64_t tid,u_int64_t mid)
#else
void show_posting(t_cf_hash *head,void *shm_ptr,u_int64_t tid,u_int64_t mid)
#endif
{
  t_cl_thread thread;

  #ifndef CF_SHARED_MEM
  rline_t tsd;
  #endif

  u_char fo_thread_tplname[256],
         fo_posting_tplname[256],
         *UserName = cf_hash_get(GlobalValues,"UserName",8),
         *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  t_name_value *fo_thread_tpl  = NULL,
               *fo_posting_tpl = cfg_get_first_value(&fo_view_conf,forum_name,"TemplatePosting"),
               *cs             = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
               *rm             = cfg_get_first_value(&fo_view_conf,forum_name,"ReadMode"),
               *fbase          = NULL,
               *name           = cfg_get_first_value(&fo_view_conf,NULL,"Name"),
               *email          = cfg_get_first_value(&fo_view_conf,NULL,"EMail"),
               *hpurl          = cfg_get_first_value(&fo_view_conf,NULL,"HomepageUrl"),
               *imgurl         = cfg_get_first_value(&fo_view_conf,NULL,"ImageUrl");

  t_cf_template tpl;

  size_t len;
  char buff[128];
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  memset(&thread,0,sizeof(thread));


  if(cf_strcmp(rm->values[0],"thread") == 0)          fo_thread_tpl = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumThread");
  else if(cf_strcmp(rm->values[0],"threadlist") == 0) fo_thread_tpl = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumThreadList");
  else if(cf_strcmp(rm->values[0],"list") == 0)       fo_thread_tpl = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumList");


  /* {{{ init and get message from server */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(!fo_thread_tpl || !fo_posting_tpl) {
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  cf_gen_tpl_name(fo_thread_tplname,256,fo_thread_tpl->values[0]);
  cf_gen_tpl_name(fo_posting_tplname,256,fo_posting_tpl->values[0]);

  if(cf_tpl_init(&tpl,fo_posting_tplname) != 0) {
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  #ifndef CF_SHARED_MEM
  if(cf_get_message_through_sock(sock,&tsd,&thread,fo_thread_tplname,tid,mid,del) == -1)
  #else
  if(cf_get_message_through_shm(shm_ptr,&thread,fo_thread_tplname,tid,mid,del) == -1)
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

  /* {{{ set standard variables */
  cf_tpl_setvalue(&tpl,"charset",TPL_VARIABLE_STRING,cs->values[0],strlen(cs->values[0]));

  UserName = cf_hash_get(GlobalValues,"UserName",8);
  if(UserName) fbase = cfg_get_first_value(&fo_default_conf,forum_name,"UBaseURL");
  else         fbase = cfg_get_first_value(&fo_default_conf,forum_name,"BaseURL");

  cf_set_variable(&tpl,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);

  /* user values */
  if(name && *name->values[0]) cf_set_variable(&tpl,cs,"aname",name->values[0],strlen(name->values[0]),1);
  if(email && *email->values[0]) cf_set_variable(&tpl,cs,"aemail",email->values[0],strlen(email->values[0]),1);
  if(hpurl && *hpurl->values[0]) cf_set_variable(&tpl,cs,"aurl",hpurl->values[0],strlen(hpurl->values[0]),1);
  if(imgurl && *imgurl->values[0]) cf_set_variable(&tpl,cs,"aimg",imgurl->values[0],strlen(imgurl->values[0]),1);

  cf_tpl_setvalue(&thread.messages->tpl,"start",TPL_VARIABLE_INT,1);
  cf_tpl_setvalue(&thread.messages->tpl,"msgnum",TPL_VARIABLE_INT,thread.msg_len);
  cf_tpl_setvalue(&thread.messages->tpl,"answers",TPL_VARIABLE_INT,thread.msg_len-1);
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

/* {{{ show_thread */
#ifndef CF_SHARED_MEM
void show_thread(t_cf_hash *head,int sock,u_int64_t tid)
#else
void show_thread(t_cf_hash *head,void *sock,u_int64_t tid)
#endif
{
  /** \todo implement it */
}
/* }}} */

/* {{{ show_threadlist */
#ifndef CF_SHARED_MEM
void show_threadlist(int sock,t_cf_hash *head)
#else
void show_threadlist(void *shm_ptr,t_cf_hash *head)
#endif
{
  /* {{{ variables */
  int ret,len;
  #ifndef CF_SHARED_MEM
  rline_t tsd;
  u_char *line,*tmp;
  #else
  void *ptr,*ptr1;
  #endif

  u_char fo_begin_tplname[256],
         fo_end_tplname[256],
         fo_thread_tplname[256],
         buff[128],
         *ltime,
         *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10),
         *UserName = cf_hash_get(GlobalValues,"UserName",8);

  t_name_value *fo_begin_tpl  = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumBegin"),
               *fo_end_tpl    = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumEnd"),
               *fo_thread_tpl = cfg_get_first_value(&fo_view_conf,forum_name,"TemplateForumThread"),
               *cs            = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset"),
               *fbase         = NULL,
               *time_fmt      = cfg_get_first_value(&fo_view_conf,forum_name,"DateFormatLoadTime"),
               *time_lc       = cfg_get_first_value(&fo_default_conf,forum_name,"DateLocale");

  t_cf_template tpl_begin,tpl_end;

  time_t tm;
  t_cl_thread thread,*threadp;
  t_message *msg;
  size_t i;
  int del = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? CF_KILL_DELETED : CF_KEEP_DELETED;

  #ifndef CF_NO_SORTING
  t_array threads;
  #endif
  /* }}} */

  /* {{{ initialization work */
  #ifndef CF_SHARED_MEM
  memset(&tsd,0,sizeof(tsd));
  #endif

  if(!fo_begin_tpl || !fo_end_tpl || !fo_thread_tpl) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  cf_gen_tpl_name(fo_begin_tplname,256,fo_begin_tpl->values[0]);
  cf_gen_tpl_name(fo_end_tplname,256,fo_end_tpl->values[0]);
  cf_gen_tpl_name(fo_thread_tplname,256,fo_thread_tpl->values[0]);
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
    fbase    = cfg_get_first_value(&fo_default_conf,forum_name,UserName ? "UBaseURL" : "BaseURL");

    if(cf_tpl_init(&tpl_begin,fo_begin_tplname) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

      cf_error_message("E_TPL_NOT_FOUND",NULL);
      return;
    }
    cf_set_variable(&tpl_begin,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);

    if(cf_tpl_init(&tpl_end,fo_end_tplname) != 0) {
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

      cf_error_message("E_TPL_NOT_FOUND",NULL);
      return;
    }

    cf_set_variable(&tpl_end,cs,"forumbase",fbase->values[0],strlen(fbase->values[0]),1);
    cf_tpl_setvalue(&tpl_begin,"charset",TPL_VARIABLE_STRING,cs->values[0],strlen(cs->values[0]));
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
    while(cf_get_next_thread_through_sock(sock,&tsd,&thread,fo_thread_tplname) == 0)
    #else
    while((ptr1 = cf_get_next_thread_through_shm(ptr1,&thread,fo_thread_tplname)) != NULL)
    #endif
    {
      if(thread.messages) {
        if((thread.messages->invisible == 0 && thread.messages->may_show) || del == CF_KEEP_DELETED) {
          cf_tpl_setvalue(&thread.messages->tpl,"start",TPL_VARIABLE_INT,1);
          cf_tpl_setvalue(&thread.messages->tpl,"msgnum",TPL_VARIABLE_INT,thread.msg_len);
          cf_tpl_setvalue(&thread.messages->tpl,"answers",TPL_VARIABLE_INT,thread.msg_len-1);

          /* first: run VIEW_HANDLER handlers in pre-mode */
          ret = cf_run_view_handlers(&thread,head,CF_MODE_THREADLIST|CF_MODE_PRE);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=thread.messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,thread.tid,CF_MODE_THREADLIST);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(&thread,head,CF_MODE_THREADLIST|CF_MODE_POST);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) print_thread_structure(&thread,head);
          }

          cf_cleanup_thread(&thread);
        }
      }
    }

    /* sorting algorithms are allowed */
    #else

    #ifdef CF_SHARED_MEM
    if(cf_get_threadlist(&threads,ptr1,fo_thread_tplname) == -1)
    #else
    if(cf_get_threadlist(&threads,sock,&tsd,fo_thread_tplname) == -1)
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
        threadp = array_element_at(&threads,i);

        if((threadp->messages->invisible == 0 && threadp->messages->may_show) || del == CF_KEEP_DELETED) {
          cf_tpl_setvalue(&threadp->messages->tpl,"start",TPL_VARIABLE_INT,1);
          cf_tpl_setvalue(&threadp->messages->tpl,"msgnum",TPL_VARIABLE_INT,threadp->msg_len);
          cf_tpl_setvalue(&threadp->messages->tpl,"answers",TPL_VARIABLE_INT,threadp->msg_len-1);

          /* first: run VIEW_HANDLER handlers in pre-mode */
          ret = cf_run_view_handlers(threadp,head,CF_MODE_THREADLIST|CF_MODE_PRE);

          if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) {
            /* run list handlers */
            for(msg=threadp->messages;msg;msg=msg->next) cf_run_view_list_handlers(msg,head,threadp->tid,CF_MODE_THREADLIST);

            /* after that, run VIEW_HANDLER handlers in post-mode */
            ret = cf_run_view_handlers(threadp,head,CF_MODE_THREADLIST|CF_MODE_POST);

            /* if thread is still visible print it out */
            if(ret == FLT_OK || ret == FLT_DECLINE || del == CF_KEEP_DELETED) print_thread_structure(threadp,head);
          }
        }
      }

      array_destroy(&threads);
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
  u_char  *ucfg,*m  = NULL,*t = NULL,*UserName,*fname;
  t_array *cfgfiles;
  t_cf_hash *head;
  t_configfile conf,dconf;
  t_name_value *cs = NULL;
  t_name_value *pt;
  u_char *forum_name = NULL;

  u_int64_t tid = 0,mid = 0;
  /* }}} */

  /* {{{ set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);
  /* }}} */

  /* {{{ initialization */
  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    fprintf(stderr,"Could not find configuration files...\n");
    return EXIT_FAILURE;
  }

  cfg_init();
  init_modules();
  cf_init();

  #ifndef CF_SHARED_MEM
  sock = 0;
  #else
  sock = NULL;
  #endif

  ret  = FLT_OK;
  /* }}} */

  /* {{{ read configuration */
  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_view_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&conf,NULL,CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ ensure that CF_FORUM_NAME is set and we have got a context in every file */
  if((forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10)) == NULL) {
    fprintf(stderr,"Could not get forum name!");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cfg_get_first_value(&fo_default_conf,forum_name,"ThreadIndexFile") == NULL) {
    fprintf(stderr,"Have no context for forum %s in default configuration file!\n",forum_name);

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }

  if(cfg_get_first_value(&fo_view_conf,forum_name,"ParamType") == NULL) {
    fprintf(stderr,"Have no context for forum %s in fo_view configuration file!\n",forum_name);

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    cfg_destroy();
    cf_fini();

    return EXIT_FAILURE;
  }
  /* }}} */

  pt = cfg_get_first_value(&fo_view_conf,forum_name,"ParamType");
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

      if(read_config(&conf,ignre,CFG_MODE_USER) != 0) {
        fprintf(stderr,"config file error!\n");

        cfg_cleanup_file(&conf);
        cfg_cleanup_file(&dconf);

        cfg_destroy();
        cf_fini();

        return EXIT_FAILURE;
      }
    }
  }
  /* }}} */

  /* run init handlers */
  if(ret != FLT_EXIT) ret = cf_run_init_handlers(head);

  cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");

  if(ret != FLT_EXIT) {
    /* {{{ now, we need a socket connection/shared mem pointer */
    #ifndef CF_SHARED_MEM
    if((sock = cf_socket_setup()) < 0) {
      printf("Content-Type: text/html; charset=%s\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
      cf_error_message("E_NO_SOCK",NULL,strerror(errno));
      exit(0);
    }
    #else
    if((sock = cf_get_shm_ptr()) == NULL) {
      printf("Content-Type: text/html; charset=%s\015\012\015\012",cs?cs->values[0]?cs->values[0]:(u_char *)"UTF-8":(u_char *)"UTF-8");
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
        t = cf_cgi_get(head,"t");
        m = cf_cgi_get(head,"m");
      }

      if(t) tid = str_to_u_int64(t);
      if(m) mid = str_to_u_int64(m);

      if(tid && mid) show_posting(head,sock,tid,mid);
      else if(tid)   show_thread(head,sock,tid);
      else           show_threadlist(sock,head);
    }

    #ifndef CF_SHARED_MEM
    writen(sock,"QUIT\n",5);
    close(sock);
    #endif
  }

  /* cleanup source */
  cfg_cleanup_file(&dconf);
  cfg_cleanup_file(&conf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  cleanup_modules(Modules);
  cf_fini();
  cfg_destroy();

  if(head) cf_hash_destroy(head);

  #ifdef CF_SHARED_MEM
  if(sock) shmdt((void *)sock);
  #endif

  return EXIT_SUCCESS;
}

/* eof */
