/**
 * \file clientlib.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief client library functions
 *
 * This file contains some functions and datatypes used in client modus,
 * e.g. delete_subtree() or generate_tpl_name() or something like that
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
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <dlfcn.h>
#include <locale.h>

#include <pthread.h>

#include <pwd.h>

#include <db.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#endif

#ifdef CF_SHARED_MEM
#include "semaphores.h"
#endif

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "template.h"
#include "readline.h"
#include "charconvert.h"
#include "clientlib.h"
/* }}} */

/* user authentification */
t_cf_hash *GlobalValues = NULL;

t_cf_hash *APIEntries = NULL;

/* error string */
u_char ErrorString[50];

DB *Msgs = NULL;

#ifdef CF_SHARED_MEM
/* {{{ Shared memory functions */

static int   shm_id        = -1;   /**< Shared memory id */
static void *shm_ptr       = NULL; /**< Shared memory pointer */
static int   shm_lock_sem  = -1;   /**< semaphore showing which shared memory segment we shall use */

void *reget_shm_ptr() {
  t_name_value *shm  = cfg_get_first_value(&fo_default_conf,"SharedMemIds");
  unsigned short val;

  if(shm_ptr) {
    shmdt(shm_ptr);
    shm_ptr = NULL;
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    if((shm_id = shmget(atoi(shm->values[val]),0,0)) == -1) {
      return NULL;
    }

    /*
     * we don't have and don't *need* write-permissions.
     * So set SHM_RDONLY in the shmat-flag.
     */
    if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) {
      return NULL;
    }
  }

  return shm_ptr;
}

void *get_shm_ptr() {
  t_name_value *shm  = cfg_get_first_value(&fo_default_conf,"SharedMemIds");
  unsigned short val;

  if(shm_lock_sem == -1) {
    /* create a new segment */
    if((shm_lock_sem = semget(atoi(shm->values[2]),0,0)) == -1) {
      perror("semget");
      return NULL;
    }
  }

  if(cf_sem_getval(shm_lock_sem,0,1,&val) == 0) {
    val = val == 1 ? 0 : 1;

    if(shm_id == -1) {
      if((shm_id = shmget(atoi(shm->values[val]),0,0)) == -1) {
        perror("shmget");
        return NULL;
      }
    }

    if(shm_ptr == NULL) {
      /*
       * we don't have and don't *need* write-permissions.
       * So set SHM_RDONLY in the shmat-flag.
       */
      if((shm_ptr = shmat(shm_id,0,SHM_RDONLY)) == (void *)-1) {
        perror("shmat");
        return NULL;
      }
    }
  }

  return shm_ptr;
}

/* }}} */
#endif

/* {{{ get_uconf_name
 * Returns: u_char *    the userconfig filename or NULL
 * Parameters:
 *
 * this function searches for a user configuration
 *
 */
u_char *get_uconf_name(const u_char *uname) {
  u_char *path,*name;
  u_char *ptr;
  struct stat sb;
  t_name_value *confpath = cfg_get_first_value(&fo_default_conf,"ConfigDirectory");

  if(!uname) return NULL;

  name = strdup(uname);
  if(!name) return NULL;

  for(ptr = name;*ptr;ptr++) {
    if(isupper(*ptr)) {
      *ptr = tolower(*ptr);
    }
  }

  path = fo_alloc(NULL,strlen(confpath->values[0]) + strlen(name) + 12,1,FO_ALLOC_MALLOC);

  sprintf(path,"%s%c/%c/%c/%s.conf",confpath->values[0],name[0],name[1],name[2],name);

  if(stat(path,&sb) == -1) {
    fprintf(stderr,"user config file '%s' not found!\n",path);
    free(path);
    free(name);
    return NULL;
  }

  free(name);

  return path;
}
/* }}} */

/* {{{ set_us_up_the_socket
 * Returns: int   the socket
 * Parameters:
 *
 * this functions sets up the socket
 *
 */
int set_us_up_the_socket(void) {
  int sock;
  struct sockaddr_un addr;
  t_name_value *sockpath = cfg_get_first_value(&fo_default_conf,"SocketName");

  if(sockpath) {
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;
    (void)strncpy(addr.sun_path,sockpath->values[0],103);

    if((sock = socket(AF_LOCAL,SOCK_STREAM,0)) == -1) {
      strcpy(ErrorString,"E_NO_SOCK");
      return -1;
    }

    if((connect(sock,(struct sockaddr *)&addr,sizeof(addr))) != 0) {
      strcpy(ErrorString,"E_NO_CONN");
      return -1;
    }

    return sock;
  }

  strcpy(ErrorString,"E_CONFIG_ERR");
  return -1;
}
/* }}} */

