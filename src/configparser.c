/**
 * \file configparser.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief configuration parser functions and datatypes
 *
 * This file contains the configuration parser used by this project
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <dlfcn.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <pwd.h>

#include "utils.h"
#include "hashlib.h"
#include "configparser.h"
/* }}} */

/* {{{ globals */

/***************************************** CONFIGURATION OPTIONS ************************************************
 * defining the global variables for configuration
 *
 ***/

t_configuration fo_default_conf;
t_configuration fo_server_conf;
t_configuration fo_view_conf;
t_configuration fo_arcview_conf;
t_configuration fo_post_conf;
t_configuration fo_vote_conf;

t_array Modules[MOD_MAX+1];

/*
 * forum default config options
 */
t_conf_opt default_options[] = {
  { "<General>",                NULL,             NULL             },
  { "ExternCharset",            handle_command,   &fo_default_conf },
  { "TemplateMode",             handle_command,   &fo_default_conf },
  { "MessagePath",              handle_command,   &fo_default_conf },
  { "ArchivePath",              handle_command,   &fo_default_conf },
  { "ThreadIndexFile",          handle_command,   &fo_default_conf },
  { "SocketName",               handle_command,   &fo_default_conf },
  { "BaseURL",                  handle_command,   &fo_default_conf },
  { "UBaseURL",                 handle_command,   &fo_default_conf },
  { "PostingURL",               handle_command,   &fo_default_conf },
  { "UPostingURL",              handle_command,   &fo_default_conf },
  { "ArchiveURL",               handle_command,   &fo_default_conf },
  { "ArchivePostingURL",        handle_command,   &fo_default_conf },
  { "ErrorTemplate",            handle_command,   &fo_default_conf },
  { "ErrorMessages",            handle_command,   &fo_default_conf },
  { "PostScript",               handle_command,   &fo_default_conf },
  { "UPostScript",              handle_command,   &fo_default_conf },
  { "UserConfig",               handle_command,   &fo_default_conf },
  { "UserRegister",             handle_command,   &fo_default_conf },
  { "SharedMemIds",             handle_command,   &fo_default_conf },
  { "Administrators",           handle_command,   &fo_default_conf },
  { "AuthMode",                 handle_command,   &fo_default_conf },
  { "</General>",               NULL,             NULL             },

  { "<Categories>",             NULL,             NULL             },
  { "Category",                 handle_command,   &fo_default_conf },
  { "</Categories>",            NULL,             NULL             },

  { "<DateConfig>",             NULL,             NULL             },
  { "DateLocale",               handle_command,   &fo_default_conf },
  { "</DateConfig>",            NULL,             NULL             },

  { "<Users>",                  NULL,             NULL             },
  { "ConfigDirectory",          handle_command,   &fo_default_conf },
  { "InactivityDelete",         handle_command,   &fo_default_conf },
  { "SendMailBeforeDelete",     handle_command,   &fo_default_conf },
  { "</Users>",                 NULL,             NULL             },

  { NULL,                       NULL,             NULL             }
};

/*
 * forum client config options
 */
t_conf_opt fo_view_options[] = {
  { "<ForumBehavior>",            NULL,             NULL          },
  { "ShowFlags",                  handle_command,   &fo_view_conf },
  { "ReadMode",                   handle_command,   &fo_view_conf },
  { "ParamType",                  handle_command,   &fo_view_conf },
  { "</ForumBehavior>",           NULL,             NULL          },

  { "<Templates>",                NULL,             NULL          },
  { "TemplateForumBegin",         handle_command,   &fo_view_conf },
  { "TemplateForumEnd",           handle_command,   &fo_view_conf },
  { "TemplateForumThread",        handle_command,   &fo_view_conf },
  { "TemplatePosting",            handle_command,   &fo_view_conf },
  { "</Templates>",               NULL,             NULL          },

  { "<DateConfig>",               NULL,             NULL          },
  { "DateFormatThreadList",       handle_command,   &fo_view_conf },
  { "DateFormatThreadView",       handle_command,   &fo_view_conf },
  { "DateFormatLoadTime",         handle_command,   &fo_view_conf },
  { "</DateConfig>",              NULL,             NULL          },

  { "<Form>",                     NULL,             NULL          },
  { "Name",                       handle_command,   &fo_view_conf },
  { "EMail",                      handle_command,   &fo_view_conf },
  { "HomepageUrl",                handle_command,   &fo_view_conf },
  { "ImageUrl",                   handle_command,   &fo_view_conf },
  { "</Form>",                    NULL,             NULL          },

  { "<Filters>",                  NULL,             NULL          },
  { "AddFilter",                  add_module,       &Modules      },
  { "</Filters>",                 NULL,             NULL          },

  { NULL,                         NULL,             NULL          }
};

