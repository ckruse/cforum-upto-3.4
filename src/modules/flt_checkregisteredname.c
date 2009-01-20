/*
 * \file flt_preview.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Posting preview
 *
 * This file is a plugin for fo_post. It gives the user the
 * ability to preview his postings.
 *
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <unistd.h>

#include <time.h>

#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_post.h"
#include "userconf.h"
#include "fo_userconf.h"
/* }}} */

/* {{{ flt_checkregisteredname_execute */
#ifdef CF_SHARED_MEM
int flt_checkregisteredname_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *ptr,int sock,int mode)
#else
int flt_checkregisteredname_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  u_char *username = cf_hash_get(GlobalValues,"UserName",8);
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  u_char *line;
  cf_string_t *name;

  rline_t rsd;

  cf_string_t str;

  if(!head) return FLT_DECLINE;
  memset(&rsd,0,sizeof(rsd));

  name = cf_cgi_get(head,"Name");
  cf_str_init(&str);

  cf_str_chars_append(&str,"SELECT ",7);
  cf_str_chars_append(&str,forum_name,strlen(forum_name));
  cf_str_chars_append(&str,"\nAUTH CHECK\nName: ",18);
  cf_str_str_append(&str,name);
  cf_str_char_append(&str,'\n');

  if(username) {
    cf_str_chars_append(&str,"Pass: ",6);
    cf_str_chars_append(&str,username,strlen(username));
    cf_str_char_append(&str,'\n');
  }

  cf_str_char_append(&str,'\n');

  writen(sock,str.content,str.len);
  cf_str_cleanup(&str);

  if((line = readline(sock,&rsd)) == NULL) return FLT_DECLINE;
  free(line);
  if((line = readline(sock,&rsd)) == NULL) return FLT_DECLINE;

  if(cf_strncmp(line,"200",3) != 0) {
    strcpy(ErrorString,"E_auth_required");
    display_posting_form(head,p,NULL);

    free(line);
    return FLT_EXIT;
  }

  free(line);
  return FLT_OK;
}
/* }}} */

