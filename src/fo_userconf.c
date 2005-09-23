/**
 * \file fo_userconf.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The forum userconfig program
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
#include <ctype.h>

#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>

/* socket includes */
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/un.h>

#include <db.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"

#include "userconf.h"
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

    fprintf(fd,"fo_userconf: Got signal %s!\nUsername: %s\nQuery-String: %s\n----\n",buff,uname?uname:(u_char *)"(null)",qs?qs:(u_char *)"(null)");
    fclose(fd);
  }

  exit(0);
}
/* }}} */

int cfg_compare(cf_tree_dataset_t *dt1,cf_tree_dataset_t *dt2);
void destroy_directive_list(cf_tree_dataset_t *dt);

static int inited = 0;
configuration_t glob_config;

/* {{{ handle_userconf_command */
int handle_userconf_command(configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t argnum) {
  conf_opt_t opt;
  int ret;
  u_char *fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  opt.name  = strdup(name);
  opt.data  = &glob_config;
  opt.flags = CFG_OPT_LOCAL|CFG_OPT_USER|CFG_OPT_CONFIG;

  ret = handle_command(NULL,&opt,fn,args,argnum);

  if(ret != -1) free(opt.name);

  return ret;
}
/* }}} */

/* {{{ show_edit_content */
void show_edit_content(cf_hash_t *head,const u_char *msg,const u_char *source,int saved,array_t *errors) {
  u_char tplname[256],*ucfg,*uname,*fn = cf_hash_get(GlobalValues,"FORUM_NAME",10),*tmp,buff[256];

  name_value_t *cval,
    *cs = cfg_get_first_value(&fo_default_conf,fn,"ExternCharset"),
    *tplnv = cfg_get_first_value(&fo_userconf_conf,fn,"Edit"),
    *cats = cfg_get_first_value(&fo_default_conf,fn,"Categories");

  configfile_t config;
  uconf_userconfig_t *modxml;
  cf_template_t tpl;
  uconf_directive_t *directive;
  uconf_argument_t *arg;
  name_value_t *value;
  cf_cgi_param_t *mult;
  string_t val,*emsg;
  cf_tpl_variable_t array,errmsgs;
  int utf8 = cf_strcmp(cs->values[0],"UTF-8") == 0;
  struct tm tm;
  time_t tval;

  size_t i,j;

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  if(inited == 0) {
    if((ucfg = cf_get_uconf_name(uname)) == NULL) {
      printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message("E_MUST_AUTH",NULL);
      return;
    }

    cfg_init_file(&config,ucfg);
    free(ucfg);

    if(read_config(&config,handle_userconf_command,CFG_MODE_USER) != 0) {
      printf("Status: 500 Internal Server Error\015\012COntent-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message("E_CONFIG_BROKEN",NULL);
      return;
    }

    inited = 1;
  }

  if((modxml = cf_uconf_read_modxml()) == NULL) {
    printf("Status: 500 Internal Server Error\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_dontknow",NULL);
    return;
  }

  cf_gen_tpl_name(tplname,256,tplnv->values[0]);
  if(cf_tpl_init(&tpl,tplname) != 0) {
    printf("Status: 500 Internal Server Error\015\012\015\012");
    cf_error_message("E_TPL_NOT_FOUND",NULL);
    return;
  }

  for(i=0;i<modxml->directives.elements;++i) {
    directive = array_element_at(&modxml->directives,i);
    if(directive->flags & CF_UCONF_FLAG_INVISIBLE) continue;

    /* {{{ get value in hierarchy */
    if((source && cf_strcmp(source,"cgi") != 0) || !source) {
      if((value = cfg_get_first_value(&glob_config,fn,directive->name)) == NULL) {
        if((value = cfg_get_first_value(&fo_userconf_conf,fn,directive->name)) == NULL) {
          if((value = cfg_get_first_value(&fo_view_conf,fn,directive->name)) == NULL) {
            value = cfg_get_first_value(&fo_default_conf,fn,directive->name);
          }
        }
      }
    }
    else value = NULL;
    /* }}} */

    for(j=0;j<directive->arguments.elements;++j) {
      arg = array_element_at(&directive->arguments,j);

      /* {{{ set value */
      str_init(&val);
      if(source) {
        /*
         * Source is set, so we check: if it is CGI, we take our value from the CGI
         * environment. If not, we check if value is not null and set one (the right)
         * of the values
         */
        if(cf_strcmp(source,"cgi") == 0) {
          if(head && (mult = cf_cgi_get_multiple(head,arg->param)) != NULL && mult->value && *mult->value) {
            str_char_set(&val,mult->value,strlen(mult->value));

            for(mult=mult->next;mult;mult=mult->next) {
              str_char_append(&val,',');
              str_cstr_append(&val,mult->value);
            }
          }
          else if(arg->ifnotcommitted) str_char_set(&val,arg->ifnotcommitted,strlen(arg->ifnotcommitted));
        }
        /*
         * Ok, source is config, check if config has the specific
         * value (j < valnum) and set it to the template
         */
        else {
          if(value && j < value->valnum) str_char_set(&val,value->values[j],strlen(value->values[j]));
          else if(arg->deflt) str_char_set(&val,arg->deflt,strlen(arg->deflt));
        }
      }
      else {
        /*
         * source is not set; so try to get the value first from CGI
         * environment (CGI overwrites config), after that look
         * if the specific config value exists.
         */
        if(head && (mult = cf_cgi_get_multiple(head,arg->param)) != NULL) {
          str_char_set(&val,mult->value,strlen(mult->value));

          for(mult=mult->next;mult;mult=mult->next) {
            str_char_append(&val,',');
            str_cstr_append(&val,mult->value);
          }
        }
        else {
          /*
           * A value for this config directive has not been
           * committed by CGI, so set it by config if exists
           */
          if(value && j < value->valnum) str_char_set(&val,value->values[j],strlen(value->values[j]));
          else if(arg->deflt) str_char_set(&val,arg->deflt,strlen(arg->deflt));
        }
      }
      /* }}} */

      if(val.content && *val.content) {
        if(!arg->parse || cf_strcmp(arg->parse,"date") != 0) cf_uconf_to_html(&val);
        else {
          /* date value */
          tval = (time_t)strtoul(val.content,NULL,10);
          localtime_r(&tval,&tm);
          j = snprintf(buff,256,"%02d. %02d. %4d %02d:%02d:%02d",tm.tm_mday,tm.tm_mon+1,tm.tm_year+1900,tm.tm_hour,tm.tm_min,tm.tm_sec);
          str_char_set(&val,buff,j);
        }

        cf_set_variable(&tpl,cs,arg->param,val.content,val.len,1);
        str_cleanup(&val);
      }
    }
  }

  /* {{{ set error message */
  if(msg) {
    if((tmp = cf_get_error_message(msg,&i)) != NULL) {
      cf_set_variable(&tpl,cs,"err",tmp,i,1);
      free(tmp);
    }
    else cf_set_variable(&tpl,cs,"err",msg,strlen(msg),1);

    cf_tpl_setvalue(&tpl,"error",TPL_VARIABLE_INT,1);
  }
  /* }}} */

  /* {{{ set categories */
  cf_tpl_var_init(&array,TPL_VARIABLE_ARRAY);
  for(i=0;i<cats->valnum;++i) {
    if(utf8) {
      tmp = htmlentities(cats->values[i],0);
      j   = strlen(tmp);
    }
    else tmp = charset_convert_entities(cats->values[i],strlen(cats->values[i]),"UTF-8",cs->values[0],&j);

    cf_tpl_var_addvalue(&array,TPL_VARIABLE_STRING,tmp,j);
    free(tmp);
  }
  cf_tpl_setvar(&tpl,"categories",&array);
  /* }}} */

  if(saved) cf_tpl_setvalue(&tpl,"save",TPL_VARIABLE_INT,1);
  if(cf_hash_get(GlobalValues,"is_admin",8) != NULL) cf_tpl_setvalue(&tpl,"save",TPL_VARIABLE_INT,1);

  cval = cfg_get_first_value(&fo_default_conf,fn,"UBaseURL");
  cf_set_variable(&tpl,cs,"forumbase",cval->values[0],strlen(cval->values[0]),1);

  cval = cfg_get_first_value(&fo_default_conf,fn,"UserConfig");
  cf_set_variable(&tpl,cs,"userconfig",cval->values[0],strlen(cval->values[0]),1);
  cf_set_variable(&tpl,cs,"script",cval->values[0],strlen(cval->values[0]),1);

  cval = cfg_get_first_value(&fo_default_conf,fn,"UserManagement");
  cf_set_variable(&tpl,cs,"usermanagement",cval->values[0],strlen(cval->values[0]),1);

  cf_set_variable(&tpl,cs,"charset",cs->values[0],strlen(cs->values[0]),1);

  if(errors && errors->elements) {
    cf_tpl_var_init(&errmsgs,TPL_VARIABLE_ARRAY);

    for(i=0;i<errors->elements;++i) {
      emsg = array_element_at(errors,i);
      cf_tpl_var_addvalue(&errmsgs,TPL_VARIABLE_STRING,emsg->content,emsg->len);
    }

    cf_tpl_setvar(&tpl,"errmsgs",&errmsgs);
    cf_tpl_setvalue(&tpl,"error",TPL_VARIABLE_INT,1);
  }

  cf_run_uconf_display_handlers(head,&fo_default_conf,&fo_userconf_conf,&tpl,&glob_config);

  printf("Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
  cf_tpl_parse(&tpl);

  cf_tpl_finish(&tpl);
  cf_uconf_cleanup_modxml(modxml);
  cfg_cleanup_file(&config);
}
/* }}} */

/* {{{ do_save */
void do_save(cf_hash_t *head) {
  uconf_userconfig_t *merged;
  u_char *msg,*uname,*ucfg, *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);;
  configfile_t config;
  name_value_t *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  array_t errmsgs;

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  if((ucfg = cf_get_uconf_name(uname)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return;
  }

  cfg_init_file(&config,ucfg);

  if(read_config(&config,handle_userconf_command,CFG_MODE_USER) != 0) {
    printf("Status: 500 Internal Server Error\015\012COntent-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_CONFIG_BROKEN",NULL);
    free(ucfg);
    return;
  }

  inited = 1;

  memset(&errmsgs,0,sizeof(errmsgs));
  if((merged = cf_uconf_merge_config(head,&glob_config,&errmsgs,1)) != NULL) {
    if(cf_run_uconf_write_handlers(head,&fo_default_conf,&fo_userconf_conf,&glob_config,merged) == FLT_EXIT) {
      cf_uconf_cleanup_modxml(merged);
      free(merged);
      cfg_cleanup_file(&config);
      free(ucfg);
      return;
    }

    if((msg = cf_write_uconf(ucfg,merged)) != NULL) cf_error_message(msg,NULL,strerror(errno));
    else show_edit_content(head,NULL,"cgi",1,NULL);

    cf_uconf_cleanup_modxml(merged);
    free(merged);
    cfg_cleanup_file(&config);
    free(ucfg);
    return;
  }
  else {
    show_edit_content(head,NULL,"cgi",0,&errmsgs);
    array_destroy(&errmsgs);
  }

  cfg_cleanup_file(&config);
  free(ucfg);
}
/* }}} */

/* {{{ normalize_params */
u_char *normalize_params(cf_hash_t *head,const u_char *name) {
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10),*converted,*val,c;
  register u_char *ptr;
  name_value_t *cs = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  cf_hash_keylist_t *key;
  cf_cgi_param_t *param;
  size_t flen;

  string_t str;


  if((val = cf_cgi_get(head,(u_char *)name)) == NULL) return "E_manipulated";

  /* utf-8? */
  if(*val != 0xC3 || *(val+1) != 0xBF) {
    /* {{{ transform everything to utf-8... */
    for(key=head->keys.elems;key;key=key->next) {
      for(param = cf_cgi_get_multiple(head,key->key);param;param=param->next) {
        if((converted = charset_convert(param->value,strlen(param->value),cs->values[0],"UTF-8",NULL)) == NULL) return "E_manipulated";

        /* {{{ remove unicode whitespaces */
        str_init(&str);
        for(ptr=converted;*ptr;++ptr) {
          // \xC2\xA0 is nbsp
          if(cf_strncmp(ptr,"\xC2\xA0",2) == 0) {
            str_char_append(&str,' ');
            ++ptr;
          }
          // \xE2\x80[\x80-\x8B\xA8-\xAF] are unicode whitespaces
          else if(cf_strncmp(ptr,"\xE2\x80",2) == 0) {
            c = *(ptr+3);
            if((c >= 0x80 && c <= 0x8B) || (c >= 0xA8 && c <= 0xAF)) {
              str_char_append(&str,' ');
              ptr += 2;
            }
            else str_char_append(&str,*ptr);
          }
          else str_char_append(&str,*ptr);
        }
        /* }}} */

        free(param->value);
        param->value = str.content;

        free(converted);
      }
    }
    /* }}} */
  }
  else {
    /* {{{ input seems to be UTF-8, check if strings are valid UTF-8 */
    for(key=head->keys.elems;key;key=key->next) {
      for(param = cf_cgi_get_multiple(head,key->key);param;param=param->next) {
        if(is_valid_utf8_string(param->value,strlen(param->value)) != 0) return "E_manipulated";

        /* {{{ removed unicode whitespaces */
        str_init(&str);
        for(ptr=param->value;*ptr;++ptr) {
          // \xC2\xA0 is nbsp
          if(cf_strncmp(ptr,"\xC2\xA0",2) == 0) {
            str_char_append(&str,' ');
            ++ptr;
          }
          // \xE2\x80[\x80-\x8B\xA8-\xAF] are unicode whitespaces
          else if(cf_strncmp(ptr,"\xE2\x80",2) == 0) {
            c = *(ptr+3);
            if((c >= 0x80 && c <= 0x8B) || (c >= 0xA8 && c <= 0xAF)) {
              str_char_append(&str,' ');
              ptr += 2;
            }
            else str_char_append(&str,*ptr);
          }
          else str_char_append(&str,*ptr);
        }
        /* }}} */

        free(param->value);
        param->value = str.content;
      }
    }
    /* }}} */
  }

  /* {{{ remove first two characters */
  if((val = cf_cgi_get(head,(u_char *)name)) != NULL) {
    flen  = strlen(val);

    converted = fo_alloc(NULL,1,flen-2,FO_ALLOC_MALLOC);

    /* strip character from field */
    memcpy(converted,val+2,flen-2);
    memcpy(val,converted,flen-2);

    val[flen-2] = '\0';
    free(converted);
  }
  /* }}} */

  return NULL;
}
/* }}} */

/* {{{ add_view_directive */
int add_view_directive(configfile_t *cfile,const u_char *context,u_char *name,u_char **args,size_t argnum) {
  conf_opt_t opt;
  int ret;

  opt.name   = strdup(name);
  opt.data   = &fo_view_conf;
  if(context) opt.flags = CFG_OPT_LOCAL;
  else opt.flags = CFG_OPT_GLOBAL;

  opt.flags |= CFG_OPT_USER|CFG_OPT_CONFIG;

  ret = handle_command(NULL,&opt,context,args,argnum);

  if(ret != -1) free(opt.name);

  return ret;
}
/* }}} */

/**
 * Dummy function, for ignoring unknown directives
 */
int ignre(configfile_t *cf,const u_char *context,u_char *name,u_char **args,size_t argnum) {
  return 0;
}

/**
 * The main function of the forum userconfig program. No command line switches
 * used.
 * \param argc The argument count
 * \param argv The argument vector
 * \param env The environment vector
 * \return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int main(int argc,char *argv[],char *env[]) {
  /* {{{ initialization */
  static const u_char *wanted[] = {
    "fo_default", "fo_userconf", "fo_view"
  };

  cf_hash_t *head;

  u_char *forum_name,*fname,*uname,*err,*action,*ucfg;

  array_t *cfgfiles;
  configfile_t dconf,conf,vconf;

  name_value_t *cs;

  int ret;
  /* }}} */

  /* {{{ set signal handler for bad signals (for error reporting) */
  signal(SIGSEGV,sighandler);
  signal(SIGILL,sighandler);
  signal(SIGFPE,sighandler);
  signal(SIGBUS,sighandler);
  /* }}} */

  cf_init();
  init_modules();
  cfg_init();

  cf_tree_init(&glob_config.global_directives,cfg_compare,destroy_directive_list);
  cf_list_init(&glob_config.forums);

  forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  head       = cf_cgi_new();

  /* {{{ read config files */
  if((cfgfiles = get_conf_file(wanted,3)) == NULL) {
    fprintf(stderr,"Could not find config files!\n");
    return EXIT_FAILURE;
  }

  fname = *((u_char **)array_element_at(cfgfiles,0));
  cfg_init_file(&dconf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,1));
  cfg_init_file(&conf,fname);
  free(fname);

  fname = *((u_char **)array_element_at(cfgfiles,2));
  cfg_init_file(&vconf,fname);
  free(fname);

  cfg_register_options(&dconf,default_options);
  cfg_register_options(&conf,fo_userconf_options);
  cfg_register_options(&vconf,fo_view_options);

  if(read_config(&dconf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&conf,NULL,CFG_MODE_CONFIG) != 0 || read_config(&vconf,add_view_directive,CFG_MODE_CONFIG|CFG_MODE_NOLOAD) != 0) {
    fprintf(stderr,"config file error!\n");
    cfg_cleanup_file(&dconf);
    cfg_cleanup_file(&conf);
    return EXIT_FAILURE;
  }
  /* }}} */

  /* first action: authorization modules */
  cs  = cfg_get_first_value(&fo_default_conf,forum_name,"ExternCharset");
  ret = cf_run_auth_handlers(head);

  if((uname = cf_hash_get(GlobalValues,"UserName",8)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return EXIT_SUCCESS;
  }

  /* {{{ read user config */
  if((ucfg = cf_get_uconf_name(uname)) == NULL) {
    printf("Status: 403 Forbidden\015\012Content-Type: text/html; charset=%s\015\012\015\012",cs->values[0]);
    cf_error_message("E_MUST_AUTH",NULL);
    return EXIT_SUCCESS;
  }

  free(conf.filename);
  conf.filename = ucfg;

  if(read_config(&conf,ignre,CFG_MODE_USER) != 0) {
    fprintf(stderr,"config file error!\n");

    cfg_cleanup_file(&conf);
    cfg_cleanup_file(&dconf);

    return EXIT_FAILURE;
  }
  /* }}} */

  if(head && cf_cgi_get(head,"cs") != NULL) {
    if((err = normalize_params(head,"cs")) != NULL) {
      printf("Status: 500 Internal Server Error\015\012Status: text/html; charset=%s\015\012\015\012",cs->values[0]);
      cf_error_message(err,NULL);
      return EXIT_SUCCESS;
    }
  }

  if(head) {
    if((action = cf_cgi_get(head,"a")) == NULL) show_edit_content(head,NULL,NULL,0,NULL);
    else {
      if(cf_strcmp(action,"save") == 0) do_save(head);
      else {
        /* TODO: check if action is registered */
        show_edit_content(head,NULL,NULL,0,NULL);
      }
    }
  }
  else show_edit_content(head,NULL,NULL,0,NULL);

  if(head) cf_hash_destroy(head);

  /* cleanup source */
  cfg_cleanup_file(&dconf);
  cfg_cleanup_file(&conf);

  array_destroy(cfgfiles);
  free(cfgfiles);

  cleanup_modules(Modules);
  cf_fini();
  cfg_destroy();

  return EXIT_SUCCESS;
}

/* eof */