/*
 * Posting configuration options
 */
t_conf_opt fo_post_options[] = {
  {  "<General>",                 NULL,             NULL          },
  {  "AddFilter",                 add_module,       &Modules      },
  {  "PostingUrl",                handle_command,   &fo_post_conf },
  {  "FieldConfig",               handle_command,   &fo_post_conf },
  {  "FieldNeeded",               handle_command,   &fo_post_conf },
  {  "RedirectOnPost",            handle_command,   &Modules      },
  {  "</General>",                NULL,             NULL          },
  {  "<Images>",                  NULL,             NULL          },
  {  "Image",                     handle_command,   &fo_post_conf },
  {  "</Images>",                 NULL,             NULL          },
  {  "<Templates>",               NULL,             NULL          },
  {  "ThreadTemplate",            handle_command,   &fo_post_conf },
  {  "FatalTemplate",             handle_command,   &fo_post_conf },
  {  "OkTemplate",                handle_command,   &fo_post_conf },
  {  "</Templates>",              handle_command,   &fo_post_conf },
  {  NULL,                        NULL,             NULL          }
};

/*
 * fo_server config options
 */
t_conf_opt fo_server_options[] = {
  { "<General>",            NULL,             NULL },
  { "SortThreads",          handle_command,   &fo_server_conf },
  { "SortMessages",         handle_command,   &fo_server_conf },
  { "RunArchiver",          handle_command,   &fo_server_conf },
  { "ErrorLog",             handle_command,   &fo_server_conf },
  { "StdLog",               handle_command,   &fo_server_conf },
  { "PIDFile",              handle_command,   &fo_server_conf },
  { "</General>",           NULL,             NULL },

  { "<Archiving>",          NULL,             NULL },
  { "ArchiveOnVote",        handle_command,   &fo_server_conf },
  { "MainFileMaxBytes",     handle_command,   &fo_server_conf },
  { "MainFileMaxPostings",  handle_command,   &fo_server_conf },
  { "MainFileMaxThreads",   handle_command,   &fo_server_conf },
  { "</Archiving>",         NULL,             NULL },

  { "<Filters>",            handle_command,   &fo_server_conf },
  { "AddFilter",            add_module,       &Modules        },
  { "</Filters>",           NULL,             NULL },

  { NULL,                   NULL,             NULL            }
};

t_conf_opt fo_arcview_options[] = {
  { "<Filters>",            NULL,             NULL },
  { "AddFilter",            add_module,       &Modules        },
  { "</Filters>",           NULL,             NULL },

  { "<General>",            NULL,             NULL },
  { "SortYearList",         handle_command,   &fo_arcview_conf },
  { "SortMonthList",        handle_command,   &fo_arcview_conf },
  { "DateFormatList",       handle_command,   &fo_arcview_conf },
  { "DateFormatViewList",   handle_command,   &fo_arcview_conf },
  { "EnableCache",          handle_command,   &fo_arcview_conf },
  { "CacheLevel",           handle_command,   &fo_arcview_conf },
  { "CacheDir",             handle_command,   &fo_arcview_conf },
  { "</General>",           NULL,             NULL },

  { "<Templates>",             NULL,             NULL },
  { "FatalTemplate",           handle_command,   &fo_arcview_conf },
  { "YearsTemplate",           handle_command,   &fo_arcview_conf },
  { "YearListTemplate",        handle_command,   &fo_arcview_conf },
  { "MonthsTemplate",          handle_command,   &fo_arcview_conf },
  { "MonthsListTemplate",      handle_command,   &fo_arcview_conf },
  { "ThreadListMonthTemplate", handle_command,   &fo_arcview_conf },
  { "ThreadTemplate",          handle_command,   &fo_arcview_conf },
  { "ThreadListTemplate",      handle_command,   &fo_arcview_conf },
  { "PerThreadTemplate",       handle_command,   &fo_arcview_conf },
  { "UpDownTemplate",          handle_command,   &fo_arcview_conf },
  { "</Templates>",            NULL,             NULL },

  { NULL,                      NULL,             NULL            }
};

t_conf_opt fo_vote_options[] = {
  { "<Filters>",                NULL,             NULL },
  { "AddFilter",                add_module,       &Modules        },
  { "</Filters>",               NULL,             NULL },

  { "<General>",                NULL,             NULL             },
  { "VotingDatabase",           handle_command,   &fo_vote_conf    },
  { "</General>",               NULL,             NULL             },
  { NULL,                       NULL,             NULL             }
};