/* {{{ generate_tpl_name */
void generate_tpl_name(u_char buff[],int len,t_name_value *v) {
  gen_tpl_name(buff,len,v->values[0]);
}
/* }}} */

/* {{{ gen_tpl_name */
void gen_tpl_name(u_char buff[],int len,const u_char *name) {
  t_name_value *vn = cfg_get_first_value(&fo_default_conf,"TemplateMode");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,"Language");

  snprintf(buff,len,name,lang->values[0],vn->values[0]);
}
/* }}} */

/* {{{ cf_set_variable */
void cf_set_variable(t_cf_template *tpl,t_name_value *cs,u_char *vname,const u_char *val,size_t len,int html) {
  u_char *tmp;
  size_t len1;

  if(cs) {
    if(cf_strcmp(cs->values[0],"UTF-8")) {
      if(html) {
        tmp = htmlentities_charset_convert(val,"UTF-8",cs->values[0],&len1,0);
        html = 0;
      }
      else {
        tmp = charset_convert_entities(val,len,"UTF-8",cs->values[0],&len1);
      }

      if(!tmp) { /* This should only happen if we use charset_convert() -- and we should not use it. */
        tpl_cf_setvar(tpl,vname,val,len,html);
      }
      else {
        tpl_cf_setvar(tpl,vname,tmp,len1,html);
        free(tmp);
      }
    }
    /* ExternCharset is also UTF-8 */
    else {
      tpl_cf_setvar(tpl,vname,val,len,html);
    }
  }
  else {
    tpl_cf_setvar(tpl,vname,val,len,html);
  }

}
/* }}} */

/* {{{ cf_get_first_visible */
t_message *cf_get_first_visible(t_message *msg) {
  register t_message *msg1;

  if(msg->may_show && msg->invisible == 0) return msg;
  for(msg1=msg;msg1;msg1=msg1->next) {
    if(msg1->may_show && msg1->invisible == 0) return msg1;
  }

  return msg;
}
/* }}} */

/* {{{ str_error_message
 * Returns: nothing
 * Parameters:
 *   - const u_char *msg the errorcode
 *
 * this function spits out an error message
 *
 */