/* {{{ flt_checkregisteredname_register */
int flt_checkregisteredname_register(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *uc,cf_configuration_t *oldconf,cf_uconf_userconfig_t *newconf) {
  u_char
    buff[512],
    *line,
    *oldname = NULL,*newname,*oldregistered = NULL,*newregistered,
    *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),
    *uname = cf_hash_get(GlobalValues,"UserName",8);

  cf_name_value_t *cs = cf_cfg_get_first_value(dc,fn,"ExternCharset"),*on,*or;
  int sock,status,doer;
  size_t len;
  rline_t tsd;
  cf_string_t str;

  if(!uname) return FLT_DECLINE; /* cannot happen, but who knows... */

  if((sock = cf_socket_setup()) < 0) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    show_edit_content(cgi,"E_NO_CONN","cgi",0,NULL);
    return FLT_EXIT;
  }

  memset(&tsd,0,sizeof(tsd));

  /* {{{ select right forum */
  len = snprintf(buff,512,"SELECT %s\n",fn);
  writen(sock,buff,len);

  line = readline(sock,&tsd);
  if(!line || atoi(line) != 200) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    if(!line) show_edit_content(cgi,"E_NO_CONN","cgi",0,NULL);
    else {
      snprintf(buff,512,"E_FO_%d",atoi(line));
      show_edit_content(cgi,buff,"cgi",0,NULL);
    }

    return FLT_EXIT;
  }
  /* }}} */

  free(line);

  on            = cf_cfg_get_first_value(oldconf,fn,"Name");
  or            = cf_cfg_get_first_value(oldconf,fn,"RegisteredName");
  newname       = (u_char *)cf_uconf_get_conf_val(newconf,"Name",0);
  newregistered = (u_char *)cf_uconf_get_conf_val(newconf,"RegisteredName",0);

  if(on) oldname = on->values[0];
  if(or) oldregistered = or->values[0];

  /* in this case we don't need to do anything: nothing changed */
  if(oldregistered && cf_strcmp(oldregistered,"yes") == 0 && newregistered && cf_strcmp(newregistered,"yes") == 0 && oldname && newname && cf_strcmp(oldname,newname) == 0) return FLT_OK;

  /* {{{ oldregistered and newregistered set, but either newname or oldname is not set or unequal */
  else if(oldregistered && cf_strcmp(oldregistered,"yes") == 0 && newregistered && cf_strcmp(newregistered,"yes") == 0) {
    cf_str_init_growth(&str,512);

    /* {{{ we shall register a new name but the new name is not given; so just unregister old name */
    if(oldname && !newname) {
      cf_str_char_set(&str,"AUTH DELETE\nName: ",18);
      cf_str_cstr_append(&str,oldname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */
    /* {{{ we shall change name registration */
    else if(oldname && newname) {
      cf_str_char_set(&str,"AUTH SET\nName: ",15);
      cf_str_cstr_append(&str,oldname);
      cf_str_chars_append(&str,"\nNew-Name: ",11);
      cf_str_cstr_append(&str,newname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */
    /* {{{ oldname not set, register new one */
    else {
      cf_str_char_set(&str,"AUTH SET\nNew-Name: ",19);
      cf_str_cstr_append(&str,newname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */

    writen(sock,str.content,str.len);
    cf_str_cleanup(&str);

    if((line = readline(sock,&tsd)) == NULL) {
      close(sock);
      printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      show_edit_content(cgi,"E_NO_CONN","cgi",0,NULL);
      return FLT_EXIT;
    }

    if((status = atoi(line)) != 200) {
      free(line);
      close(sock);

      if(status == 500) {
        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        show_edit_content(cgi,"E_FO_500","cgi",0,NULL);
      }
      else {
        printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        show_edit_content(cgi,"E_FO_504","cgi",0,NULL);
      }

      return FLT_EXIT;
    }

    free(line);
    close(sock);
    return FLT_OK;
  }
  /* }}} */

  /* {{{ oldregistered and newregistered set, but one of them is "no" */
  else if(oldregistered && newregistered) {
    doer = 0;
    cf_str_init_growth(&str,512);

    /* {{{ delete old registration */
    if(cf_strcmp(oldregistered,"yes") == 0 && cf_strcmp(newregistered,"no") == 0 && oldname) {
      doer = 1;
      cf_str_char_set(&str,"AUTH DELETE\nName: ",18);
      cf_str_cstr_append(&str,oldname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */
    else if(cf_strcmp(oldregistered,"no") == 0 && cf_strcmp(newregistered,"yes") == 0 && newname) {
      doer = 1;
      cf_str_char_set(&str,"AUTH SET\nNew-Name: ",19);
      cf_str_cstr_append(&str,newname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }

    if(doer) {
      writen(sock,str.content,str.len);
      cf_str_cleanup(&str);

      if((line = readline(sock,&tsd)) == NULL) {
        close(sock);
        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        show_edit_content(cgi,"E_NO_CONN","cgi",0,NULL);
        return FLT_EXIT;
      }

      if((status = atoi(line)) != 200) {
        free(line);
        close(sock);

        if(status == 500) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          show_edit_content(cgi,"E_FO_500","cgi",0,NULL);
        }
        else {
          printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          show_edit_content(cgi,"E_FO_504","cgi",0,NULL);
        }

        return FLT_EXIT;
      }

      free(line);
    }

    close(sock);
    return FLT_OK;
  }
  /* }}} */

  /* {{{ either oldregistered or newregistered not set */
  else {
    doer = 0;
    cf_str_init_growth(&str,512);

    /* {{{ delete old auth */
    if(oldregistered && !newregistered && cf_strcmp(oldregistered,"yes") == 0 && oldname) {
      doer = 1;
      cf_str_char_set(&str,"AUTH DELETE\nName: ",18);
      cf_str_cstr_append(&str,oldname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */
    /* {{{ set new auth */
    else if(!oldregistered && newregistered && cf_strcmp(newregistered,"yes") == 0 && newname) {
      doer = 1;
      cf_str_char_set(&str,"AUTH SET\nNew-Name: ",19);
      cf_str_cstr_append(&str,newname);
      cf_str_chars_append(&str,"\nPass: ",7);
      cf_str_cstr_append(&str,uname);
      cf_str_chars_append(&str,"\n\n",2);
    }
    /* }}} */

    if(doer) {
      writen(sock,str.content,str.len);
      cf_str_cleanup(&str);

      if((line = readline(sock,&tsd)) == NULL) {
        close(sock);
        printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
        show_edit_content(cgi,"E_NO_CONN","cgi",0,NULL);
        return FLT_EXIT;
      }

      if((status = atoi(line)) != 200) {
        free(line);
        close(sock);

        if(status == 500) {
          printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          show_edit_content(cgi,"E_FO_500","cgi",0,NULL);
        }
        else {
          printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
          show_edit_content(cgi,"E_FO_504","cgi",0,NULL);
        }

        return FLT_EXIT;
      }

      free(line);
    }

    close(sock);
    return FLT_OK;
  }
  /* }}} */

  return FLT_DECLINE;
}
/* }}} */

cf_conf_opt_t flt_checkregisteredname_config[] = {
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_checkregisteredname_handlers[] = {
  { NEW_POST_HANDLER,    flt_checkregisteredname_execute },
  { UCONF_WRITE_HANDLER, flt_checkregisteredname_register },
  { 0, NULL }
};

cf_module_config_t flt_checkregisteredname = {
  MODULE_MAGIC_COOKIE,
  flt_checkregisteredname_config,
  flt_checkregisteredname_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