/* }}} */

/* {{{ get_conf_file
 * Returns: u_char ** (NULL on failure, a string-array on success)
 * Parameters:
 *   - u_char **argv  a list of parameter strings
 *
 * this function expects the argv parameter list of main(). It uses
 * the first parameter, the complete path to the program and returns
 * the pathes to the configuration files
 *
 */
t_array *get_conf_file(const u_char **which,size_t llen) {
  t_array *ary = fo_alloc(NULL,1,sizeof(*ary),FO_ALLOC_CALLOC);
  u_char *env;
  t_string file;
  size_t len;
  struct stat st;
  size_t i;

  if((env = getenv("CF_CONF_DIR")) == NULL) {
    fprintf(stderr,"CF_CONF_DIR has not been set!\n");
    return NULL;
  }

  len = strlen(env);

  array_init(ary,sizeof(u_char **),NULL);

  for(i=0;i<llen;i++) {
    str_init(&file);

    str_char_set(&file,env,len);

    if(file.content[file.len-1] != '/') {
      str_char_append(&file,'/');
    }

    str_chars_append(&file,which[i],strlen(which[i]));
    str_chars_append(&file,".conf",5);

    memset(&st,0,sizeof(st));
    if(stat(file.content,&st) == -1) {
      str_cleanup(&file);
      perror("stat");
      return NULL;
    }

    array_push(ary,&file.content);
  }

  return ary;
}
/* }}} */

/* {{{ parse_args */
/**
 * This function parses the arguments of a configuration directive
 * Parameters:
 * \param line The line in the configuration file
 * \param args A reference to a u_char ** pointer
 * \param lnum The line number
 * \return 0 on success, error code on failure
 */
int parse_args(u_char *fname,u_char *line,u_char ***args,int lnum) {
  register u_char *ptr = line;
  int len = -1,run;
  t_string argument;

  *args = NULL;

  while(len < 2) {
    /*
     * skip trailing whitespaces
     */
    for(;*ptr && isspace(*ptr) && *ptr != (u_char)012;ptr++);
    if(*ptr == (u_char)012 || *ptr == (u_char)0 || *ptr == '#') break; /* end of line or end of file or comment */

    if(*ptr != '"') {
      fprintf(stderr,"%s: unexpected character %d at line %d!\n",fname,*ptr,lnum);
      return -1;
    }

    str_init(&argument);
    run = 1;

    for(ptr++;*ptr && run;ptr++) {
      switch(*ptr) {
        case '\\':
          switch(*++ptr) {
            case 'n':
              str_char_append(&argument,'\n');
              break;
            case 't':
              str_char_append(&argument,'\t');
              break;
            case 'r':
              str_char_append(&argument,'\r');
              break;
            default:
              str_char_append(&argument,*ptr);
              break;
          }
          break;
        case '"':
          /* empty arguments have to be filled up with a \0 byte */
          if(len >= 0 && !(*args)[len]) (*args)[len] = strdup("\0");

          *args = fo_alloc(*args,++len + 1,sizeof(u_char *),FO_ALLOC_REALLOC);
          (*args)[len] = argument.content;
          run = 0;
          break;
        default:
          str_char_append(&argument,*ptr);
          break;
      }
    }

  }

  /* empty arguments have to be filled up with a \0 byte */
  if(len >= 0 && !(*args)[len]) (*args)[len] = strdup("\0");

  return len + 1;
}
/* }}} */

/* {{{ read_config
 * Returns: int              0 on success, 1 on error
 * Parameters:
 *   - t_configuration conf  the configuration structure
 *
 * This function parses an configuration structure
 *
 */
