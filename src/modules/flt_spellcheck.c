/**
 * \file flt_spellcheck.c
 * \author Christian Seiler, <self@christian-seiler.de>
 * \brief Posting preview
 *
 * This file is a plugin for fo_post. It gives the user the
 * ability to do a spelling check on his posting.
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
#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
#include "htmllib.h"
#include "fo_post.h"
/* }}} */

typedef struct s_flt_spellcheck_replacement {
  int start;
  int len;
  const u_char *replacement;
} flt_spellcheck_replacement_t;

static int flt_spellcheck_enabled        = 0;
static u_char *flt_spellcheck_path       = NULL;
static u_char *flt_spellcheck_dictionary = NULL;
static u_char *flt_spellcheck_formatter  = NULL;

static u_char *flt_spellcheck_fn         = NULL;

static cf_array_t flt_spellcheck_options;

int flt_spellcheck_replacement_compare(flt_spellcheck_replacement_t *a, flt_spellcheck_replacement_t *b) {
  return a->start - b->start;
}

/* {{{ flt_spellcheck_execute */
#ifdef CF_SHARED_MEM
int flt_spellcheck_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,void *shm_ptr,int sock,int mode)
#else
int flt_spellcheck_execute(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  // -a -S -C -d <dict> [-p <dict>] [-T <type>]
  long i, j, res, had_break = 0;
  char *ptr;
  char **ispell_argv;
  char *ispell_env[] = { NULL };
  char linebuf[4096];
  char buf[20];
  u_char **tmplist;
  u_char **tmplist2;
  u_char **tmplist3;
  int ispell_fds[2];
  int cpos;
  int wpos;
  pid_t ispell_pid;
  size_t m,n,l,m2,n2,m3,n3;
  
  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  
  cf_string_t spellchecker_input;
  cf_string_t html_out;
  cf_string_t tmp;
  cf_string_t *ptmp;
  FILE *ispell_read;
  
  cf_hash_entry_t *ent;
  cf_cgi_param_t *param;
  cf_name_value_t *v;
  
  cf_array_t replacements;
  flt_spellcheck_replacement_t replacement;
  flt_spellcheck_replacement_t *r;
  
  if(!head) return FLT_DECLINE;
  if(!cf_cgi_get(head,"spellcheck")) {
    return FLT_DECLINE;
  }
  
  v = cf_cfg_get_first_value(pc,forum_name,"QuotingChars");
  
  if(cf_cgi_get(head,"spellcheck_ok")) {
    cf_array_init(&replacements,sizeof(flt_spellcheck_replacement_t),NULL);
    for(i=0;(size_t)i<cf_hashsize(head->tablesize);i++) {
      if(!head->table[i]) continue;

      for(ent = head->table[i];ent;ent=ent->next) {
        for(param = (cf_cgi_param_t *)ent->data;param;param=param->next) {
          if(!param->value) continue;
          if(cf_strncmp(param->name,"spelling_",9)) continue;

          ptr = param->name + 9;
          wpos = strtoul(ptr,&ptr,10);

          if(*ptr != '_') continue;

          ptr++;
          l = strtoul(ptr,&ptr,10);

          if(*ptr) continue;
          if(wpos < 0 || l < 0 || (size_t)wpos > p->content.len || (size_t)(wpos + l) > p->content.len) continue;

          replacement.replacement = param->value;
          replacement.start = wpos;
          replacement.len = l;
          cf_array_push(&replacements,&replacement);
        }
      }
    }

    cf_array_sort(&replacements,(int (*)(const void *,const void *))flt_spellcheck_replacement_compare);
    
    cpos = 0;
    cf_str_init(&html_out);
    cf_str_init(&tmp);
    for(j = 0; (size_t)j < replacements.elements; j++) {
      r = (flt_spellcheck_replacement_t *)cf_array_element_at(&replacements,j);

      for(i = cpos; (size_t)i < p->content.len && i < r->start; i++) {
        ptr = p->content.content + i;

        if(*ptr == '<') {
          while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
          cf_str_char_append(&html_out,'\n');
        }
        else if(*ptr == '\x7f') cf_str_cstr_append(&html_out,v->values[0]);
        else cf_str_char_append(&html_out,*ptr);
      }

      for(i = cpos; (size_t)i < p->content.len && i < r->start; i++) cf_str_char_append(&tmp,p->content.content[i]);

      cf_str_cstr_append(&html_out,r->replacement);
      cf_str_cstr_append(&tmp,r->replacement);
      cpos = r->start + r->len;

      if((size_t)cpos > p->content.len) break;
    }
    
    for(i = cpos; (size_t)i < p->content.len; i++) {
      ptr = p->content.content + i;

      if(*ptr == '<') {
        while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
        cf_str_char_append(&html_out,'\n');
      }
      else if(*ptr == '\x7f') cf_str_cstr_append(&html_out,v->values[0]);
      else if(!cf_strncmp(ptr,"_/_SIG_/_",9)) {
        cf_str_cstr_append(&html_out,"\n-- \n");
        i += 8;
      }
      else cf_str_char_append(&html_out,*ptr);
    }

    for(i = cpos; (size_t)i < p->content.len; i++) cf_str_char_append(&tmp,p->content.content[i]);
    
    cf_cgi_set(head,"ne_body",html_out.content);
    cf_str_cleanup(&p->content);
    cf_str_init(&p->content);
    cf_str_str_set(&p->content,&tmp);
    
    cf_str_cleanup(&html_out);
    cf_str_cleanup(&tmp);
    
    cf_array_destroy(&replacements);
    
    cf_cgi_set(head,"preview","1");
    cf_hash_entry_delete(head,"spellcheck",10);
    cf_hash_entry_delete(head,"spellcheck_ok",13);

    return FLT_OK;
  }
  
  cf_array_init(&flt_spellcheck_options,sizeof(cf_string_t),(void (*)(void *))cf_str_cleanup);

  // generic options
  cf_str_init(&tmp);
  cf_str_char_set(&tmp,"-a",2);
  cf_array_push(&flt_spellcheck_options,&tmp);
  cf_str_init(&tmp);
  cf_str_char_set(&tmp,"-S",2);
  cf_array_push(&flt_spellcheck_options,&tmp);

  // allow compounds
  cf_str_init(&tmp);
  cf_str_char_set(&tmp,"-C",2);
  cf_array_push(&flt_spellcheck_options,&tmp);

  // dictionary
  cf_str_init(&tmp);
  cf_str_char_set(&tmp,"-d",2);
  cf_array_push(&flt_spellcheck_options,&tmp);
  cf_str_init(&tmp);
  cf_str_char_set(&tmp,flt_spellcheck_dictionary,strlen(flt_spellcheck_dictionary));
  cf_array_push(&flt_spellcheck_options,&tmp);

  // TODO: personal dictionary
  // formatter
  if(flt_spellcheck_formatter) {
    cf_str_init(&tmp);
    cf_str_char_set(&tmp,"-T",2);
    cf_str_chars_append(&tmp,flt_spellcheck_formatter,strlen(flt_spellcheck_formatter));
    cf_array_push(&flt_spellcheck_options,&tmp);
  }
  
  cf_str_init(&spellchecker_input);
  cf_str_char_append(&spellchecker_input,'^');
  for(i = 0; (size_t)i < p->content.len; i++) {
    ptr = p->content.content + i;

    if(*ptr == '<') { // ignore <br>
      while((size_t)i < p->content.len && p->content.content[i] != '>') {
        cf_str_char_append(&spellchecker_input,' ');
        i++;
      }

      cf_str_char_append(&spellchecker_input,' ');
      had_break = 1;
      continue;
    }
    else if(!cf_strncmp(ptr,"_/_SIG_/_",9)) break; // ignore signature
    else if(*ptr == '\x7f') { // ignore cites
      while((size_t)i < p->content.len && p->content.content[i] != '<') {
        cf_str_char_append(&spellchecker_input,' ');
        i++;
      }

      // now, we need to make sure the <br> get's recognized by the next loop
      if(i) i--;
    }
    else if(*ptr == '&') { // only &amp;, &quot; etc.
      // never a relevant part of a word => ignore them
      while((size_t)i < p->content.len && p->content.content[i] != ';') {
        cf_str_char_append(&spellchecker_input,' ');
        i++;
      }

      cf_str_char_append(&spellchecker_input,' ');
    }
    else if(*ptr == '[') cf_str_char_append(&spellchecker_input,*ptr); // we need a way to detect if this is a valid tag or not (or at least could be a valid tag)
    else cf_str_char_append(&spellchecker_input,*ptr);

    had_break = 0;
  }

  cf_str_char_append(&spellchecker_input,'\n');
  
  ispell_argv = (char **)cf_alloc(NULL,sizeof(char *),flt_spellcheck_options.elements+2,CF_ALLOC_MALLOC);
  ispell_argv[0] = "ispell";

  for(i = 0; (size_t)i < flt_spellcheck_options.elements; i++) {
    ptmp = (cf_string_t *)cf_array_element_at(&flt_spellcheck_options,i);
    ispell_argv[i+1] = ptmp->content;
  }

  ispell_argv[i+1] = NULL;
  res = cf_ipc_dpopen(flt_spellcheck_path,ispell_argv,ispell_env,ispell_fds,&ispell_pid);
  //res = cf_ipc_dpopen("/bin/cat",cat_argv,ispell_env,ispell_fds,&ispell_pid);
  free(ispell_argv);
  cf_array_destroy(&flt_spellcheck_options);

  if(res != 0) {
    cf_str_cleanup(&spellchecker_input);
    return FLT_DECLINE;
  }

  
  if((ispell_read = fdopen(ispell_fds[0],"r")) == NULL) {
    cf_str_cleanup(&spellchecker_input);
    cf_ipc_dpclose(ispell_fds,&ispell_pid);
    return FLT_DECLINE;
  }

  n = write(ispell_fds[1],spellchecker_input.content,spellchecker_input.len);
  if(n != spellchecker_input.len) {
    cf_str_cleanup(&spellchecker_input);
    cf_ipc_dpclose(ispell_fds,&ispell_pid);
    return FLT_DECLINE;
  }

  cf_str_cleanup(&spellchecker_input);
  close(ispell_fds[1]); // make sure ispell gets the eof
  
  cpos = 0;
  cf_str_init(&html_out);
  
  while(fgets(linebuf,4095,ispell_read)) {
    linebuf[4095] = '\0';
    n = strlen(linebuf)-1;
    if(linebuf[n] == '\n') linebuf[n] = '\0';

    switch(linebuf[0]) {
      case '#': // unknown
        n = cf_split((const u_char *)linebuf," ",&tmplist);
        if(n != 3) {
          for(m = 0; m < n; m++) free(tmplist[m]);
          if(n >= 0) free(tmplist);

          // ignore this line
          continue;
        }

        // tmplist: # WORD LEN
        l = strlen(tmplist[1]);
        wpos = strtoul(tmplist[2],NULL,10);

        for(i = cpos; i < wpos - 1; i++) {
          if(p->content.content[i] == '\x7f') cf_str_cstr_append(&html_out,v->values[0]); 
          else cf_str_char_append(&html_out,p->content.content[i]);
        }

        cf_str_cstr_append(&html_out,"<select name=\"spelling_");
        snprintf(buf,19,"%d",wpos-1);
        cf_str_cstr_append(&html_out,buf);
        cf_str_char_append(&html_out,'_');
        snprintf(buf,19,"%d",l);
        cf_str_cstr_append(&html_out,buf);
        cf_str_cstr_append(&html_out,"\">");
        cf_str_cstr_append(&html_out,"<option>");
        cf_str_cstr_append(&html_out,tmplist[1]);
        cf_str_cstr_append(&html_out,"</option>");
        // no misses - sorry
        cf_str_cstr_append(&html_out,"</select>");

        for(m = 0; m < n; m++) free(tmplist[m]);

        free(tmplist);
        cpos = wpos + l - 1;
        break;

      case '&': // miss
        // first step: separate infos from misses
        n = cf_split((const u_char *)linebuf,": ",&tmplist);
        if(n != 2) {
          for(m = 0; m < n; m++) free(tmplist[m]);
          if(n >= 0) free(tmplist);

          // ignore this line
          continue;
        }

        // second step
        n2 = cf_split(tmplist[0]," ",&tmplist2);
        if(n2 != 4) {
          for(m = 0; m < n; m++) free(tmplist[m]);
          free(tmplist);

          for(m2 = 0; m2 < n2; m2++) free(tmplist2[m2]);
          if(n2 >= 0) free(tmplist2);

          // ignore this line
          continue;
        }

        // third step
        n3 = cf_split(tmplist[1],", ",&tmplist3);
        if(n3 <= 0) {
          for(m = 0; m < n; m++) free(tmplist[m]);
          free(tmplist);

          for(m2 = 0; m2 < n2; m2++) free(tmplist2[m2]);
          free(tmplist2);

          for(m3 = 0; m3 < n3; m3++) free(tmplist3[m3]);
          if(n3 >= 0) free(tmplist3);

          // ignore this line
          continue;
        }

        // tmplist2: & WORD COUNT LEN
        l = strlen(tmplist2[1]);
        wpos = strtoul(tmplist2[3],NULL,10);
        for(i = cpos; i < wpos - 1; i++) {
          if(p->content.content[i] == '\x7f') cf_str_cstr_append(&html_out,v->values[0]); 
          else cf_str_char_append(&html_out,p->content.content[i]);
        }

        cf_str_cstr_append(&html_out,"<select name=\"spelling_");
        snprintf(buf,19,"%d",wpos-1);
        cf_str_cstr_append(&html_out,buf);
        cf_str_char_append(&html_out,'_');
        snprintf(buf,19,"%d",l);
        cf_str_cstr_append(&html_out,buf);
        cf_str_cstr_append(&html_out,"\">");
        cf_str_cstr_append(&html_out,"<option>");
        cf_str_cstr_append(&html_out,tmplist2[1]);
        cf_str_cstr_append(&html_out,"</option>");

        // append misses
        for(m3 = 0; m3 < n3; m3++) {
          cf_str_cstr_append(&html_out,"<option>");
          cf_str_cstr_append(&html_out,tmplist3[m3]);
          cf_str_cstr_append(&html_out,"</option>");
        }

        cf_str_cstr_append(&html_out,"</select>");
        for(m = 0; m < n; m++) free(tmplist[m]);
        free(tmplist);

        for(m2 = 0; m2 < n2; m2++) free(tmplist2[m2]);
        free(tmplist2);

        for(m3 = 0; m3 < n3; m3++) free(tmplist3[m3]);
        free(tmplist3);

        cpos = wpos + l - 1;
        break;

      default: // do nothing
        break;
    }
  }
  
  for(i = cpos; (size_t)i < p->content.len; i++) {
    if(p->content.content[i] == '\x7f') cf_str_cstr_append(&html_out,v->values[0]); 
    else if(!cf_strncmp(p->content.content+i,"_/_SIG_/_",9)) {
      cf_str_cstr_append(&html_out,"<br />-- <br />"); // BUG: XHTML Mode Checking
      i += 8;
    }
    else cf_str_char_append(&html_out,p->content.content[i]);
  }

  cf_cgi_set(head,"do_spellcheck","1");
  cf_cgi_set(head,"ne_html_txt",html_out.content);
  cf_str_cleanup(&html_out);
  cf_str_init(&html_out);

  for(i = 0; (size_t)i < p->content.len; i++) {
    ptr = p->content.content + i;

    if(*ptr == '<') {
      while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
      cf_str_char_append(&html_out,'\n');
    }
    else if(*ptr == '\x7f') cf_str_cstr_append(&html_out,v->values[0]);
    else if(*ptr == '&') {
      if(!cf_strncmp(ptr,"&amp;",5)) {
        cf_str_cstr_append(&html_out,"&");
        i += 4;
      }
      else if(!cf_strncmp(ptr,"&lt;",4)) {
        cf_str_cstr_append(&html_out,"<");
        i += 3;
      }
      else if(!cf_strncmp(ptr,"&gt;",4)) {
        cf_str_cstr_append(&html_out,">");
        i += 3;
      }
      else if(!cf_strncmp(ptr,"&quot;",6)) {
        cf_str_cstr_append(&html_out,"\"");
        i += 5;
      }
      else {
        cf_str_char_append(&html_out,*ptr);
      }
    }
    else if(!cf_strncmp(ptr,"_/_SIG_/_",9)) {
      cf_str_cstr_append(&html_out,"\n-- \n");
      i += 8;
    }
    else cf_str_char_append(&html_out,*ptr);
  }

  cf_cgi_set(head,"orig_txt",html_out.content);
  cf_str_cleanup(&html_out);
  
  fclose(ispell_read);
  cf_ipc_dpclose(NULL,&ispell_pid);
  
  display_posting_form(head,p,NULL);
  return FLT_EXIT;
}

