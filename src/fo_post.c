/*
 * \file fo_post.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Implementation of fo_post
 *
 * This file implements fo_post in C
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
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>

#include <sys/types.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#ifdef CF_SHARED_MEM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <signal.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "charconvert.h"
#include "clientlib.h"
#include "validate.h"
#include "fo_post.h"
/* }}} */

/* {{{ run_after_post_filters */
void run_after_post_filters(t_cf_hash *head,t_message *p,u_int64_t tid) {
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_after_post_filter fkt;

  if(Modules[AFTER_POST_HANDLER].elements) {
    for(i=0;i<Modules[AFTER_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[AFTER_POST_HANDLER],i);
      fkt     = (t_after_post_filter)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_post_conf,p,tid);
    }
  }
}
/* }}} */

/* {{{ run_post_filters */
#ifdef CF_SHARED_MEM
int run_post_filters(t_cf_hash *head,t_message *p,void *sock)
#else
int run_post_filters(t_cf_hash *head,t_message *p,int sock)
#endif
{
  int ret = FLT_OK;
  t_handler_config *handler;
  size_t i;
  t_new_post_filter fkt;
  int fupto = cf_cgi_get(head,"fupto") != NULL;

  if(Modules[NEW_POST_HANDLER].elements) {
    for(i=0;i<Modules[NEW_POST_HANDLER].elements && (ret == FLT_OK || ret == FLT_DECLINE);i++) {
      handler = array_element_at(&Modules[NEW_POST_HANDLER],i);
      fkt     = (t_new_post_filter)handler->func;
      ret     = fkt(head,&fo_default_conf,&fo_view_conf,p,(int)sock,fupto);
    }
  }

  return ret;
}
/* }}} */

/* {{{ display_finishing_screen */
/**
 * Function for displaying the user the 'Ok, posting has been processed' site
 * \param p The message struct
 */