void str_error_message(const u_char *err,FILE *out, ...) {
  t_name_value *v = cfg_get_first_value(&fo_default_conf,"ErrorTemplate");
  t_name_value *db = cfg_get_first_value(&fo_default_conf,"MessagesDatabase");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,"Language");
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_name_value *vs = cfg_get_first_value(&fo_default_conf,cf_hash_get(GlobalValues,"UserName",8) ? "UBaseURL" : "BaseURL");
  t_cf_template tpl;
  u_char tplname[256];
  u_char errname[256];
  va_list ap;

  u_char *buff = NULL,ibuff[256];
  register u_char *ptr;
  t_string msg;

  int ivar,ret;
  u_char *svar;

  size_t size;

  DBT key,value;

  if(v && db && lang) {
    generate_tpl_name(tplname,256,v);
    tpl_cf_init(&tpl,tplname);
    cf_set_variable(&tpl,cs,"forumbase",vs->values[0],strlen(vs->values[0]),1);

    if(tpl.tpl) {
      str_init(&msg);

      if(Msgs == NULL) {
        if((ret = db_create(&Msgs,NULL,0)) == 0) {
          if((ret = Msgs->open(Msgs,NULL,db->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
            fprintf(stderr,"DB->open(%s) error: %s\n",db->values[0],db_strerror(ret));
          }
        }
        else fprintf(stderr,"db_create() error: %s\n",db_strerror(ret));
      }

      if(Msgs) {
        memset(&key,0,sizeof(key));
        memset(&value,0,sizeof(value));

        size = snprintf(errname,256,"%s_%s",lang->values[0],err);

        key.data = errname;
        key.size = size;

        if(Msgs->get(Msgs,NULL,&key,&value,0) == 0) {
          buff = strndup(value.data,value.size);
        }
      }

      if(buff) {
        va_start(ap,out);

        for(ptr=buff;*ptr;ptr++) {
          if(*ptr == '%') {
            ptr++;

            switch(*ptr) {
              case '%':
                str_char_append(&msg,*ptr);
                break;

              case 's':
                svar = va_arg(ap,u_char *);
                str_chars_append(&msg,svar,strlen(svar));
                break;

              case 'd':
                ivar = va_arg(ap,int);
                size = snprintf(ibuff,50,"%d",ivar);
                str_chars_append(&msg,ibuff,50);
                break;

              default:
                str_char_append(&msg,*ptr);
                break;
            }
          }
          else {
            str_char_append(&msg,*ptr);
          }
        }

        va_end(ap);

        cf_set_variable(&tpl,cs,"error",msg.content,msg.len,1);
        str_cleanup(&msg);
        free(buff);

        if(out) {
          tpl_cf_parse_to_mem(&tpl);
          fwrite(tpl.parsed.content,1,tpl.parsed.len,out);
        }
        else {
          tpl_cf_parse(&tpl);
        }

        tpl_cf_finish(&tpl);
      }
      else {
        printf("Sorry, internal error, cannot do anything. Perhaps you should kick your system administrator.\n");
      }
    }
    else {
      printf("Sorry, could not find template file. I got error %s\n",err);
    }
  }
  else {
    printf("Sorry, but I could not find my configuration.\nI got error %s\n",err);
  }
}
/* }}} */

/* {{{ get_error_message */
u_char *get_error_message(const u_char *err,size_t *len, ...) {
  t_name_value *db = cfg_get_first_value(&fo_default_conf,"MessagesDatabase");
  t_name_value *lang = cfg_get_first_value(&fo_default_conf,"Language");
  va_list ap;

  u_char *buff = NULL,ibuff[256],errname[256];
  register u_char *ptr;
  t_string msg;

  int ivar,ret;
  u_char *svar;

  size_t size;

  DBT key,value;

  if(db && lang) {
    str_init(&msg);

    if(Msgs == NULL) {
      if((ret = db_create(&Msgs,NULL,0)) == 0) {
        if((ret = Msgs->open(Msgs,NULL,db->values[0],NULL,DB_BTREE,DB_RDONLY,0)) != 0) {
          fprintf(stderr,"DB->open(%s) error: %s\n",db->values[0],db_strerror(ret));
        }
      }
      else fprintf(stderr,"db_create() error: %s\n",db_strerror(ret));
    }

    if(Msgs) {
      memset(&key,0,sizeof(key));
      memset(&value,0,sizeof(value));

      size = snprintf(errname,256,"%s_%s",lang->values[0],err);

      key.data = errname;
      key.size = size;

      if(Msgs->get(Msgs,NULL,&key,&value,0) == 0) {
        buff = strndup(value.data,value.size);
      }
    }

    if(buff) {
      va_start(ap,len);

      for(ptr=buff;*ptr;ptr++) {
        if(*ptr == '%') {
          ptr++;

          switch(*ptr) {
            case '%':
              str_char_append(&msg,*ptr);
              break;

            case 's':
              svar = va_arg(ap,u_char *);
              str_chars_append(&msg,svar,strlen(svar));
              break;

            case 'd':
              ivar = va_arg(ap,int);
              size = snprintf(ibuff,50,"%d",ivar);
              str_chars_append(&msg,ibuff,50);
              break;

            default:
              str_char_append(&msg,*ptr);
              break;
          }
        }
        else {
          str_char_append(&msg,*ptr);
        }
      }

      va_end(ap);
      free(buff);
      if(len) *len = msg.len;
      return msg.content;
    }
  }

  return NULL;
}
/* }}} */

/* {{{ delete_subtree
 * Returns:            the next message
 * Parameters:
 *   - t_message *msg  the message structure
 *
 * This function deletes a posting subtree
 *
 */
t_message *delete_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next) {
    msg->may_show = 0;
  }

  return msg;
}
/* }}} */

/* {{{ next_subtree */
t_message *next_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->next;msg && msg->level > lvl;msg=msg->next);

  return msg;
}
/* }}} */

/* {{{ prev_subtree */
t_message *prev_subtree(t_message *msg) {
  int lvl = msg->level;

  for(msg=msg->prev;msg && msg->level > lvl;msg=msg->prev);

  return msg;
}
/* }}} */

/* {{{ parent_message */
t_message *parent_message(t_message *tmsg) {
  t_message *msg = NULL;

  for(msg=tmsg;msg;msg=msg->prev) {
    if(msg->level == tmsg->level - 1) return msg;
  }

  return NULL;
}
/* }}} */

/* {{{ cleanup_struct
 * Returns: nothing
 * Parameters:
 *   - t_cl_thread *thr   the thread structure
 *
 * this function frees reserved memory and closes templates
 *
 */
