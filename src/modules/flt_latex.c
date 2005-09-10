/**
 * \file flt_latex.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin handles lot of standard directives
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
#include <time.h>

#include <string.h>
#include <ctype.h>

#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/wait.h>

#include <openssl/md5.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "charconvert.h"
#include "validate.h"
#include "htmllib.h"
/* }}} */

#define FLT_LATEX_PNG    0
#define FLT_LATEX_OBJECT 1
#define FLT_LATEX_INLINE 2

static struct {
  u_char *cache_path;
  u_char *tmp_path;
  u_char *tex;
  u_char *dvips;
  u_char *convert;
  u_char *mzlatex;
  u_char *path_env;
  u_char *uri;
  int mode;
} flt_latex_cfg = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FLT_LATEX_PNG };

/* {{{ flt_latex_create_cache */
int flt_latex_create_cache(const u_char *cnt,size_t len,const u_char *our_sum,int elatex) {
  FILE *fd;
  string_t path,document;
  size_t mylen;
  pid_t pid;
  int status,fds[2];
  struct stat st;

  /* {{{ check if png file already exists */
  str_init_growth(&path,128);
  str_char_set(&path,flt_latex_cfg.cache_path,strlen(flt_latex_cfg.cache_path));
  str_char_append(&path,'/');
  str_chars_append(&path,our_sum,32);
  str_chars_append(&path,".png",4);

  if(stat(path.content,&st) == 0) {
    str_cleanup(&path);
    return 0;
  }
  str_cleanup(&path);
  /* }}} */

  setenv("PATH",flt_latex_cfg.path_env,1);

  /* {{{ create path */
  str_init_growth(&path,128);
  str_char_set(&path,flt_latex_cfg.tmp_path,strlen(flt_latex_cfg.tmp_path));
  str_char_append(&path,'/');
  str_chars_append(&path,our_sum,32);
  str_char_append(&path,'/');

  if(cf_make_path(path.content,0755) != 0) {
    str_cleanup(&path);
    return -1;
  }
  /* }}} */

  mylen = path.len;

  /* {{{ create latex document and write it to the file */
  str_init(&document);
  str_chars_append(&document,"\\documentclass[12pt]{article}\n\\usepackage{ucs}\n\\usepackage[utf8x]{inputenc}\n\\nonstopmode\n\\usepackage{amsmath}\n\\usepackage{amsfonts}\n\\usepackage{amssymb}\n\\pagestyle{empty}\n\n\\begin{document}\n\n",190);

  if(elatex == 0) str_chars_append(&document,"\\[",2);
  str_chars_append(&document,cnt,len);
  if(elatex == 0) str_chars_append(&document,"\n\\]",3);
  str_chars_append(&document,"\n\\end{document}\n",16);

  str_chars_append(&path,"file.tex",8);
  if((fd = fopen(path.content,"w")) == NULL) {
    path.content[mylen-1] = '\0';
    rmdir(path.content);
    str_cleanup(&path);
    str_cleanup(&document);
    return -1;
  }
  fwrite(document.content,document.len,1,fd);
  fclose(fd);

  str_cleanup(&document);
  /* }}} */

  path.len = mylen;
  path.content[path.len] = '\0';

  /* {{{ create dvi file */
  switch(pid = fork()) {
    case -1:
      fprintf(stderr,"flt_latex: fork() error: %s\n",strerror(errno));
      cf_remove_recursive(path.content);
      str_cleanup(&path);
      return -1;

    case 0:
      close(0);
      close(1);

      chdir(path.content);

      if(execlp(flt_latex_cfg.tex,flt_latex_cfg.tex,"file.tex",NULL) == -1) {
        fprintf(stderr,"flt_latex: execlp: could not execlp '%s': %s\n",flt_latex_cfg.tex,strerror(errno));
        exit(-1);
      }
      /* should never be reached, but who knows what could happen on obscure systems */
      exit(0);

    default:
      waitpid(pid,&status,0);
  }

  if(WEXITSTATUS(status) != 0) {
    fprintf(stderr,"flt_latex: execlp(%s)'s exit status is not 0 but %d!\n",flt_latex_cfg.tex,WEXITSTATUS(status));
    cf_remove_recursive(path.content);
    str_cleanup(&path);
    return -1;
  }
  /* }}} */

  /* {{{ create png file from dvi file (call dvips -R -E md5.dvi -f | convert -quality 100 -density 120 ps:- /path/to/right/md5.png) */
  switch(pid = fork()) {
    case -1:
      fprintf(stderr,"flt_latex: fork() error: %s\n",strerror(errno));
      cf_remove_recursive(path.content);
      str_cleanup(&path);
      return -1;

    case 0:
      if(pipe(fds) < 0) {
        fprintf(stderr,"flt_latex: pipe() error: %s\n",strerror(errno));
        return -1;
      }

      switch(pid = fork()) {
        case -1:
          exit(-1);

        case 0:
          dup2(fds[0],STDIN_FILENO);
          close(STDOUT_FILENO);
          close(fds[1]);

          str_cleanup(&path);
          str_char_set(&path,flt_latex_cfg.cache_path,strlen(flt_latex_cfg.cache_path));
          str_char_append(&path,'/');
          str_chars_append(&path,our_sum,32);
          str_chars_append(&path,".png",4);

          if(execlp(flt_latex_cfg.convert,flt_latex_cfg.convert,"-quality","100","-density","120","ps:-",path.content,NULL) == -1) {
            fprintf(stderr,"flt_latex: execlp: could not execlp '%s': %s\n",flt_latex_cfg.convert,strerror(errno));
            exit(-1);
          }

          /* obscure systems, you know */
          exit(0);

        default:
          chdir(path.content);

          dup2(fds[1],STDOUT_FILENO);
          close(fds[0]);
          close(STDIN_FILENO);
          close(STDERR_FILENO);

          if(execlp(flt_latex_cfg.dvips,flt_latex_cfg.dvips,"-R","-E","file.dvi","-f",NULL) == -1) {
            fprintf(stderr,"flt_latex: execlp: could not execlp '%s': %s\n",flt_latex_cfg.dvips,strerror(errno));
            exit(-1);
          }

          /* obscure systems, again */
          exit(0);
      }

    default:
      waitpid(pid,&status,0);
  }

  if(WEXITSTATUS(status) != 0) {
    fprintf(stderr,"flt_latex: execlp(%s)'s exit status is not 0 but %d!\n",flt_latex_cfg.tex,WEXITSTATUS(status));
    cf_remove_recursive(path.content);
    str_cleanup(&path);
    return -1;
  }
  /* }}} */

  cf_remove_recursive(path.content);
  str_cleanup(&path);

  return 0;
}
/* }}} */