void display_finishing_screen(t_message *p) {
  t_cf_template tpl;
  u_char tplname[256];
  t_name_value *tt = cfg_get_first_value(&fo_post_conf,"OkTemplate");
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");;
  size_t len;
  u_char *val;
  t_mod_api msg_to_html = cf_get_mod_api_ent("msg_to_html");
  void *ptr = fo_alloc(NULL,1,sizeof(const u_char *) + 2 * sizeof(t_string *),FO_ALLOC_MALLOC);
  t_string *str = fo_alloc(NULL,1,sizeof(*str),FO_ALLOC_CALLOC);

  if(!tt) {
    str_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  generate_tpl_name(tplname,256,tt);

  if(tpl_cf_init(&tpl,tplname) != 0) {
    str_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  cf_set_variable(&tpl,cs,"Name",p->author,p->author_len,1);
  cf_set_variable(&tpl,cs,"subject",p->subject,p->subject_len,1);

  /* {{{ transform body to html and set it in the template */
  memcpy(ptr,&p->content,sizeof(const u_char *));
  memcpy(ptr+sizeof(const u_char *),&str,sizeof(str));
  memset(ptr+sizeof(const u_char *) + sizeof(str),0,sizeof(str));
  msg_to_html(ptr);

  cf_set_variable(&tpl,cs,"body",str->content,str->len,0);

  str_cleanup(str);
  free(str);
  /* }}} */

  if(p->email)    cf_set_variable(&tpl,cs,"EMail",p->email,p->email_len,1);
  if(p->category) cf_set_variable(&tpl,cs,"cat",p->category,p->category_len,1);
  if(p->hp)       cf_set_variable(&tpl,cs,"HomepageUrl",p->hp,p->hp_len,1);
  if(p->img)      cf_set_variable(&tpl,cs,"ImageUrl",p->img,p->img_len,1);

  tpl_cf_parse(&tpl);
  tpl_cf_finish(&tpl);
}
/* }}} */

/* {{{ display_posting_form */
void display_posting_form(t_cf_hash *head) {
  /* display him the fucking formular */
  t_cf_template tpl;
  u_char tplname[256];
  t_name_value *tt  = cfg_get_first_value(&fo_post_conf,"ThreadTemplate");
  t_name_value *cs  = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_cf_list_head *cats = cfg_get_value(&fo_default_conf,"Category");
  t_cf_list_element *catelem;
  t_name_value *catnm;
  size_t len;
  u_char *val;
  u_char *cat = NULL,*tmp;
  t_string catstr;
  
  if(!tt) {
    str_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  generate_tpl_name(tplname,256,tt);

  if(tpl_cf_init(&tpl,tplname) != 0) {
    str_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  /* set categories */
  str_init(&catstr);
  if(head) cat = cf_cgi_get(head,"cat");
  for(catelem=cats->elements;catelem;catelem=catelem->next) {
    catnm = catelem->data;
    tmp   = charset_convert_entities(catnm->values[0],strlen(catnm->values[0]),"UTF-8",cs->values[0],&len);

    str_chars_append(&catstr,"<option value=\"",15);
    str_chars_append(&catstr,tmp,len);
    str_char_append(&catstr,'"');

    if(cat && cf_strcmp(cat,catnm->values[0]) == 0) str_chars_append(&catstr," selected",9);

    str_char_append(&catstr,'>');
    str_chars_append(&catstr,tmp,len);
    str_chars_append(&catstr,"</option>\n",10);

    free(tmp);
  }
  tpl_cf_setvar(&tpl,"cats",catstr.content,catstr.len,0);
  str_cleanup(&catstr);

  if(*ErrorString) {
    val = get_error_message(ErrorString,&len);
    cf_set_variable(&tpl,cs,"error",val,len,1);
    *ErrorString = '0';
    free(val);

    tpl_cf_parse(&tpl);
  }
  else {
    /* run filters... */
    if(handle_posting_filters(head,NULL,&tpl,&fo_post_conf) != FLT_EXIT) {
      if(*ErrorString) {
        val = get_error_message(ErrorString,&len);
        cf_set_variable(&tpl,cs,"error",val,len,1);
        free(val);
      }

      tpl_cf_parse(&tpl);
    }
  }


  tpl_cf_finish(&tpl);
}
/* }}} */

/* {{{ normalize_cgi_variables */
/**
 * "Normalizes" the cgi input variables (means: everything to UTF-8)
 * and checks if strings are valid UTF-8 (in case of UTF-8 cgi params)
 * \param head The cgi hash
 * \param field_name The name of the field containing &#255;
 * \return 0 on success, -1 on failure
 */
int normalize_cgi_variables(t_cf_hash *head,const u_char *field_name) {
  size_t flen;
  u_char *field = cf_cgi_get(head,(u_char *)field_name);
  u_int32_t i;
  t_cf_hash_entry *ent;
  u_char *converted;
  t_name_value *cs = cfg_get_first_value(&fo_default_conf,"ExternCharset");
  t_cf_cgi_param *param;
  char buff[50];

  if(!field) return -1;

  /* UTF-8 is \194\173, so if first char is not \194 it is not UTF-8 */
  if(*field != 194) {
    /* transform everything to utf-8... */
    for(i=0;i<hashsize(head->tablesize);i++) {
      if(head->table[i]) {
        for(ent = head->table[i];ent;ent=ent->next) {
          for(param = (t_cf_cgi_param *)ent->data;param;param=param->next) {
            if((converted = charset_convert(param->value,strlen(param->value),cs->values[0],"UTF-8",NULL)) == NULL) {
              return -1;
            }

            free(param->value);
            param->value = converted;
          }
        }
      }
    }
  }
  else {
    /* input seems to be UTF-8, check if strings are valid UTF-8 */
    for(i=0;i<hashsize(head->tablesize);i++) {
      if(head->table[i]) {
        for(ent = head->table[i];ent;ent=ent->next) {
          for(param = (t_cf_cgi_param *)ent->data;param;param=param->next) {
            if(is_valid_utf8_string(param->value,strlen(param->value)) != 0) return -1;
          }
        }
      }
    }
  }

  field = cf_cgi_get(head,(u_char *)field_name);
  flen  = strlen(field);

  /* strip character from field */
  memcpy(buff,field+2,flen-2);
  memcpy(field,buff,flen-2);
  field[flen-2] = '\0';

  return 0;
}
/* }}} */

/* {{{ validate_cgi_variables */
/**
 * Validates the input cgi variables as defined in configuration
 * \param head The cgi hash
 * \return -1 on failure, 0 on success
 */
int validate_cgi_variables(t_cf_hash *head) {
  u_char *value;
  t_name_value *cfg;
  t_cf_list_head *list;
  t_cf_list_element *elem;

  size_t maxlen,minlen,len;
  int fupto = cf_cgi_get(head,"fupto") != NULL;

  if((list = cfg_get_value(&fo_post_conf,"FieldNeeded")) != NULL) {
    for(elem = list->elements;elem;elem=elem->next) {
      cfg = (t_name_value *)elem->data;

      if((value = cf_cgi_get(head,cfg->values[0])) == NULL || *value == '\0') {
        /*
         * ok, value doesn't exist. But it may be that it does not need
         * to exist in a particular case (FieldNeeded has two parameters)
         */
        if((cf_strcmp(cfg->values[2],"yes") == 0 && fupto) || (cf_strcmp(cfg->values[1],"yes") == 0 && !fupto)) {
          snprintf(ErrorString,50,"E_%s_missing",cfg->values[0]);
          return -1;
        }
      }
    }
  }

  if((list = cfg_get_value(&fo_post_conf,"FieldConfig")) != NULL) {
    for(elem=list->elements;elem;elem=elem->next) {
      cfg = (t_name_value *)elem->data;

      if((value = cf_cgi_get(head,cfg->values[0])) != NULL) {
        maxlen = minlen = 0;
        if(cfg->values[1]) maxlen = atoi(cfg->values[1]);
        if(cfg->values[2]) maxlen = atoi(cfg->values[2]);

        len = cf_strlen_utf8(value,strlen(value));
        if(maxlen && len > maxlen) {
          snprintf(ErrorString,50,"E_%s_long",cfg->values[0]);
          return -1;
        }

        /* bad boy, string is to short */
        if(minlen && len < minlen) {
          snprintf(ErrorString,50,"E_%s_short",cfg->values[0]);
          return -1;
        }
      }
    }
  }

  /* everything is fine */
  return 0;
}
/* }}} */

/* {{{ get_message_url */
int get_message_url(const u_char *msgstr,t_name_value **v) {
  t_name_value *ent;
  t_cf_list_element *elem;
  t_cf_list_head *list = cfg_get_value(&fo_post_conf,"Image");

  for(elem=list->elements;elem;elem=elem->next) {
    ent = (t_name_value *)elem->data;

    if(cf_strcasecmp(ent->values[0],msgstr) == 0) {
      *v = ent;
      return 0;
    }
  }

  return -1;
}
/* }}} */

/* {{{ body_plain2coded */
t_string *body_plain2coded(const u_char *text) {
  u_char *body;
  t_string *str = fo_alloc(NULL,1,sizeof(*str),FO_ALLOC_CALLOC);
  u_char *ptr,*end,*tmp,*qchars;
  t_string tmpstr;
  t_name_value *v;
  t_mod_api get_qchars = cf_get_mod_api_ent("get_qchars");
  size_t len;

  qchars = (u_char *)get_qchars(NULL);
  len    = strlen(qchars);

  /* first phase: we need the string entity encoded */
  body = htmlentities(text,0);

  /*
   * second phase:
   * - normalize newlines
   * - normalize [message:], [image:], [iframe:] and [link:]
   * - normalize quoting characters
   */
  for(ptr=body;*ptr;ptr++) {
    switch(*ptr) {
      case '\015':
        if(*(ptr+1) == '\012') ptr++;
        str_char_append(str,'\012');
        break;
      case '[':
        /* ok, this could be: [message:], [image:], [iframe:] or [link:] */
        switch(*(ptr+1)) {
          case 'm':
            if(cf_strncmp(ptr+1,"message:",8) == 0) {
              for(end=ptr;*end;end++) {
                if(isspace(*end) || *end == ']') break;
              }

              if(*end == ']') {
                tmp  = strndup(ptr+7,end-ptr-7);

                if(get_message_url(tmp,&v) == 0) {
                  str_chars_append(str,"<img src=\"",10);
                  str_chars_append(str,v->values[1],strlen(v->values[1]));
                  str_chars_append(str,"\" alt=\"",7);
                  str_chars_append(str,v->values[2],strlen(v->values[2]));
                  str_chars_append(str,"\">",2);
                }

                free(tmp);
              }
            }
            else str_char_append(str,*ptr);
            break;
          case 'i':
            if(cf_strncmp(ptr+1,"image:",6) == 0) {
              for(end=ptr;*end;end++) {
                if(isspace(*end) || *end == ']') break;
              }

              /* ok, we found a valid end */
              if(*end == ']') {
                tmp = strndup(ptr+7,end-ptr-7);

                /* is it a valid absolute url? */
                if(is_valid_link(tmp) == 0) {
                  str_chars_append(str,"<img src=\"",10);
                  str_chars_append(str,ptr+7,end-ptr-7);
                  str_chars_append(str,"\">",2);
                  ptr = end;
                }
                /* no... */
                else {
                  str_init(&tmpstr);
                  str_chars_append(&tmpstr,"http://example.org",18);
                  if(*tmp != '/') str_char_append(&tmpstr,'/');
                  str_chars_append(&tmpstr,tmp,end-ptr-7);

                  /* valid relative link? */
                  if(is_valid_link(tmpstr.content) == 0) {
                    str_chars_append(str,"<img src=\"",10);
                    str_chars_append(str,ptr+7,end-ptr-7);
                    str_chars_append(str,"\">",2);
                    ptr = end;
                  }

                  str_cleanup(&tmpstr);
                }

                free(tmp);
              }
              else str_char_append(str,*ptr);
            }
            else if(cf_strncmp(ptr+1,"iframe:",7) == 0) {
              for(end=ptr;*end;end++) {
                if(isspace(*end) || *end == ']') break;
              }

              /* ok, we found a valid end */
              if(*end == ']') {
                tmp = strndup(ptr+7,end-ptr-7);

                /* is it a valid absolute url? */
                if(is_valid_link(tmp) == 0) {
                  str_chars_append(str,"<iframe src=\"",13);
                  str_chars_append(str,ptr+7,end-ptr-7);
                  str_chars_append(str,"\" width=\"90%\" height=\"90%\"><a href=\"",36);
                  str_chars_append(str,ptr+7,end-ptr-7);
                  str_chars_append(str,"\">",2);
                  str_chars_append(str,ptr+7,end-ptr-7);
                  str_chars_append(str,"</a></iframe>",13);

                  ptr = end;
                }
                /* no... */
                else {
                  str_init(&tmpstr);
                  str_chars_append(&tmpstr,"http://example.org",18);
                  if(*tmp != '/') str_char_append(&tmpstr,'/');
                  str_chars_append(&tmpstr,tmp,end-ptr-7);

                  /* valid relative link? */
                  if(is_valid_link(tmpstr.content) == 0) {
                    str_chars_append(str,"<iframe src=\"",13);
                    str_chars_append(str,ptr+7,end-ptr-7);
                    str_chars_append(str,"\" width=\"90%\" height=\"90%\"><a href=\"",36);
                    str_chars_append(str,ptr+7,end-ptr-7);
                    str_chars_append(str,"\">",2);
                    str_chars_append(str,ptr+7,end-ptr-7);
                    str_chars_append(str,"</a></iframe>",13);

                    ptr = end;
                  }

                  str_cleanup(&tmpstr);
                }

                free(tmp);
              }
            }
            else str_char_append(str,*ptr);
            break;
          case 'l':
            if(cf_strncmp(ptr+1,"link:",5) == 0) {
              for(end=ptr;*end;end++) {
                if(isspace(*end) || *end == ']') break;
              }

              /* ok, we found a valid end */
              if(*end == ']') {
                tmp = strndup(ptr+7,end-ptr-7);

                /* is it a valid absolute url? */
                if(is_valid_link(tmp) == 0) {
                  str_chars_append(str,"<a href=\"",9);
                  str_chars_append(str,tmp,end-ptr-7);
                  str_chars_append(str,"\">",2);
                  str_chars_append(str,tmp,end-ptr-7);
                  str_chars_append(str,"</a>",4);

                  ptr = end;
                }
                /* no... */
                else {
                  str_init(&tmpstr);
                  str_chars_append(&tmpstr,"http://example.org",18);
                  if(*tmp != '/') str_char_append(&tmpstr,'/');
                  str_chars_append(&tmpstr,tmp,end-ptr-7);

                  /* valid relative link? */
                  if(is_valid_link(tmpstr.content) == 0) {
                    str_chars_append(str,"<a href=\"",9);
                    str_chars_append(str,tmp,end-ptr-7);
                    str_chars_append(str,"\">",2);
                    str_chars_append(str,tmp,end-ptr-7);
                    str_chars_append(str,"</a>",4);

                    ptr = end;
                  }

                  str_cleanup(&tmpstr);
                }

                free(tmp);
              }

            }
            else str_char_append(str,*ptr);
            break;
        }
      default:
        if(cf_strncmp(ptr,qchars,len) == 0) {
          str_char_append(str,(u_char)127);
        }
        else {
          str_char_append(str,*ptr);
        }
    }
  }

  /*
   * ok, third phase:
   * - strip whitespaces at the end of a line and string
   * - change more than one whitespace at the beginning to &nbsp;
   * - transform the signature to internal representation
   * - transform \n to <br />
   */

  free(body);
  body = str->content;
  str_init(str);

  for(ptr=body;*ptr;ptr++) {
    if(*ptr == '\012') {
      str_chars_append(str,"<br />",6);
    }
    else if(isspace(*ptr)) {
      for(end=ptr;*end && isspace(*end) && *end != '\012';end++);

      /* strip newlines at the end of the line */
      if(*end == '\012') {
        ptr = end - 1;
        continue;
      }

      /* transform spaces after newlines to &nbsp; */
      for(len=0;len<(size_t)(end-ptr-1);len++) {
        str_chars_append(str,"&nbsp;",6);
      }

      /* but leave exact one character */
      str_char_append(str,' ');

      ptr = end - 1;
    }
    else if(cf_strncmp(ptr,"-- \012",4) == 0) {
      str_chars_append(str,"_/_SIG_/_",9);
      ptr += 4;
    }
    else {
      str_char_append(str,*ptr);
    }
  }

  /* all phases finished, body has been normalized */
  return str;
}
/* }}} */

/* {{{ get_remote_addr */
/**
 * Tries to get the remote address of the user
 * \return The remote address or a fake address of 666.666.666.666
 */
u_char *get_remote_addr(void) {
  u_char *tmp;

  if((tmp = getenv("X_FORWARDED_FOR")) == NULL) {
    if((tmp = getenv("REMOTE_ADDR")) == NULL) {
      tmp = "666.666.666.666";
    }
  }

  return tmp;
}
/* }}} */

/* {{{ handle_post_command */
int handle_post_command(t_configfile *cfile,u_char *name,u_char **args,int argnum) {
  t_name_value tmp;
  t_cf_tree_dataset dt;
  const t_cf_tree_dataset *dt1;
  t_cf_list_head *head;

  tmp.values = args;
  tmp.valnum = argnum;
  tmp.name   = strdup(name);

  dt.key = name;

  if((dt1 = cf_tree_find(&fo_post_conf.directives,fo_post_conf.directives.root,&dt)) != NULL) {
    head = dt1->data;
    cf_list_append(head,&tmp,sizeof(tmp));
  }
  else {
    head = fo_alloc(NULL,1,sizeof(*head),FO_ALLOC_CALLOC);
    cf_list_append(head,&tmp,sizeof(tmp));

    dt.key = strdup(name);
    dt.data = head;

    cf_tree_insert(&fo_post_conf.directives,NULL,&dt);
  }

  return -1;
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

/* {{{ main */
/**
 * The main function of the forum poster. No command line switches used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  /* {{{ initialization */
  static const u_char *wanted[] = {
    "fo_default", "fo_post"
  };

  int ret;
  u_char  *ucfg,*val,buff[256];
  t_array *cfgfiles;
  t_cf_hash *head;
  t_configfile conf,dconf,uconf;
  t_name_value *cs = NULL,*cfg_val;
  u_char *UserName;
  u_char *fname;
  t_cl_thread thr;
  t_message *p;

  size_t len;

  t_string *str,str1;

  u_char *tidmid;
  u_int64_t tid,mid;

  #ifdef CF_SHARED_MEM
  void *shm;
  #endif

  int sock,new_thread = 0;
  rline_t rl;

  /* set signal handler for SIGSEGV (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);

  head = cf_cgi_new();

  memset(&rl,0,sizeof(rl));
  str_init(&str1);

  if((cfgfiles = get_conf_file(wanted,2)) == NULL) {
    return EXIT_FAILURE;
  }

  init_modules();
  cf_init();
  cfg_init();

  sock = 0;

  #ifdef CF_SHARED_MEM
  shm = NULL;
  #endif

  ret  = FLT_OK;
  /* }}} */

  /* {{{ read config */
  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_post_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&conf,NULL,CFG_MODE_CONFIG) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  /* {{{ authorize */
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
  /* }}} */

  /* {{{ read user config */
  if((UserName = cf_hash_get(GlobalValues,"UserName",8)) != NULL) {
    /* get user config */
    ucfg = get_uconf_name(UserName);
    if(ucfg) {
      free(conf.filename);
      conf.filename = ucfg;

      if(read_config(&conf,handle_post_command,CFG_MODE_USER) != 0) {
        fprintf(stderr,"config file error!\n");

        cfg_cleanup_file(&conf);
        cfg_cleanup_file(&dconf);
        cfg_cleanup_file(&uconf);

        return EXIT_FAILURE;
      }
    }
  }
  /* }}} */

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
    /* fine -- lets spit out http headers */
    printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);

    /* ok -- lets check if user want's to post or just want's to fill out the form */
    if(head) {
      /* ok, user gave us variables -- lets validate them */
      if(normalize_cgi_variables(head,"qchar") != 0) {
        str_error_message("E_manipulated",NULL);
      }
      else {
        /* everything seems to be fine, so lets validate user input */
        if(validate_cgi_variables(head) != 0) {
          display_posting_form(head);
        }
        else {
          /* go and normalize the posting body */
          val = cf_cgi_get(head,"body");
          str = body_plain2coded(val);

          tidmid = cf_cgi_get(head,"fupto");
          if(tidmid) {
            tid = strtoull(tidmid,(char **)&tidmid,10);
            mid = strtoull(tidmid+1,NULL,10);

            if(!tid || !mid) {
              display_posting_form(head);
              return EXIT_SUCCESS;
            }
          }
          else new_thread = 1;

          /* ok, everythings fine: all fields, all fields are long enough. Go and get parent posting */
          #ifdef CF_SHARED_MEM
          if((shm = get_shm_ptr()) == NULL) {
            str_error_message(ErrorString,NULL);
          }
          #else
          /* fatal error: could not connect */
          if((sock = set_us_up_the_socket()) == -1) {
            str_error_message(ErrorString,NULL,);
          }
          #endif
          else {
            #ifdef CF_SHARED_MEM
            if(new_thread == 0 && cf_get_message_through_shm(shm,&thr,NULL,tid,mid,CF_KILL_DELETED) == -1) {
              str_error_message(ErrorString,NULL);
            }
            #else
            if(new_thread == 0 && cf_get_message_through_sock(sock,&rl,&thr,NULL,tid,mid,CF_KILL_DELETED) == -1) {
              str_error_message(ErrorString,NULL);
            }
            #endif
            else {
              p = fo_alloc(NULL,1,sizeof(*p),FO_ALLOC_MALLOC);
              p->mid = 0;
              p->author      = strdup(cf_cgi_get(head,"Name"));
              p->author_len  = strlen(p->author);

              p->content     = str->content;
              p->content_len = str->len;

              free(str);

              /* we inherit subject if none given */
              if((p->subject = cf_cgi_get(head,"subject")) == NULL) {
                p->subject     = strdup(thr.threadmsg->subject);
                p->subject_len = thr.threadmsg->subject_len;
              }
              else p->subject_len = strlen(p->subject);

              /* we inherit category */
              if((p->category = cf_cgi_get(head,"cat")) == NULL) {
                p->category     = strdup(thr.threadmsg->category);
                p->category_len = thr.threadmsg->category_len;
              }
              else p->category_len = strlen(p->category);

              p->email = cf_cgi_get(head,"EMail");
              if(p->email) p->email_len = strlen(p->email);

              p->hp = cf_cgi_get(head,"HomepageUrl");
              if(p->hp) p->hp_len = strlen(p->hp);

              p->img = cf_cgi_get(head,"ImageUrl");
              if(p->img) p->img_len = strlen(p->img);

              p->date        = time(NULL);
              if(new_thread == 0) p->level = thr.threadmsg->level + 1;
              p->may_show    = 1;
              p->invisible   = 0;

              /* ok, we did everything we had to do, let filters run */
              #ifndef CF_SHARED_MEM
              if(run_post_filters(head,p,sock) != FLT_EXIT) {
              #else
              if(run_post_filters(head,p,shm) != FLT_EXIT) {
                /* filters finished... send posting */
                if((sock = set_us_up_the_socket()) == -1) {
                  str_error_message(ErrorString,NULL);
                }
              #endif

                if(tid && mid) len = snprintf(buff,256,"POST ANSWER t=%llu m=%llu\n",tid,mid);
                else           len = snprintf(buff,256,"POST THREAD\n");

                str_chars_append(&str1,buff,len);

                val = cf_cgi_get(head,"unid");
                str_chars_append(&str1,"Unid:",5);
                str_chars_append(&str1,val,strlen(val));

                str_chars_append(&str1,"\nAuthor:",8);
                str_chars_append(&str1,p->author,p->author_len);

                str_chars_append(&str1,"\nSubject:",9);
                str_chars_append(&str1,p->subject,p->subject_len);

                if(p->email) {
                  str_chars_append(&str1,"\nEMail:",7);
                  str_chars_append(&str1,p->email,p->email_len);
                }

                if(p->category) {
                  str_chars_append(&str1,"\nCategory:",10);
                  str_chars_append(&str1,p->category,p->category_len);
                }

                if(p->hp) {
                  if(cf_strcmp(p->hp,"http://")) {
                    str_chars_append(&str1,"\nHomepageUrl:",13);
                    str_chars_append(&str1,p->hp,p->hp_len);
                  }
                }

                if(p->img) {
                  if(cf_strcmp(p->img,"http://")) {
                    str_chars_append(&str1,"\nImageUrl:",10);
                    str_chars_append(&str1,p->img,p->img_len);
                  }
                }

                str_chars_append(&str1,"\nBody:",6);
                str_chars_append(&str1,p->content,p->content_len);

                val = get_remote_addr();
                str_chars_append(&str1,"\nRemoteAddr:",12);
                str_chars_append(&str1,val,strlen(val));

                str_chars_append(&str1,"\n\nQUIT\n",7);

                /* now: transmit everything */
                writen(sock,str1.content,str1.len);
                str_cleanup(&str1);

                /* ok, everything has been submitted to the forum. Now lets wait for an answer... */
                if((val = readline(sock,&rl)) == NULL) {
                  str_error_message("E_IO_ERR",NULL);
                }
                else {
                  if(cf_strncmp(val,"200",3)) {
                    ret = atoi(val);
                    free(val);
                    len = snprintf(buff,256,"E_FO_%d",ret);
                    str_error_message(buff,NULL);
                  }
                  else {
                    free(val);

                    /* yeah, posting has ben processed! now, get the new message id and the (new?) thread id */
                    if((val = readline(sock,&rl)) != NULL) {
                      if(cf_strncmp(val,"Mid:",4)) {
                        tid = strtoull(val+5,NULL,10);

                        if((val = readline(sock,&rl)) != NULL) mid = strtoull(val+5,NULL,10);
                        else                                   tid = 0;
                      }
                      else {
                        mid = strtoull(val+5,NULL,10);
                      }

                      cfg_val = cfg_get_first_value(&fo_post_conf,"RedirectOnPost");

                      p->mid = mid;
                      run_after_post_filters(head,p,tid);

                      if(cfg_val && cf_strcmp(cfg_val->values[0],"yes") == 0) {
                        cfg_val = cfg_get_first_value(&fo_default_conf,UserName ? "BaseURL" : "UBaseURL");
                        printf("Status: 302 Moved Temporarily\015\012Location: %s?t=%llu&m=%llu\015\012\015\012",cfg_val->values[0],tid,mid);
                      }
                      else {
                        display_finishing_screen(p);
                      }

                      free(p->author);
                      free(p->subject);
                      free(p->content);
                      if(p->email)    free(p->email);
                      if(p->category) free(p->category);
                      if(p->hp)       free(p->hp);
                      if(p->img)      free(p->img);
                      free(p);

                      close(sock);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    else {
      display_posting_form(head);
    }
  }

  /* cleanup source */
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup_file(&dconf);
  cfg_cleanup(&fo_post_conf);
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
/* }}} */

/* eof */