void cleanup_struct(t_cl_thread *thr) {
  t_message *msg = thr->messages,*last = thr->messages;

  for(;msg;msg=last) {
    last = msg->next;

    #ifndef CF_SHARED_MEM
    free(msg->author);
    free(msg->subject);

    if(msg->category) {
      free(msg->category);
    }

    if(msg->content) {
      free(msg->content);
    }

    if(msg->email) {
      free(msg->email);
    }

    if(msg->hp) {
      free(msg->hp);
    }

    if(msg->img) {
      free(msg->img);
    }
    #endif

    if(msg->tpl.tpl) tpl_cf_finish(&msg->tpl);

    free(msg);
  }

}
/* }}} */

/* {{{ get_link
 * Returns: u_char *    NULL or a string
 * Parameters:
 *   - u_int64_t tid  the thread id
 *   - u_int64_t mid  the message id
 *
 * this function creates the URL string
 *
 */
u_char *get_link(const u_char *link,u_int64_t tid,u_int64_t mid) {
  t_name_value *vs;
  register const u_char *ptr;
  t_string buff;

  str_init(&buff);

  if(link == NULL)  {
    vs = cfg_get_first_value(&fo_default_conf,cf_hash_get(GlobalValues,"UserName",8) ? "UPostingURL" : "PostingURL");
    if(vs) link = vs->values[0];
  }

  if(link) {
    for(ptr=link;*ptr;++ptr) {
      switch(*ptr) {
        case '%':
          if(*(ptr+1) == 't') {
            u_int64_to_str(&buff,tid);
            ptr += 1;
          }
          else if(*(ptr+1) == 'm') {
            u_int64_to_str(&buff,mid);
            ptr += 1;
          }
          else str_char_append(&buff,*ptr);

          break;
        default:
          str_char_append(&buff,*ptr);
      }
    }
  }

  return buff.content;
}
/* }}} */

/* {{{ advanced_get_link */
u_char *advanced_get_link(const u_char *link,u_int64_t tid,u_int64_t mid,const u_char *parameters,size_t plen,size_t *l) {
  register const u_char *ptr;
  t_string buff;
  int qm = 0;

  str_init(&buff);

  for(ptr=link;*ptr;++ptr) {
    switch(*ptr) {
      case '%':
        if(*(ptr+1) == 't') {
          u_int64_to_str(&buff,tid);
          ptr += 1;
        }
        else if(*(ptr+1) == 'm') {
          u_int64_to_str(&buff,mid);
          ptr += 1;
        }
        else str_char_append(&buff,*ptr);

        break;
      case '?':
        qm = 1;
      default:
        str_char_append(&buff,*ptr);
    }
  }

  if(qm) {
   str_char_append(&buff,'&');
   str_chars_append(&buff,parameters,plen);
  }
  else {
    str_char_append(&buff,'?');
    str_chars_append(&buff,parameters,plen);
  }

  if(l) *l = buff.len;
  return buff.content;
}
/* }}} */

/* {{{ has_answers
 * Returns: int         returns a true value if the msg has anwers, a false value if it has no answers
 * Parameters:
 *   - t_messages *msg  a pointer to the message
 *
 * as the name says: it checks, if the message has an answer
 *
 */
int has_answers(t_message *msg) {
  int lvl = msg->level;
  int ShowInvisible = cf_hash_get(GlobalValues,"ShowInvisible",13) == NULL ? 0 : 1;

  if(msg->next) {
    if((msg->next->may_show && msg->next->invisible == 0) || ShowInvisible == 1) {
      if(msg->next->level > msg->level) {
        return 1;
      }
      return 0;
    }
    else {
      for(msg=msg->next;msg && (msg->may_show == 0 || msg->invisible == 1) && ShowInvisible == 0;msg=msg->next);

      if(!msg) {
        return 0;
      }
      else {
        if((msg->invisible == 1 || msg->may_show == 0) && ShowInvisible == 0) return 0;

        if(msg->level > lvl) {
          return 1;
        }

        return 0;
      }
    }
  }
  else {
    return 0;
  }
}
/* }}} */

/* {{{ handle_thread_list_posting
 * Returns: int                 returns the return value of the last filter
 * Parameters:
 *   - t_message *m             a pointer to the message
 *   - t_cf_hash *head          a pointer to the cgi variable list
 *   - unsigned long long tid   the thread id
 *   - int mode                 the mode; 0 if in threadlist, 1 if in posting
 *
 * handles VIEW_LIST_HANDLER filters
 *
 */