int read_config(t_configfile *conf,t_take_default deflt) {
  int fd  = open(conf->filename,O_RDONLY);
  u_char *buff;
  u_char *ptr,*ptr1;
  struct stat st;
  u_char *directive_name;
  u_char **args;
  int i,found;
  t_conf_opt *opt;
  unsigned int linenum = 0;
  int argnum;

  /*
   * open() could fail :)
   */
  if(fd == -1) {
    perror("open");
    return 1;
  }

  if(stat(conf->filename,&st) == -1) {
    close(fd);
    perror("stat");
    return 1;
  }

  if(((void *)(buff = mmap(0,st.st_size+1,PROT_READ,MAP_FILE|MAP_SHARED,fd,0))) == (caddr_t)-1) {
    close(fd);
    perror("mmap");
    return 1;
  }

  ptr = buff;

  /*
   * loop through the hole file
   */
  while(*ptr) {
    /* we're at the next line */
    ++linenum;

    /* first, lets find the beginning of the line (whitespaces have to be ignored) */
    for(;*ptr && isspace(*ptr);ptr++);

    /* eof */
    if(!*ptr) break;

    /* comment - continue with next line */
    if(*ptr == '#') {
      /* read until the end of the line */
      for(;*ptr && *ptr != (u_char)012;ptr++);
      if(!*ptr) break;
      ptr += 1;
      continue;
    }

    /* got end of line - continue with next line */
    if(*ptr == (u_char)012) {
      ptr += 1;
      continue;
    }

    /* ok, we're at the beginning of the directive. */
    ptr1 = ptr;

    /* now we got the end of the directive */
    for(;*ptr && !isspace(*ptr);ptr++);

    /* eof */
    if(!*ptr) break;

    directive_name = memdup(ptr1,ptr-ptr1+1);
    directive_name[ptr-ptr1] = '\0';

    if((argnum = parse_args(conf->filename,ptr,&args,linenum)) == -1) {
      close(fd);
      munmap(buff,st.st_size);
      return 1;
    }

    found = 0;
    if((opt = cf_hash_get(conf->options,directive_name,ptr-ptr1)) != NULL) {
      if(opt->callback) found = opt->callback(conf,opt,args,argnum);
    }
    else {
      if(deflt) {
        found = deflt(conf,directive_name,args,argnum);
      }
      else {
        fprintf(stderr,"%s: Configuration entry for directive %s not found!\n",conf->filename,directive_name);
        return 1;
      }
    }

    if(found == 0) {
      /* arguments are no longer needed */
      if(argnum > 0) {
        for(i=0;i<argnum;i++) free(args[i]);
        free(args);
      }
    }
    else if(found != -1) {
      fprintf(stderr,"%s: %s: Callback function returned not 0 or -1!\n",conf->filename,directive_name);
      return 1;
    }

    free(directive_name);

    /* now everything whith this directive has finished. Find end of line... */
    for(;*ptr && *ptr != (u_char)012;ptr++);

    /* eof */
    if(!*ptr) break;

    ptr += 1;
  }

  close(fd);
  munmap(buff,st.st_size);

  return 0;
}
/* }}} */

/* {{{ handle_command
 * Returns: int    FO_CONF_OK or FO_CONF_PARENTHESIS
 * Parameters:
 *   - t_conf_entry *entry   a pointer to the configuration entry
 *   - void *arg             user argument (the configuration variable)
 *
 * this function puts an entry to a configuration structure
 *
 */
int handle_command(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  t_configuration *conf = (t_configuration *)opt->data;
  t_name_value tmp;
  t_cf_tree_dataset dt;
  const t_cf_tree_dataset *dt1;
  t_cf_list_head *head;

  tmp.values = args;
  tmp.valnum = argnum;
  tmp.name   = strdup(opt->name);

  dt.key = opt->name;

  if((dt1 = cf_tree_find(&conf->directives,conf->directives.root,&dt)) != NULL) {
    head = dt1->data;
    cf_list_append(head,&tmp,sizeof(tmp));
  }
  else {
    head = fo_alloc(NULL,1,sizeof(*head),FO_ALLOC_CALLOC);
    cf_list_append(head,&tmp,sizeof(tmp));

    dt.key = strdup(opt->name);
    dt.data = head;

    cf_tree_insert(&conf->directives,NULL,&dt);
  }

  return -1;
}
/* }}} */

/* {{{ add_module */

int add_module(t_configfile *cfile,t_conf_opt *opt,u_char **args,int argnum) {
  t_array *modules = (t_array *)opt->data;
  void *mod = dlopen(args[0],RTLD_LAZY);
  char *error;
  t_module module = { NULL, NULL, NULL };

  if(mod) {
    void *mod_cfg_p = dlsym(mod,args[1]);

    if((error = (char *)dlerror()) == NULL) {
      t_module_config *mod_cfg = (t_module_config *)mod_cfg_p;
      int i;

      if(!mod_cfg_p) {
        fprintf(stderr,"ERROR: cannot load plugin configuration!\n");
        return -1;
      }

      if(mod_cfg->cfgopts) {
        cfg_register_options(cfile,mod_cfg->cfgopts);
      }

      /* register the module in the module list */
      if(!modules[0].element_size) {
        array_init(&modules[0],sizeof(t_module),cfg_destroy_module);
      }

      module.module = mod;
      module.cfg    = mod_cfg;

      array_push(&modules[0],&module);

      /* register all handlers */
      for(i=0;mod_cfg->handlers[i].handler;i++) {
        if(!modules[mod_cfg->handlers[i].handler].element_size) {
          array_init(&modules[mod_cfg->handlers[i].handler],sizeof(t_handler_config),NULL);
        }

        array_push(&modules[mod_cfg->handlers[i].handler],&mod_cfg->handlers[i]);
      }
    }
    else {
      dlclose(mod);
      fprintf(stderr,"could not get module conmfig: %s\n",error);
      return -1;
    }
  }
  else {
    fprintf(stderr,"%s\n",dlerror());
  }

  return 0;
}

