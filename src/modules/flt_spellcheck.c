/*
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
} t_flt_spellcheck_replacement;

static int flt_spellcheck_enabled = 0;
static u_char *flt_spellcheck_path = NULL;
static u_char *flt_spellcheck_dictionary = NULL;
static u_char *flt_spellcheck_formatter = NULL;

static u_char *flt_spellcheck_fn = NULL;

static t_array flt_spellcheck_options;

/*



*/

int flt_spellcheck_replacement_compare(t_flt_spellcheck_replacement *a, t_flt_spellcheck_replacement *b) {
  return a->start - b->start;
}


/* {{{ flt_spellcheck_execute */
#ifdef CF_SHARED_MEM
int flt_spellcheck_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,void *ptr,int sock,int mode)
#else
int flt_spellcheck_execute(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_message *p,int sock,int mode)
#endif
{
  // -a -S -C -d <dict> [-p <dict>] [-T <type>]
  long i, j, res, had_break = 0;
  char *ptr;
  char **ispell_argv;
  char *ispell_env[] = { NULL };
  char *cat_argv[] = { "cat", NULL };
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
  
  t_string spellchecker_input;
  t_string html_out;
  t_string tmp;
  t_string *ptmp;
  FILE *ispell_read;
  
  t_cf_hash_entry *ent;
  t_cf_cgi_param *param;
  t_name_value *v;
  
  t_array replacements;
  t_flt_spellcheck_replacement replacement;
  t_flt_spellcheck_replacement *r;
  
  if(!head) return FLT_DECLINE;
  if(!cf_cgi_get(head,"spellcheck")) {
    return FLT_DECLINE;
  }
  
  v = cfg_get_first_value(pc,forum_name,"QuotingChars");
  
  if(cf_cgi_get(head,"spellcheck_ok")) {
    array_init(&replacements,sizeof(t_flt_spellcheck_replacement),NULL);
    for(i=0;i<hashsize(head->tablesize);i++) {
      if(!head->table[i]) {
        continue;
      }
      for(ent = head->table[i];ent;ent=ent->next) {
        for(param = (t_cf_cgi_param *)ent->data;param;param=param->next) {
          if(!param->value) {
            continue;
          }
          if(cf_strncmp(param->name,"spelling_",9)) {
            continue;
          }
          ptr = param->name + 9;
          wpos = strtoul(ptr,&ptr,10);
          if(*ptr != '_') {
            continue;
          }
          ptr++;
          l = strtoul(ptr,&ptr,10);
          if(*ptr) {
            continue;
          }
          if(wpos < 0 || l < 0 || wpos > p->content.len || wpos + l > p->content.len) {
            continue;
          }
          replacement.replacement = param->value;
          replacement.start = wpos;
          replacement.len = l;
          array_push(&replacements,&replacement);
        }
      }
    }
    array_sort(&replacements,flt_spellcheck_replacement_compare);
    
    cpos = 0;
    str_init(&html_out);
    str_init(&tmp);
    for(j = 0; j < replacements.elements; j++) {
      r = (t_flt_spellcheck_replacement *)array_element_at(&replacements,j);
      for(i = cpos; i < p->content.len && i < r->start; i++) {
        ptr = p->content.content + i;
        if(*ptr == '<') {
          while(p->content.content[i] != '>' && i < p->content.len) i++;
          str_char_append(&html_out,'\n');
        }
        else if(*ptr == '\x7f') {
          str_cstr_append(&html_out,v->values[0]);
        }
        else {
          str_char_append(&html_out,*ptr);
        }
      }
      for(i = cpos; i < p->content.len && i < r->start; i++) {
        str_char_append(&tmp,p->content.content[i]);
      }
      str_cstr_append(&html_out,r->replacement);
      str_cstr_append(&tmp,r->replacement);
      cpos = r->start + r->len;
      if(cpos > p->content.len) {
        break;
      }
    }
    
    for(i = cpos; i < p->content.len; i++) {
      ptr = p->content.content + i;
      if(*ptr == '<') {
        while(p->content.content[i] != '>' && i < p->content.len) i++;
        str_char_append(&html_out,'\n');
      }
      else if(*ptr == '\x7f') {
        str_cstr_append(&html_out,v->values[0]);
      }
      else {
        str_char_append(&html_out,*ptr);
      }
    }
    for(i = cpos; i < p->content.len; i++) {
      str_char_append(&tmp,p->content.content[i]);
    }
    
    cf_cgi_set(head,"ne_body",html_out.content);
    str_cleanup(&p->content);
    str_init(&p->content);
    str_str_set(&p->content,&tmp);
    
    str_cleanup(&html_out);
    str_cleanup(&tmp);
    
    array_destroy(&replacements);
    
    cf_cgi_set(head,"preview","1");
    cf_hash_entry_delete(head,"spellcheck",10);
    cf_hash_entry_delete(head,"spellcheck_ok",13);
    return FLT_OK;
  }
  
  
  array_init(&flt_spellcheck_options,sizeof(t_string),str_cleanup);
  // generic options
  str_init(&tmp);
  str_char_set(&tmp,"-a",2);
  array_push(&flt_spellcheck_options,&tmp);
  str_init(&tmp);
  str_char_set(&tmp,"-S",2);
  array_push(&flt_spellcheck_options,&tmp);
  // allow compounds
  str_init(&tmp);
  str_char_set(&tmp,"-C",2);
  array_push(&flt_spellcheck_options,&tmp);
  // dictionary
  str_init(&tmp);
  str_char_set(&tmp,"-d",2);
  array_push(&flt_spellcheck_options,&tmp);
  str_init(&tmp);
  str_char_set(&tmp,flt_spellcheck_dictionary,strlen(flt_spellcheck_dictionary));
  array_push(&flt_spellcheck_options,&tmp);
  // TODO: personal dictionary
  // formatter
  if(flt_spellcheck_formatter) {
    str_init(&tmp);
    str_char_set(&tmp,"-T",2);
    str_chars_append(&tmp,flt_spellcheck_formatter,strlen(flt_spellcheck_formatter));
    array_push(&flt_spellcheck_options,&tmp);
  }
  
  str_init(&spellchecker_input);
  str_char_append(&spellchecker_input,'^');
  for(i = 0; i < p->content.len; i++) {
    ptr = p->content.content + i;
    if(*ptr == '<') { // ignore <br>
      while(i < p->content.len && p->content.content[i] != '>') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }
      str_char_append(&spellchecker_input,' ');
      had_break = 1;
      continue;
    }
    else if(!cf_strncmp(ptr,"_/_SIG_/_",9) && had_break) { // ignore signature
      break;
    }
    else if(*ptr == '\x7f') { // ignore citations
      while(i < p->content.len && p->content.content[i] != '<') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }
      // now, we need to make sure the <br> get's recognized by the next loop
      if(i) i--;
    }
    else if(*ptr == '&') { // only &amp;, &quot; etc.
      // never a relevant part of a word => ignore them
      while(i < p->content.len && p->content.content[i] != ';') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }
      str_char_append(&spellchecker_input,' ');
    }
    else if(*ptr == '[') {
      // we need a way to detect if this is a valid tag or not (or at least could be a valid tag)
      str_char_append(&spellchecker_input,*ptr);
    }
    else {
      str_char_append(&spellchecker_input,*ptr);
    }
    had_break = 0;
  }
  str_char_append(&spellchecker_input,'\n');
  
  ispell_argv = (char **)fo_alloc(NULL,sizeof(char *),flt_spellcheck_options.elements+2,FO_ALLOC_MALLOC);
  ispell_argv[0] = "ispell";
  for(i = 0; i < flt_spellcheck_options.elements; i++) {
    ptmp = (t_string *)array_element_at(&flt_spellcheck_options,i);
    ispell_argv[i+1] = ptmp->content;
  }
  ispell_argv[i+1] = NULL;
  res = ipc_dpopen(flt_spellcheck_path,ispell_argv,ispell_env,ispell_fds,&ispell_pid);
  //res = ipc_dpopen("/bin/cat",cat_argv,ispell_env,ispell_fds,&ispell_pid);
  free(ispell_argv);
  array_destroy(&flt_spellcheck_options);
  if(res != 0) {
    str_cleanup(&spellchecker_input);
    return FLT_DECLINE;
  }
  ispell_read = fdopen(ispell_fds[0],"r");
  if(!ispell_read) {
    str_cleanup(&spellchecker_input);
    ipc_dpclose(ispell_fds,&ispell_pid);
    return FLT_DECLINE;
  }
  n = write(ispell_fds[1],spellchecker_input.content,spellchecker_input.len);
  if (n != spellchecker_input.len) {
  str_cleanup(&spellchecker_input);
    ipc_dpclose(ispell_fds,&ispell_pid);
    return FLT_DECLINE;
  }
  str_cleanup(&spellchecker_input);
  close(ispell_fds[1]); // make sure ispell gets the eof
  
  cpos = 0;
  str_init(&html_out);
  
  while(fgets(linebuf,4095,ispell_read)) {
    linebuf[4095] = '\0';
    n = strlen(linebuf)-1;
    if (linebuf[n] == '\n') {
      linebuf[n] = '\0';
    }
    switch(linebuf[0]) {
      case '#': // unknown
        n = split((const u_char *)linebuf," ",&tmplist);
        if(n != 3) {
          for(m = 0; m < n; m++) {
            free(tmplist[m]);
          }
          if(n >= 0) {
            free(tmplist);
          }
          // ignore this line
          continue;
        }
        // tmplist: # WORD LEN
        l = strlen(tmplist[1]);
        wpos = strtoul(tmplist[2],NULL,10);
        for(i = cpos; i < wpos - 1; i++) {
          if(p->content.content[i] == '\x7f') {
            str_cstr_append(&html_out,v->values[0]); 
          } else {
            str_char_append(&html_out,p->content.content[i]);
          }
        }
        str_cstr_append(&html_out,"<select name=\"spelling_");
        snprintf(buf,19,"%d",wpos-1);
        str_cstr_append(&html_out,buf);
        str_char_append(&html_out,'_');
        snprintf(buf,19,"%d",l);
        str_cstr_append(&html_out,buf);
        str_cstr_append(&html_out,"\">");
        str_cstr_append(&html_out,"<option>");
        str_cstr_append(&html_out,tmplist[1]);
        str_cstr_append(&html_out,"</option>");
        // no misses - sorry
        str_cstr_append(&html_out,"</select>");
        for(m = 0; m < n; m++) {
          free(tmplist[m]);
        }
        free(tmplist);
        cpos = wpos + l - 1;
        break;
      case '&': // miss
        // first step: separate infos from misses
        n = split((const u_char *)linebuf,": ",&tmplist);
        if(n != 2) {
          for(m = 0; m < n; m++) {
            free(tmplist[m]);
          }
          if(n >= 0) {
            free(tmplist);
          }
          // ignore this line
          continue;
        }
        // second step
        n2 = split(tmplist[0]," ",&tmplist2);
        if(n2 != 4) {
          for(m = 0; m < n; m++) {
            free(tmplist[m]);
          }
          free(tmplist);
          for(m2 = 0; m2 < n2; m2++) {
            free(tmplist2[m2]);
          }
          if(n2 >= 0) {
            free(tmplist2);
          }
          // ignore this line
          continue;
        }
        // third step
        n3 = split(tmplist[1],", ",&tmplist3);
        if(n3 <= 0) {
          for(m = 0; m < n; m++) {
            free(tmplist[m]);
          }
          free(tmplist);
          for(m2 = 0; m2 < n2; m2++) {
            free(tmplist2[m2]);
          }
          free(tmplist2);
          for(m3 = 0; m3 < n3; m3++) {
            free(tmplist3[m3]);
          }
          if(n3 >= 0) {
            free(tmplist3);
          }
          // ignore this line
          continue;
        }
        // tmplist2: & WORD COUNT LEN
        l = strlen(tmplist2[1]);
        wpos = strtoul(tmplist2[3],NULL,10);
        for(i = cpos; i < wpos - 1; i++) {
          if(p->content.content[i] == '\x7f') {
            str_cstr_append(&html_out,v->values[0]); 
          } else {
            str_char_append(&html_out,p->content.content[i]);
          }
        }
        str_cstr_append(&html_out,"<select name=\"spelling_");
        snprintf(buf,19,"%d",wpos-1);
        str_cstr_append(&html_out,buf);
        str_char_append(&html_out,'_');
        snprintf(buf,19,"%d",l);
        str_cstr_append(&html_out,buf);
        str_cstr_append(&html_out,"\">");
        str_cstr_append(&html_out,"<option>");
        str_cstr_append(&html_out,tmplist2[1]);
        str_cstr_append(&html_out,"</option>");
        // append misses
        for(m3 = 0; m3 < n3; m3++) {
          str_cstr_append(&html_out,"<option>");
          str_cstr_append(&html_out,tmplist3[m3]);
          str_cstr_append(&html_out,"</option>");
        }
        str_cstr_append(&html_out,"</select>");
        for(m = 0; m < n; m++) {
          free(tmplist[m]);
        }
        free(tmplist);
        for(m2 = 0; m2 < n2; m2++) {
          free(tmplist2[m2]);
        }
        free(tmplist2);
        for(m3 = 0; m3 < n3; m3++) {
          free(tmplist3[m3]);
        }
        free(tmplist3);
        cpos = wpos + l - 1;
        break;
      default: // do nothing
        break;
    }
  }
  
  for(i = cpos; i < p->content.len; i++) {
    if(p->content.content[i] == '\x7f') {
      str_cstr_append(&html_out,v->values[0]); 
    } else {
      str_char_append(&html_out,p->content.content[i]);
    }
  }
  cf_cgi_set(head,"do_spellcheck","1");
  cf_cgi_set(head,"ne_html_txt",html_out.content);
  str_cleanup(&html_out);
  str_init(&html_out);
  for(i = 0; i < p->content.len; i++) {
    ptr = p->content.content + i;
    if(*ptr == '<') {
      while(p->content.content[i] != '>' && i < p->content.len) i++;
      str_char_append(&html_out,'\n');
    }
    else if(*ptr == '\x7f') {
      str_cstr_append(&html_out,v->values[0]);
    }
    else {
      str_char_append(&html_out,*ptr);
    }
  }
  cf_cgi_set(head,"orig_txt",html_out.content);
  str_cleanup(&html_out);
  
  fclose(ispell_read);
  ipc_dpclose(NULL,&ispell_pid);
  
  display_posting_form(head,p);
  return FLT_EXIT;
}
/* }}} */