int handle_thread_list_posting(t_message *p,t_cf_hash *head,u_int64_t tid,int mode) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_list_posting fkt;

  if(Modules[VIEW_LIST_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_LIST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[VIEW_LIST_HANDLER],i);
      fkt     = (t_filter_list_posting)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,p,tid,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ handle_thread
 * Returns: int
 * Parameters:
 *   - t_cl_thread *thr  the thread structure
 *
 * this function handles a completely read thread
 *
 */
int handle_thread(t_cl_thread *thr,t_cf_hash *head,int mode) {
  int    ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_list fkt;

  if(Modules[VIEW_HANDLER].elements) {
    for(i=0;i<Modules[VIEW_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[VIEW_HANDLER],i);
      fkt     = (t_filter_list)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,thr,mode);
    }
  }

  return ret;
}
/* }}} */

/* {{{ handle_posting_filters */
int handle_posting_filters(t_cf_hash *head,t_cl_thread *thr,t_cf_template *tpl,t_configuration *vc) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_filter_posting fkt;

  if(Modules[POSTING_HANDLER].elements) {
    for(i=0;i<Modules[POSTING_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[POSTING_HANDLER],i);
      fkt     = (t_filter_posting)handler->func;
      ret     = fkt(head,&fo_default_conf,vc,thr,tpl);
    }
  }

  return ret;
}
/* }}} */

/* {{{ general_get_time */
u_char *general_get_time(u_char *fmt,u_char *locale,int *len,time_t *date) {
  u_char *buff;
  struct tm *tptr;
  size_t ln;

  if(!setlocale(LC_TIME,locale)) {
    return NULL;
  }

  ln    = strlen(fmt) + 256;
  buff  = fo_alloc(NULL,ln,1,FO_ALLOC_MALLOC);
  tptr  = localtime(date);

  *len = strftime(buff,ln,fmt,tptr);

  return buff;
}
/* }}} */

/* {{{ get_time
 * Returns: u_char *         a pointer or NULL
 * Parameters:
 *   - const u_char *symbol the date symbol
 *   - int *len            the len pointer
 *
 * this function creates a date string
 *
 */
u_char *get_time(t_configuration *cfg,const u_char *symbol,int *len,time_t *date) {
  t_name_value *symb = cfg_get_first_value(cfg,symbol);
  t_name_value *loc  = cfg_get_first_value(&fo_default_conf,"DateLocale");

  if(!symb || !loc) {
    return NULL;
  }

  return general_get_time(symb->values[0],loc->values[0],len,date);
}
/* }}} */

/* {{{ get_next_thread_through_sock */
int cf_get_next_thread_through_sock(int sock,rline_t *tsd,t_cl_thread *thr,const u_char *tplname) {
  u_char *line,*chtmp;
  int shallRun = 1;
  t_message *x;
  int ok = 0;

  memset(thr,0,sizeof(*thr));

  /* now lets read the threadlist */
  while(shallRun) {
    line = readline(sock,tsd);

    if(line) {
      line[tsd->rl_len-1] = '\0';

      if(*line == '\0') {
        shallRun = 0;
      }
      else if(cf_strncmp(line,"THREAD t",8) == 0) {
        chtmp = strstr(line,"m");

        thr->messages           = fo_alloc(NULL,1,sizeof(t_message),FO_ALLOC_CALLOC);
        thr->last               = thr->messages;
        thr->last->mid          = strtoull(&line[chtmp-line+1],NULL,10);
        thr->messages->may_show = 1;
        thr->msg_len            = 1;
        thr->tid                = strtoull(line+8,NULL,10);

        if(tplname) {
          tpl_cf_init(&thr->last->tpl,tplname);
          tpl_cf_setvar(&thr->last->tpl,"start","1",1,0);
        }
      }
      else if(cf_strncmp(line,"MSG m",5) == 0) {
        x = thr->last;

        if(thr->last) {
          thr->last->next = fo_alloc(NULL,1,sizeof(t_message),FO_ALLOC_CALLOC);
          thr->last->next->prev = thr->last;
          thr->last       = thr->last->next;
        }
        else {
          thr->messages = fo_alloc(NULL,1,sizeof(t_message),FO_ALLOC_CALLOC);
          thr->last     = thr->messages;
        }

        thr->last->mid      = strtoull(&line[5],NULL,10);
        thr->last->may_show = 1;

        thr->msg_len++;

        if(tplname) {
          tpl_cf_init(&thr->last->tpl,tplname);
        }
      }
      else if(cf_strncmp(line,"Author:",7) == 0) {
        thr->last->author     = strdup(&line[7]);
        thr->last->author_len = tsd->rl_len - 8;
      }
      else if(cf_strncmp(line,"Visible:",8) == 0) {
        thr->last->invisible = line[8] == '0';
        if(thr->last->invisible) thr->msg_len--;
      }
      else if(cf_strncmp(line,"Subject:",8) == 0) {
        thr->last->subject     = strdup(&line[8]);
        thr->last->subject_len = tsd->rl_len - 9;
      }
      else if(cf_strncmp(line,"Category:",9) == 0) {
        thr->last->category     = strdup(&line[9]);
        thr->last->category_len = tsd->rl_len - 10;
      }
      else if(cf_strncmp(line,"Date:",5) == 0) {
        thr->last->date = strtoul(&line[5],NULL,10);
      }
      else if(cf_strncmp(line,"Level:",6) == 0) {
        thr->last->level = atoi(&line[6]);
      }
      else if(cf_strncmp(line,"Content:",8) == 0) {
        thr->last->content     = strdup(&line[8]);
        thr->last->content_len = tsd->rl_len - 9;
      }
      else if(cf_strncmp(line,"Homepage:",9) == 0) {
        thr->last->hp     = strdup(&line[9]);
        thr->last->hp_len = tsd->rl_len - 10;
      }
      else if(cf_strncmp(line,"Image:",6) == 0) {
        thr->last->img     = strdup(&line[6]);
        thr->last->img_len = tsd->rl_len - 7;
      }
      else if(cf_strncmp(line,"EMail:",6) == 0) {
        thr->last->email     = strdup(&line[6]);
        thr->last->email_len = tsd->rl_len - 7;
      }
      else if(cf_strncmp(line,"Votes-Good:",11) == 0) {
        thr->last->votes_good = strtoul(line+11,NULL,10);
      }
      else if(cf_strncmp(line,"Votes-Bad:",10) == 0) {
        thr->last->votes_bad = strtoul(line+10,NULL,10);
      }
      else if(cf_strncmp(line,"END",3) == 0) {
        shallRun = 0;
        ok = 1;
      }

      free(line);
    }
    else {
      shallRun = 0; /* connection broke */
      strcpy(ErrorString,"E_COMMUNICATE");
    }
  }

  if(ok) return 0;
  return -1;
}