/* {{{ flt_latex_create_md5_sum */
void flt_latex_create_md5_sum(u_char *str,size_t len,u_char *res) {
  u_char sum[16];
  int i;

  MD5(str,len,sum);

  for(i=0;i<16;++i,res+=2) sprintf(res,"%02x",sum[i]);
  *res = '\0';
}
/* }}} */

/* {{{ flt_latex_filter */
u_char *flt_latex_filter(const u_char *cnt,size_t *len) {
  u_char *val;
  string_t str;
  register u_char *ptr;

  str_init(&str);

  for(ptr=(u_char *)cnt;*ptr;++ptr) {
    switch(*ptr) {
      case '<':
        if(cf_strncmp(ptr,"<br>",4) == 0 || cf_strncmp(ptr,"<br />",6) == 0) str_char_append(&str,'\n');
        for(;*ptr && *ptr != '>';++ptr);
        continue;
      case '\\':
        if(cf_strncmp(ptr,"\\include",8) == 0 || cf_strncmp(ptr,"\\newcommand",11) == 0 || cf_strncmp(ptr,"\\renewcommand",13) == 0) {
          str_cleanup(&str);
          return NULL;
        }
      default:
        str_char_append(&str,*ptr);
    }
  }

  val = htmlentities_decode(str.content,len);
  str_cleanup(&str);

  return val;
}
/* }}} */

/* {{{ flt_latex_execute */
int flt_latex_execute(configuration_t *fdc,configuration_t *fvc,cl_thread_t *thread,const u_char *directive,const u_char **parameters,size_t plen,string_t *bco,string_t *bci,string_t *content,string_t *cite,const u_char *qchars,int sig) {
  u_char *fn;
  u_char sum[33];
  name_value_t *xhtml;
  size_t len;
  u_char *my_cnt;

  if(sig) return FLT_DECLINE;
  if((my_cnt = flt_latex_filter(content->content,&len)) == NULL) {
    fprintf(stderr,"Security violation! include, newcommand or renewcommand!\n");
    return FLT_DECLINE;
  }

  fn     =  cf_hash_get(GlobalValues,"FORUM_NAME",10);
  xhtml  = cfg_get_first_value(fdc,fn,"XHTMLMode");

  flt_latex_create_md5_sum(my_cnt,len,sum);

  if(flt_latex_create_cache(my_cnt,len,sum,cf_strcmp(directive,"elatex") == 0) != 0) {
    free(my_cnt);
    return FLT_DECLINE;
  }

  if(bci) {
    if(cf_strcmp(directive,"elatex") == 0) {
      str_chars_append(bci,"[elatex]",8);
      str_str_append(bci,cite);
      str_chars_append(bci,"[/elatex]",9);
    }
    else {
      str_chars_append(bci,"[latex]",7);
      str_str_append(bci,cite);
      str_chars_append(bci,"[/latex]",8);
    }
  }

  str_chars_append(bco,"<img src=\"",10);
  str_chars_append(bco,flt_latex_cfg.uri,strlen(flt_latex_cfg.uri));
  str_chars_append(bco,sum,32);
  str_chars_append(bco,".png\" alt=\"",11);
  str_str_append(bco,content);
  str_chars_append(bco,"\" title=\"",9);
  str_str_append(bco,content);
  str_char_append(bco,'"');

  if(*xhtml->values[0] == 'y') str_chars_append(bco," />",3);
  else str_char_append(bco,'>');

  free(my_cnt);

  return FLT_OK;
}
/* }}} */