/* {{{ flt_spellcheck_variables */
int flt_spellcheck_variables(t_cf_hash *head,t_configuration *dc,t_configuration *vc,t_cl_thread *thread,t_cf_template *tpl) {
  if(flt_spellcheck_enabled) cf_tpl_setvalue(tpl,"spellcheck_enabled",TPL_VARIABLE_INT,1);
  
  return FLT_OK;
}
/* }}} */

int flt_spellcheck_variables_posting(t_cf_hash *head,t_configuration *dc,t_configuration *pc,t_cf_template *tpl) {
  return flt_spellcheck_variables(NULL,NULL,NULL,NULL,tpl);
}

/* {{{ flt_spellcheck_cmd */
int flt_spellcheck_cmd(t_configfile *cfile,t_conf_opt *opt,const u_char *context,u_char **args,size_t argnum) {
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

t_conf_opt flt_spellcheck_config[] = {
  { "SpellCheckerEnabled",       flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "SpellCheckerPath",          flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "SpellCheckerDictionary",    flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "SpellCheckerFormatterType", flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_spellcheck_handlers[] = {
  { NEW_POST_HANDLER,     flt_spellcheck_execute },
  { POSTING_HANDLER,      flt_spellcheck_variables },
  { POST_DISPLAY_HANDLER, flt_spellcheck_variables_posting },
  { 0, NULL }
};

t_module_config flt_spellcheck = {
  flt_spellcheck_config,
  flt_spellcheck_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