#ifdef CF_SHARED_MEM
void *cf_get_next_thread_through_shm(void *shm_ptr,t_cl_thread *thr,const u_char *tplname) {
  register void *ptr = shm_ptr;
  u_int32_t post;
  struct shmid_ds shm_buf;
  void *ptr1 = get_shm_ptr();
  size_t len;
  int msglen;
  u_char buff[128];

  memset(thr,0,sizeof(*thr));

  if(shmctl(shm_id,IPC_STAT,&shm_buf) != 0) {
    strcpy(ErrorString,"E_CONFIG_ERR");
    return NULL;
  }

  /*
   *
   * CAUTION! Deep magic begins here! Do not edit if you don't know
   * what you are doing!
   *
   */

  /* Uh, oh, we are at the end of the segment */
  if(ptr >= ptr1 + shm_buf.shm_segsz) return NULL;

  /* thread id */
  thr->tid = *((u_int64_t *)ptr);
  ptr += sizeof(u_int64_t);

  /* message id */
  thr->msg_len = msglen = *((int32_t *)ptr);
  ptr += sizeof(int32_t);

  for(post = 0;post < msglen;post++) {
    if(thr->last == NULL) {
      thr->messages = thr->last = fo_alloc(NULL,1,sizeof(t_message),FO_ALLOC_CALLOC);
    }
    else {
      thr->last->next = fo_alloc(NULL,1,sizeof(t_message),FO_ALLOC_CALLOC);
      thr->last->next->prev = thr->last;
      thr->last       = thr->last->next;
    }

    if(tplname) tpl_cf_init(&thr->last->tpl,tplname);

    /* message id */
    thr->last->mid = *((u_int64_t *)ptr);
    ptr += sizeof(u_int64_t);

    /* length of subject */
    thr->last->subject_len = *((u_int32_t *)ptr) - 1;
    ptr += sizeof(u_int32_t);

    /* subject */
    thr->last->subject = ptr;
    ptr += thr->last->subject_len + 1;

    /* length of category */
    thr->last->category_len = *((u_int32_t *)ptr) - 1;
    ptr += sizeof(u_int32_t);

    /* category */
    if(thr->last->category_len) {
      thr->last->category = ptr;
      ptr += thr->last->category_len + 1;
    }

    /* content length */
    thr->last->content_len = *((u_int32_t *)ptr) - 1;
    ptr += sizeof(u_int32_t);

    /* content */
    thr->last->content = ptr;
    ptr += thr->last->content_len + 1;

    /* date */
    thr->last->date = *((time_t *)ptr);
    ptr += sizeof(time_t);

    /* level */
    thr->last->level = *((u_int16_t *)ptr);
    ptr += sizeof(u_int16_t);

    /* invisible */
    thr->last->invisible = *((u_int16_t *)ptr);
    thr->last->may_show  = 1;
    ptr += sizeof(u_int16_t);
    if(thr->last->invisible) thr->msg_len--;

    /* votings */
    thr->last->votes_good = *((u_int32_t *)ptr);
    ptr += sizeof(u_int32_t);

    thr->last->votes_bad = *((u_int32_t *)ptr);
    ptr += sizeof(u_int32_t);


    /* author length */
    thr->last->author_len = *((u_int32_t *)ptr) - 1;
    ptr += sizeof(u_int32_t);

    /* author */
    thr->last->author = ptr;
    ptr += thr->last->author_len + 1;

    /* email length */
    thr->last->email_len = *((u_int32_t *)ptr);
    ptr += sizeof(u_int32_t);

    /* email */
    if(thr->last->email_len) {
      thr->last->email = ptr;
      ptr += thr->last->email_len;
    }

    /* homepage length */
    thr->last->hp_len = *((u_int32_t *)ptr);
    ptr += sizeof(u_int32_t);

    /* homepage */
    if(thr->last->hp_len) {
      thr->last->hp = ptr;
      ptr += thr->last->hp_len;
    }

    /* image length */
    thr->last->img_len = *((u_int32_t *)ptr);
    ptr += sizeof(u_int32_t);

    /* image */
    if(thr->last->img_len) {
      thr->last->img = ptr;
      ptr += thr->last->img_len;
    }
  }

  if(tplname) {
    if(thr->messages) {
      tpl_cf_setvar(&thr->messages->tpl,"start","1",1,0);

      len = snprintf(buff,128,"%d",thr->msg_len);
      tpl_cf_setvar(&thr->messages->tpl,"msgnum",buff,len,0);

      len = snprintf(buff,128,"%d",thr->msg_len-1);
      tpl_cf_setvar(&thr->messages->tpl,"answers",buff,len,0);
    }
  }

  return ptr;
}
#endif
/* }}} */