/* }}} */

/* {{{ flt_spellcheck_variables */
int flt_spellcheck_variables(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  if(flt_spellcheck_enabled) cf_tpl_setvalue(tpl,"spellcheck_enabled",TPL_VARIABLE_INT,1);
  
  return FLT_OK;
}
/* }}} */

int flt_spellcheck_variables_posting(cf_hash_t *head,cf_configuration_t *dc,cf_configuration_t *pc,cf_template_t *tpl) {
  return flt_spellcheck_variables(NULL,NULL,NULL,NULL,tpl);
}

/* {{{ flt_spellcheck_cmd */
int flt_spellcheck_cmd(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_spellcheck_fn == NULL) flt_spellcheck_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_spellcheck_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SpellCheckerEnabled") == 0) {
    flt_spellcheck_enabled = cf_strcmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"SpellCheckerPath") == 0) {
    if(flt_spellcheck_path) free(flt_spellcheck_path);
    flt_spellcheck_path = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"SpellCheckerDictionary") == 0) {
    if(flt_spellcheck_dictionary) free(flt_spellcheck_dictionary);
    flt_spellcheck_dictionary = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"SpellCheckerFormatterType") == 0) {
    if(flt_spellcheck_formatter) free(flt_spellcheck_formatter);
    flt_spellcheck_formatter = strdup(args[0]);
  }

  return 0;
}
/* }}} */

cf_conf_opt_t flt_spellcheck_config[] = {
  { "SpellCheckerEnabled",       flt_spellcheck_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { "SpellCheckerPath",          flt_spellcheck_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { "SpellCheckerDictionary",    flt_spellcheck_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, NULL },
  { "SpellCheckerFormatterType", flt_spellcheck_cmd, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_spellcheck_handlers[] = {
  { NEW_POST_HANDLER,     flt_spellcheck_execute },
  { POSTING_HANDLER,      flt_spellcheck_variables },
  { POST_DISPLAY_HANDLER, flt_spellcheck_variables_posting },
  { 0, NULL }
};

cf_module_config_t flt_spellcheck = {
  MODULE_MAGIC_COOKIE,
  flt_spellcheck_config,
  flt_spellcheck_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