/* }}} */

/* {{{ cfg_cleanup
 * Returns: nothing
 * Parameters:
 *   - t_configuration *cfg  the configuration structure
 *
 * this function cleans a configuration structure and frees all mem
 *
 */
void cfg_cleanup(t_configuration *cfg) {
/** \todo write cleanup code */
#if 0
  int i,j;
  t_name_value *ent = cfg->directives,*entlast = cfg->directives;

  for(j=0;j<cfg->len;j++,ent=entlast) {
    entlast = ent->next;

    free(ent->name);

    if(ent->valnum > 0) {
      for(i=0;i<ent->valnum;i++) free(ent->values[i]);
      free(ent->values);
    }

    free(ent);
  }
#endif
}
/* }}} */

/* {{{ cfg_destroy_module */
void cfg_destroy_module(void *element) {
  t_module *mod = (t_module *)element;

  if(mod->module) {
    if(mod->cfg->finish) mod->cfg->finish();
    dlclose(mod->module);
  }
}
/* }}} */

/* {{{ cleanup_modules
 * Returns: nothing
 * Parameters:
 *   - t_module *mod   the module structure
 *
 * this function cleans up the modules
 *
 */
void cleanup_modules(t_array *modules) {
  int i;

  for(i=0;i<MOD_MAX;i++) {
    array_destroy(&modules[i]);
  }
}
/* }}} */

/* {{{ cfg_get_first_value */
t_name_value *cfg_get_first_value(t_configuration *cfg,const u_char *name) {
  t_name_value *ent;
  const t_cf_tree_dataset *dt;
  t_cf_tree_dataset dt1;
  t_cf_list_head *head;

  dt1.key = (void *)name;

  if((dt = cf_tree_find(&cfg->directives,cfg->directives.root,&dt1)) != NULL) {
    head = dt->data;
    return head->elements->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cfg_get_value */
t_cf_list_head *cfg_get_value(t_configuration *cfg,const u_char *name) {
  t_name_value *ent;
  const t_cf_tree_dataset *dt;
  t_cf_tree_dataset dt1;

  dt1.key = (void *)name;

  if((dt = cf_tree_find(&cfg->directives,cfg->directives.root,&dt1)) != NULL) return dt->data;

  return NULL;
}
/* }}} */

/* {{{ cfg_init_file */
void cfg_init_file(t_configfile *conf,u_char *filename) {
  conf->filename = strdup(filename);
  conf->options  = cf_hash_new(NULL);
}
/* }}} */

/* {{{ cfg_register_options */
int cfg_register_options(t_configfile *conf,t_conf_opt *opts) {
  int i;

  for(i=0;opts[i].name;i++) {
    if(cf_hash_get(conf->options,(u_char *)opts[i].name,strlen(opts[i].name)) != NULL) {
      errno = EINVAL;
      return -1;
    }

    cf_hash_set(conf->options,(u_char *)opts[i].name,strlen(opts[i].name),&opts[i],sizeof(opts[i]));
  }

  return 0;
}
/* }}} */

/* {{{ cfg_cleanup_file */
void cfg_cleanup_file(t_configfile *conf) {
  cf_hash_destroy(conf->options);
  free(conf->filename);
}
/* }}} */

int cfg_compare(t_cf_tree_dataset *dt1,t_cf_tree_dataset *dt2) {
  return strcmp(dt1->key,dt2->key);
}

void cfg_init(void) {
  /** \todo define cleanup function */
  cf_tree_init(&fo_default_conf.directives,cfg_compare,NULL);
  cf_tree_init(&fo_server_conf.directives,cfg_compare,NULL);
  cf_tree_init(&fo_view_conf.directives,cfg_compare,NULL);
  cf_tree_init(&fo_arcview_conf.directives,cfg_compare,NULL);
  cf_tree_init(&fo_post_conf.directives,cfg_compare,NULL);
  cf_tree_init(&fo_vote_conf.directives,cfg_compare,NULL);
}

/* eof */