/* {{{ get_message */
int cf_get_message_through_sock(int sock,rline_t *tsd,t_cl_thread *thr,const u_char *tplname,u_int64_t tid,u_int64_t mid,int del) {
  int len;
  u_char buff[128],*line;
  t_message *msg;

  memset(thr,0,sizeof(*thr));

  len  = snprintf(buff,128,"GET POSTING t%llu m%llu invisible=%d\n",tid,mid,del);
  writen(sock,buff,len);

  line = readline(sock,tsd);

  if(line && cf_strncmp(line,"200 Ok",6) == 0) {
    free(line);

    thr->tid      = tid;
    thr->messages = NULL;
    thr->last     = NULL;

    if(cf_get_next_thread_through_sock(sock,tsd,thr,tplname) < 0 && *ErrorString) {
      strcpy(ErrorString,"E_COMMUNICATION");
      return -1;
    }
    else {
      /* set thread message pointer to the right message */
      if(mid == 0) {
        thr->threadmsg = thr->messages;
      }
      else {
        for(msg = thr->messages;msg;msg=msg->next) {
          if(msg->mid == mid) {
            thr->threadmsg = msg;
            break;
          }
        }
      }
    }
  }
  else {
    /* bye, bye */
    if(line) {
      len = snprintf(ErrorString,50,"E_FO_%d",atoi(line));
      free(line);
    }
    else {
      strcpy(ErrorString,"E_COMMUNICATION");
    }

    return -1;
  }

  return 0;
}