int flt_latex_init(cf_hash_t *cgi,configuration_t *dc,configuration_t *vc) {
  cf_html_register_directive("latex",flt_latex_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);
  cf_html_register_directive("elatex",flt_latex_execute,CF_HTML_DIR_TYPE_ARG|CF_HTML_DIR_TYPE_BLOCK);

  return FLT_DECLINE;
}

/* {{{ flt_latex_handle */
int flt_latex_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  u_char *fn;

  if(cf_strcmp(opt->name,"LatexCachePath") == 0) {
    fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
    if(cf_strcmp(context,fn) == 0) {
      if(flt_latex_cfg.cache_path) free(flt_latex_cfg.cache_path);
      flt_latex_cfg.cache_path = strdup(args[0]);
    }
  }
  else if(cf_strcmp(opt->name,"LatexPathEnv") == 0) {
    if(flt_latex_cfg.path_env) free(flt_latex_cfg.path_env);
    flt_latex_cfg.path_env = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"LatexTmpPath") == 0) {
    if(flt_latex_cfg.tmp_path) free(flt_latex_cfg.tmp_path);
    flt_latex_cfg.tmp_path = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"LatexMode") == 0) {
#ifdef FUTURE_USE
    fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
    if(cf_strcmp(context,fn) == 0) {
      if(cf_strcmp(args[0],"INLINE") == 0) flt_latex_cfg.mode = FLT_LATEX_INLINE;
      else if(cf_strcmp(args[0],"OBJECT") == 0) flt_latex_cfg.mode = FLT_LATEX_OBJECT;
      else flt_latex_cfg.mode = FLT_LATEX_PNG;
    }
#endif
  }
  else if(cf_strcmp(opt->name,"LatexUri") == 0) {
    fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
    if(cf_strcmp(context,fn) == 0) {
      if(flt_latex_cfg.uri) free(flt_latex_cfg.uri);
      flt_latex_cfg.uri = strdup(args[0]);
    }
  }
  else if(cf_strcmp(opt->name,"LatexTeXPath") == 0) {
    if(flt_latex_cfg.tex) free(flt_latex_cfg.tex);
    flt_latex_cfg.tex = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"LatexDvipsPath") == 0) {
    if(flt_latex_cfg.dvips) free(flt_latex_cfg.dvips);
    flt_latex_cfg.dvips = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"LatexMzlatexPath") == 0) {
    if(flt_latex_cfg.mzlatex) free(flt_latex_cfg.mzlatex);
    flt_latex_cfg.mzlatex = strdup(args[0]);
  }
  else {
    if(flt_latex_cfg.convert) free(flt_latex_cfg.convert);
    flt_latex_cfg.convert = strdup(args[0]);
  }

  return 0;
}
/* }}} */

conf_opt_t flt_latex_config[] = {
  { "LatexTmpPath",     flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED, NULL },
  { "LatexTeXPath",     flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED, NULL },
  { "LatexConvertPath", flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED, NULL },
  { "LatexDvipsPath",   flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED, NULL },
  { "LatexPathEnv",     flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_GLOBAL|CFG_OPT_NEEDED, NULL },
  { "LatexCachePath",   flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED,  NULL },
  { "LatexUri",         flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_NEEDED,  NULL },
  { "LatexMode",        flt_latex_handle, CFG_OPT_CONFIG|CFG_OPT_USER|CFG_OPT_LOCAL,    NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_latex_handlers[] = {
  { INIT_HANDLER, flt_latex_init },
  { 0, NULL }
};

module_config_t flt_latex = {
  MODULE_MAGIC_COOKIE,
  flt_latex_config,
  flt_latex_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
