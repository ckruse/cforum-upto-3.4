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

/* {{{ forum default config options */
t_conf_opt default_options[] = {
  { "ExternCharset",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "TemplateMode",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "XHTMLMode",                handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_default_conf },
  { "ThreadIndexFile",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "SocketName",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,&fo_default_conf },
  { "BaseURL",                  handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UBaseURL",                 handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "PostingURL",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostingURL",              handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "VoteURL",                  handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "ArchiveURL",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "ArchivePostingURL",        handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "ErrorTemplate",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "ErrorMessages",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "PostScript",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostScript",              handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UserConfig",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UserManagement",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "UserRegister",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "SharedMemIds",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "Administrators",           handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL,                &fo_default_conf },
  { "AuthMode",                 handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },

  { "Categories",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },

  { "DateLocale",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "Language",                 handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "MessagesDatabase",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },

  { "ConfigDirectory",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "InactivityDelete",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },
  { "SendMailBeforeDelete",     handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_default_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_view config options */
t_conf_opt fo_view_options[] = {
  { "<ForumBehavior>", NULL, 0, NULL },
  { "DoQuote",                    handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "QuotingChars",               handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_NEEDED|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "ShowThread",                 handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "ShowFlags",                  handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL,                             &fo_view_conf },
  { "ReadMode",                   handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_NEEDED|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "ParamType",                  handle_command,   CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL,              &fo_view_conf },
  { "ShowSig",                    handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "MaxSigLines",                handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_view_conf },
  { "</ForumBehavior>", NULL, 0, NULL },

  { "<Templates>", NULL, 0, NULL },
  { "TemplateForumBegin",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "TemplateForumEnd",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "TemplateForumThread",        handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "TemplatePosting",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "</Templates>", NULL, 0, NULL },

  { "<DateConfig>", NULL, 0, NULL },
  { "DateFormatThreadList",       handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "DateFormatThreadView",       handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "DateFormatLoadTime",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_view_conf },
  { "</DateConfig>", NULL, 0, NULL},

  { "<Form>", NULL, 0, NULL },
  { "Name",                       handle_command,   CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_GLOBAL, &fo_view_conf },
  { "EMail",                      handle_command,   CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_GLOBAL, &fo_view_conf },
  { "HomepageUrl",                handle_command,   CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_GLOBAL, &fo_view_conf },
  { "ImageUrl",                   handle_command,   CFG_OPT_USER|CFG_OPT_UNIQUE|CFG_OPT_GLOBAL, &fo_view_conf },
  { "</Form>", NULL, 0, NULL },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_post configuration options */
t_conf_opt fo_post_options[] = {
  { "PostingUrl",                handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "FieldConfig",               handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "FieldNeeded",               handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "RedirectOnPost",            handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "FieldValidate",             handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "QuotingChars",              handle_command,   CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_NEEDED|CFG_OPT_UNIQUE|CFG_OPT_LOCAL, &fo_post_conf },

  { "Image",                     handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },

  { "ThreadTemplate",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "FatalTemplate",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },
  { "OkTemplate",                handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_post_conf },

  {  NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_server config options */
t_conf_opt fo_server_options[] = {
  { "RunArchiver",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "ErrorLog",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "StdLog",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "LogMaxSize",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "PIDFile",              handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "MaxThreads",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "MinThreads",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "SpareThreads",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },
  { "Forums",               handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL, &fo_server_conf },

  { "SortThreads",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_server_conf },
  { "SortMessages",         handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_server_conf },
  { "ArchiveOnVote",        handle_command,   CFG_OPT_CONFIG|CFG_OPT_LOCAL,                &fo_server_conf },
  { "MainFileMaxBytes",     handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_server_conf },
  { "MainFileMaxPostings",  handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_server_conf },
  { "MainFileMaxThreads",   handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL, &fo_server_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_arcview viewer options */
t_conf_opt fo_arcview_options[] = {
  { "<General>", NULL, 0, NULL },
  { "SortYearList",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "SortMonthList",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "DateFormatList",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "DateFormatViewList",      handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "EnableCache",             handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "CacheLevel",              handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "CacheDir",                handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "QuotingChars",            handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "</General>", NULL, 0, NULL },

  { "<Templates>", NULL, 0, NULL },
  { "FatalTemplate",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "YearsTemplate",           handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "YearListTemplate",        handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "MonthsTemplate",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "MonthsListTemplate",      handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "ThreadListMonthTemplate", handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "ThreadTemplate",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "ThreadListTemplate",      handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "PerThreadTemplate",       handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "UpDownTemplate",          handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_GLOBAL,   &fo_arcview_conf },
  { "</Templates>", NULL, 0, NULL },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_vote options */
t_conf_opt fo_vote_options[] = {
  { "VotingDatabase",  handle_command,   CFG_OPT_NEEDED|CFG_OPT_CONFIG|CFG_OPT_LOCAL,               &fo_vote_conf },
  { "Send204",         handle_command,   CFG_OPT_UNIQUE|CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,  &fo_vote_conf },
  { "OkTemplate",      handle_command,   CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL,               &fo_vote_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

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

  /* read until end-of-line */
  while(1) {
    /*
     * skip trailing whitespaces
     */
    for(;*ptr && isspace(*ptr) && *ptr != (u_char)012;++ptr);
    if(*ptr == (u_char)012 || *ptr == (u_char)0 || *ptr == '#') break; /* end of line or end of file or comment */

    if(*ptr != '"') {
      fprintf(stderr,"[%s:%d] unexpected character %x!\n",fname,lnum,*ptr);
      return -1;
    }

    str_init(&argument);
    run = 1;

    for(++ptr;*ptr && run;++ptr) {
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

/* {{{ destroy_str_ptr */
void destroy_str_ptr(void *tmp) {
  free(*((u_char **)tmp));
}
/* }}} */

/* {{{ read_config */
int read_config(t_configfile *conf,t_take_default deflt,int mode) {
  int fd = open(conf->filename,O_RDONLY);
  u_char *buff,buff1[512];
  u_char *ptr,*ptr1;
  struct stat st;
  u_char *directive_name;
  u_char **args;
  int i,found,fatal = 0;
  t_conf_opt *opt;
  unsigned int linenum = 0;
  int argnum;
  t_cf_list_element *lelem;
  u_char *context = NULL;
  t_string mpath;
  size_t len,j;
  t_cf_hash *hsh;
  t_array ary;
  int noload = mode & CFG_MODE_NOLOAD;

  mode &= ~CFG_MODE_NOLOAD;
  hsh = cf_hash_new(NULL);
  array_init(&ary,sizeof(u_char **),destroy_str_ptr);

  if(mode == CFG_MODE_USER) context = getenv("CF_FORUM_NAME");


  /* {{{ initializing */
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
  /* }}} */

  ptr = buff;

  str_init(&mpath);

  /*
   * loop through the hole file
   */
  while(*ptr) {
    /* we're at the next line */
    ++linenum;

    /* {{{ get beginning of directive */
    /* first, lets find the beginning of the line (whitespaces have to be ignored) */
    for(;*ptr && isspace(*ptr);++ptr);

    /* eof */
    if(!*ptr) break;

    /* comment - continue with next line */
    if(*ptr == '#') {
      /* read until the end of the line */
      for(;*ptr && *ptr != (u_char)012;++ptr);
      if(!*ptr) break;
      ++ptr;
      continue;
    }

    /* got end of line - continue with next line */
    if(*ptr == (u_char)012) {
      ++ptr;
      continue;
    }
    /* }}} */

    /* ok, we're at the beginning of the directive. */
    ptr1 = ptr;

    /* now we got the end of the directive */
    for(;*ptr && !isspace(*ptr);++ptr);

    /* eof */
    if(!*ptr) break;

    /* {{{ we got an eoc (end-of-context) */
    if(*ptr1 == '}') {
      if(!context) {
        fprintf(stderr,"[%s:%d] Context closed but not open!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      free(context);
      context = NULL;

      continue;
    }
    /* }}} */

    directive_name = strndup(ptr1,ptr-ptr1);

    /* {{{ hey, we got a context line -- read context */
    if(cf_strcmp(directive_name,"Forum") == 0) {
      for(;*ptr && isspace(*ptr);++ptr);
      if(!*ptr) {
        fprintf(stderr,"[%s:%d] unexpected EOF!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(!isalnum(*ptr)) {
        fprintf(stderr,"[%s:%d] Config file syntax error! Got Forum but identifier consists of characters not alnum (%c)\n",conf->filename,linenum,*ptr);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      ptr1 = ptr;

      for(;*ptr && isalnum(*ptr);++ptr);
      if(!*ptr) {
        fprintf(stderr,"[%s:%d] unexpected EOF!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }


      if(!isspace(*ptr) && *ptr != '{') {
        fprintf(stderr,"[%s:%d] Config file syntax error!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(context) {
        fprintf(stderr,"[%s:%d] Configuration file syntax error, no nesting contexts allowed!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      context = strndup(ptr1,ptr-ptr1);

      free(directive_name);
      directive_name = strdup(context);
      array_push(&ary,&directive_name);


      for(;*ptr && isspace(*ptr);++ptr);

      if(*ptr != '{') {
        fprintf(stderr,"[%s:%d] Configuration file syntax error!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      /* ok, go to next character (last was }) */
      ++ptr;

      continue;
    }
    /* }}} */

    if((argnum = parse_args(conf->filename,ptr,&args,linenum)) == -1) {
      close(fd);
      munmap(buff,st.st_size);
      return 1;
    }

    /* {{{ we got module path */
    if(cf_strcmp(directive_name,"ModulePath") == 0) {
      if(argnum != 1) {
        fprintf(stderr,"[%s:%d] Hey, wrong argument count for ModulePath!",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      str_char_set(&mpath,args[0],strlen(args[0]));
      free(args[0]);
      free(args);
      free(directive_name);

      /* now everything whith this directive has finished. Find end of line... */
      for(;*ptr && *ptr != (u_char)012;++ptr);

      continue;
    }
    /* }}} */

    /* {{{ we got a Load directive -- go and load the module */
    if(cf_strcmp(directive_name,"Load") == 0) {
      if(argnum != 1) {
        fprintf(stderr,"[%s:%d] Hey, wrong argument count for Load directive!",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(noload == 0) {
        i = mpath.len;
        str_chars_append(&mpath,args[0],strlen(args[0]));
        str_chars_append(&mpath,".so",3);

        if(add_module(conf,mpath.content,args[0]) != 0) {
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }

        mpath.len = i;
      }

      free(args[0]);
      free(args);
      free(directive_name);

      /* now everything whith this directive has finished. Find end of line... */
      for(;*ptr && *ptr != (u_char)012;++ptr);

      continue;
    }
    /* }}} */

    found = 0;
    if((opt = cf_hash_get(conf->options,directive_name,ptr-ptr1)) != NULL) {
      /* {{{ is the directive allowed? */
      if(context && opt->flags) {
        if((opt->flags & CFG_OPT_LOCAL) == 0 && mode != CFG_MODE_USER) {
          fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in local context!\n",conf->filename,linenum,directive_name);
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }
      }
      else {
        if((opt->flags & CFG_OPT_GLOBAL) == 0 && mode != CFG_MODE_USER) {
          fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in global context!\n",conf->filename,linenum,directive_name);
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }
      }
      /* }}} */

      if((opt->flags & mode)) {
        /* mark option as seen */
        if(context) {
          len = snprintf(buff1,512,"%s_%s",context,directive_name);
          cf_hash_set(hsh,buff1,len,"1",1);
        }
        else opt->flags |= CFG_OPT_SEEN;

        if(opt->callback) found = opt->callback(conf,opt,context,args,argnum);
      }
      else {
        fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in this mode!\n",conf->filename,linenum,directive_name);
        munmap(buff,st.st_size);
        close(fd);
        return 1;
      }
    }
    else {
      if(deflt) found = deflt(conf,directive_name,context,args,argnum);
      else {
        fprintf(stderr,"[%s:%d] Configuration entry for directive %s not found!\n",conf->filename,linenum,directive_name);
        return 1;
      }
    }

    if(found != -1) {
      /* arguments are no longer needed */
      if(argnum > 0) for(i=1;i<argnum;i++) free(args[i]);
      free(args[0]);
      free(args);

      if(found != 0) {
        fprintf(stderr,"[%s:%d] %s: Callback function returned not 0 or -1!\n",conf->filename,linenum,directive_name);
        return 1;
      }
    }

    free(directive_name);

    /* now everything whith this directive has finished. Find end of line... */
    for(;*ptr && *ptr != (u_char)012;++ptr);

    /* eof */
    if(!*ptr) break;

    ptr += 1;
  }

  close(fd);
  munmap(buff,st.st_size);

  if(mode != CFG_MODE_USER) {
    for(lelem=conf->options_list.elements;lelem;lelem = lelem->next) {
      opt = (t_conf_opt *)lelem->data;
      if(opt->flags & CFG_OPT_NEEDED) {
        if(opt->flags & mode) {
          /* ah, this directive has to be in every context */
          if(opt->flags & CFG_OPT_LOCAL) {
            for(j=0;j<ary.elements;++j) {
              context = *((u_char **)array_element_at(&ary,j));
              len = snprintf(buff1,512,"%s_%s",context,opt->name);
              if(cf_hash_get(hsh,buff1,len) == NULL) {
                fatal = 1;
                fprintf(stderr,"missing configuration entry %s in %s for context %s\n",opt->name,conf->filename,context);
              }
            }
          }
          /* global directive */
          else {
            if((opt->flags & CFG_OPT_SEEN) == 0) {
              fatal = 1;
              fprintf(stderr,"missing configuration entry %s in %s\n",opt->name,conf->filename);
            }
          }
        }
      }
    }
  }

  str_cleanup(&mpath);
  cf_hash_destroy(hsh);
  array_destroy(&ary);

  return fatal;
}
/* }}} */

/* {{{ cfg_compare */
int cfg_compare(t_cf_tree_dataset *dt1,t_cf_tree_dataset *dt2) {
  return strcmp(dt1->key,dt2->key);
}
/* }}} */

/* {{{ add_module */
int add_module(t_configfile *cfile,const u_char *path,const u_char *name) {
  t_array *modules = Modules;
  void *mod = dlopen(path,RTLD_LAZY);
  void *mod_cfg_p;
  char *error;
  t_module module = { NULL, NULL, NULL };
  t_module_config *mod_cfg;
  int i;


  if(mod) {
    mod_cfg_p = dlsym(mod,name);

    if((error = (u_char *)dlerror()) == NULL) {
      mod_cfg = (t_module_config *)mod_cfg_p;

      if(!mod_cfg_p) {
        fprintf(stderr,"ERROR: cannot load plugin configuration!\n");
        return -1;
      }

      if(mod_cfg->cfgopts) cfg_register_options(cfile,mod_cfg->cfgopts);

      /* register the module in the module list */
      if(!modules[0].element_size) array_init(&modules[0],sizeof(t_module),cfg_destroy_module);

      module.module = mod;
      module.cfg    = mod_cfg;

      array_push(&modules[0],&module);

      /* register all handlers */
      for(i=0;mod_cfg->handlers[i].handler;i++) {
        if(!modules[mod_cfg->handlers[i].handler].element_size) array_init(&modules[mod_cfg->handlers[i].handler],sizeof(t_handler_config),NULL);

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

/* {{{ destroy_directive */
void destroy_directive(void *arg) {
  t_name_value *val = (t_name_value *)arg;
  size_t i;

  free(val->name);
  for(i=0;i<val->valnum;i++) free(val->values[i]);
  free(val->values);
}
/* }}} */

/* {{{ destroy_directive_list */
void destroy_directive_list(t_cf_tree_dataset *dt) {
  t_cf_list_head *head = (t_cf_list_head *)dt->data;

  cf_list_destroy(head,destroy_directive);

  free(dt->key);
  free(dt->data);
}
/* }}} */

/* {{{ destroy_forums_list */
void destroy_forums_list(void *data) {
  t_internal_config *config = (t_internal_config *)data;

  free(config->name);
  cf_tree_destroy(&config->directives);
  free(config);
}
/* }}} */

/* {{{ handle_command */
int handle_command(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
  t_configuration *conf = (t_configuration *)opt->data;
  t_name_value tmp,*tmp1;
  t_cf_tree_dataset dt;
  t_cf_tree *tr;
  const t_cf_tree_dataset *dt1;
  t_cf_list_head *head;
  size_t i;
  t_cf_list_element *elem;
  t_internal_config *icfg = NULL;

  tmp.values = args;
  tmp.valnum = argnum;
  tmp.name   = strdup(opt->name);

  dt.key = opt->name;

  if(context) {
    for(elem=conf->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (t_internal_config *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) {
      icfg = fo_alloc(NULL,1,sizeof(*icfg),FO_ALLOC_MALLOC);
      icfg->name = strdup(context);
      cf_tree_init(&icfg->directives,cfg_compare,destroy_directive_list);
      cf_list_append_static(&conf->forums,icfg,sizeof(*icfg));
    }

    tr = &icfg->directives;
  }
  else tr = &conf->global_directives;


  if((dt1 = cf_tree_find(tr,tr->root,&dt)) != NULL) {
    if(opt->flags & CFG_OPT_UNIQUE) {
      head = dt1->data;
      tmp1 = head->elements->data;

      for(i=0;i<tmp1->valnum;i++) free(tmp1->values[i]);
      free(tmp1->values);

      tmp1->values = args;
    }
    else {
      head = dt1->data;
      cf_list_append(head,&tmp,sizeof(tmp));
    }
  }
  else {
    head = fo_alloc(NULL,1,sizeof(*head),FO_ALLOC_CALLOC);
    cf_list_append(head,&tmp,sizeof(tmp));

    dt.key = strdup(opt->name);
    dt.data = head;

    cf_tree_insert(tr,NULL,&dt);
  }

  return -1;
}
/* }}} */

/* {{{ cfg_cleanup */
void cfg_cleanup(t_configuration *cfg) {
  cf_tree_destroy(&cfg->global_directives);
  cf_list_destroy(&cfg->forums,destroy_forums_list);
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

  for(i=0;i<=MOD_MAX;++i) {
    array_destroy(&modules[i]);
  }
}
/* }}} */

/* {{{ cfg_get_first_value */
t_name_value *cfg_get_first_value(t_configuration *cfg,const u_char *context,const u_char *name) {
  t_name_value *ent;
  const t_cf_tree_dataset *dt;
  t_cf_tree_dataset dt1;
  t_cf_list_head *head;
  t_cf_tree *tr;
  t_cf_list_element *elem;
  t_internal_config *icfg = NULL;

  dt1.key = (void *)name;

  if(context) {
    for(elem=cfg->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (t_internal_config *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) return NULL;

    tr = &icfg->directives;
  }
  else tr = &cfg->global_directives;


  if((dt = cf_tree_find(tr,tr->root,&dt1)) != NULL) {
    head = dt->data;
    return head->elements->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cfg_get_value */
t_cf_list_head *cfg_get_value(t_configuration *cfg,const u_char *context,const u_char *name) {
  t_name_value *ent;
  const t_cf_tree_dataset *dt;
  t_cf_tree_dataset dt1;
  t_cf_list_element *elem;
  t_internal_config *icfg = NULL;
  t_cf_tree *tr;

  dt1.key = (void *)name;

  if(context) {
    for(elem=cfg->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (t_internal_config *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) return NULL;

    tr = &icfg->directives;
  }
  else tr = &cfg->global_directives;

  if((dt = cf_tree_find(tr,tr->root,&dt1)) != NULL) return dt->data;

  return NULL;
}
/* }}} */

/* {{{ cfg_init_file */
void cfg_init_file(t_configfile *conf,u_char *filename) {
  conf->filename = strdup(filename);
  conf->options  = cf_hash_new(NULL);
  cf_list_init(&conf->options_list);
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

    /*
     * be sure that seen has not been set, yet -- programmers have really
     * silly ideas, sometimes
     */
    opts[i].flags &= ~CFG_OPT_SEEN;

    cf_hash_set_static(conf->options,(u_char *)opts[i].name,strlen(opts[i].name),&opts[i]);
    cf_list_append_static(&conf->options_list,&opts[i],sizeof(opts[i]));
  }

  return 0;
}
/* }}} */

/* {{{ cfg_cleanup_file */
void cfg_cleanup_file(t_configfile *conf) {
  cf_hash_destroy(conf->options);
  cf_list_destroy(&conf->options_list,NULL);
  free(conf->filename);
}
/* }}} */

/* {{{ cfg_init */
void cfg_init(void) {
  cf_tree_init(&fo_default_conf.global_directives,cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_server_conf.global_directives,cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_view_conf.global_directives,cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_arcview_conf.global_directives,cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_post_conf.global_directives,cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_vote_conf.global_directives,cfg_compare,destroy_directive_list);

  cf_list_init(&fo_default_conf.forums);
  cf_list_init(&fo_server_conf.forums);
  cf_list_init(&fo_view_conf.forums);
  cf_list_init(&fo_arcview_conf.forums);
  cf_list_init(&fo_post_conf.forums);
  cf_list_init(&fo_vote_conf.forums);
}
/* }}} */

/* {{{ cfg_destroy */
void cfg_destroy(void) {
  cfg_cleanup(&fo_default_conf);
  cfg_cleanup(&fo_server_conf);
  cfg_cleanup(&fo_view_conf);
  cfg_cleanup(&fo_arcview_conf);
  cfg_cleanup(&fo_post_conf);
  cfg_cleanup(&fo_vote_conf);
}
/* }}} */

/* eof */