#ifdef CF_SHARED_MEM
int cf_get_message_through_shm(void *shm_ptr,t_cl_thread *thr,const u_char *tplname,u_int64_t tid,u_int64_t mid,int del) {
  struct shmid_ds shm_buf;
  register void *ptr1;
  u_int64_t val = 0;
  size_t posts,post;
  t_message *msg;

  if(shmctl(shm_id,IPC_STAT,&shm_buf) != 0) {
    strcpy(ErrorString,"E_CONFIG_ERR");
    return -1;
  }

  /*
   *
   * CAUTION! Deep magic begins here! Do not edit if you don't know
   * what you are doing
   *
   */

  for(ptr1=shm_ptr+sizeof(time_t);ptr1 < shm_ptr+shm_buf.shm_segsz;) {
    /* first: tid */
    val   = *((u_int64_t *)ptr1);

    if(val == tid) break;

    ptr1 += sizeof(u_int64_t);

    /* after that: number of postings */
    posts = *((int32_t *)ptr1);
    ptr1 += sizeof(int32_t);

    for(post = 0;post < posts;post++) {
      /* then: message id */
      ptr1 += sizeof(u_int64_t);

      /* then: length of the subject + subject */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t) + val;

      /* length of the category + category */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t);
      if(val) ptr1 += val;

      /* length of the content + content */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t) + val;

      /* date, level, invisible, votes good, votes bad */
      ptr1 += sizeof(time_t) + sizeof(u_int16_t) + sizeof(u_int16_t) + sizeof(u_int32_t) + sizeof(u_int32_t);

      /* user name length + user name */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t) + val;

      /* email length + email */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t);
      if(val) ptr1 += val;

      /* homepage length + homepage */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t);
      if(val) ptr1 += val;

      /* image length + image */
      val = *((u_int32_t *)ptr1);
      ptr1 += sizeof(u_int32_t);
      if(val) ptr1 += val;
    }
  }

  /*
   *
   * Phew, deep magic ended
   *
   */

  if(ptr1 >= shm_ptr + shm_buf.shm_segsz) {
    strcpy(ErrorString,"E_FO_404");
    return -1;
  }

  if((ptr1 = cf_get_next_thread_through_shm(ptr1,thr,tplname)) == NULL) {
    return -1;
  }

  if(mid == 0) {
    thr->threadmsg = thr->messages;
  }
  else {
    for(msg=thr->messages;msg;msg=msg->next) {
      if(msg->mid == mid) {
        thr->threadmsg = msg;
        break;
      }
    }
  }

  if(!thr->threadmsg) {
    strcpy(ErrorString,"E_FO_404");
    return -1;
  }

  if((thr->messages->invisible == 1 || thr->threadmsg->invisible == 1) && del == CF_KILL_DELETED) {
    strcpy(ErrorString,"E_FO_404");
    return -1;
  }

  return 0;
}
#endif
/* }}} */

/* {{{ cf_register_mod_api_ent */
int cf_register_mod_api_ent(const u_char *mod_name,const u_char *unique_identifier,t_mod_api func) {
  size_t len2 = strlen(unique_identifier);
  t_mod_api_ent *ent;

  if((ent = cf_hash_get(APIEntries,(u_char *)unique_identifier,len2)) != NULL) {
    if(cf_strcmp(ent->mod_name,mod_name)) {
      return -1;
    }

    ent->function          = func;
  }
  else {
    ent                    = fo_alloc(NULL,1,sizeof(*ent),FO_ALLOC_MALLOC);
    ent->mod_name          = strdup(mod_name);
    ent->unique_identifier = strdup(unique_identifier);
    ent->function          = func;

    cf_hash_set(APIEntries,(u_char *)unique_identifier,len2,ent,sizeof(*ent));

    free(ent);
  }

  return 0;
}
/* }}} */

/* {{{ cf_unregister_mod_api_ent */
int cf_unregister_mod_api_ent(const u_char *unid) {
  size_t len1 = strlen(unid);
  t_mod_api_ent *ent;

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) {
    return -1;
  }

  cf_hash_entry_delete(APIEntries,(u_char *)unid,len1);
  return 0;
}
/* }}} */

/* {{{ cf_get_mod_api_ent */
t_mod_api cf_get_mod_api_ent(const u_char *unid) {
  t_mod_api_ent *ent;
  size_t len1 = strlen(unid);

  if((ent = cf_hash_get(APIEntries,(u_char *)unid,len1)) == NULL) {
    return NULL;
  }

  return ent->function;
}
/* }}} */

/* {{{ destroy_entry */
/**
 * private function for destroying a module api entry
 * \param elem The element
 */
void destroy_entry(void *elem) {
  t_mod_api_ent *a = (t_mod_api_ent *)elem;
  free(a->mod_name);
  free(a->unique_identifier);
}
/* }}} */

/* {{{ cf_init */
/**
 * library constructor function
 */
void cf_init(void) {
  GlobalValues = cf_hash_new(NULL);
  APIEntries = cf_hash_new(destroy_entry);
  memset(ErrorString,0,sizeof(ErrorString));
}
/* }}} */

/* {{{ cf_fini */
/**
 * library destructor function
 */
void cf_fini(void) {
  cf_hash_destroy(GlobalValues);
  cf_hash_destroy(APIEntries);
}
/* }}} */

/* eof */
