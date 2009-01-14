/**
 * \file flt_spellcheck.c
 * \author Christian Seiler, <self@christian-seiler.de>
 * \brief Spell checker
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

#include <aspell.h>

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
static int flt_spellcheck_activated_dflt = 0;
static u_char *flt_spellcheck_language   = NULL;

static u_char *flt_spellcheck_fn         = NULL;

int flt_spellcheck_replacement_compare(flt_spellcheck_replacement_t *a, flt_spellcheck_replacement_t *b) {
  return a->start - b->start;
}

/* {{{ flt_spellcheck_execute */
#ifdef CF_SHARED_MEM
int flt_spellcheck_execute(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,void *shm_ptr,int sock,int mode)
#else
int flt_spellcheck_execute(cf_hash_t *head,configuration_t *dc,configuration_t *pc,message_t *p,cl_thread_t *thr,int sock,int mode)
#endif
{
  long i, j, had_break = 0;
  char *ptr;
  char buf[20];
  int cpos, wpos, l;

  u_char *forum_name = cf_hash_get(GlobalValues,"FORUM_NAME",10);

  string_t spellchecker_input;
  string_t html_out;
  string_t tmp;

  cf_cgi_param_t *param;
  name_value_t *v;

  array_t replacements;
  flt_spellcheck_replacement_t replacement;
  flt_spellcheck_replacement_t *r;

  cf_hash_keylist_t *keyel;

  AspellCanHaveError *possible_err;
  AspellSpeller *spell_checker = NULL;
  AspellDocumentChecker *document_checker = NULL;
  AspellConfig *spell_config = new_aspell_config();
  AspellToken miss;
  const AspellWordList *suggestions;
  AspellStringEnumeration * elements;
  const char *word;

  if(!head) return FLT_DECLINE;
  if(!cf_cgi_get(head,"spellcheck")) {
    return FLT_DECLINE;
  }

  v = cfg_get_first_value(pc,forum_name,"QuotingChars");

  if(cf_cgi_get(head,"spellcheck_ok")) {
    array_init(&replacements,sizeof(flt_spellcheck_replacement_t),NULL);
    for(keyel=head->keys.elems;keyel;keyel=keyel->next) {
      for(param=cf_cgi_get_multiple(head,keyel->key);param;param=param->next) {
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
        array_push(&replacements,&replacement);
      }
    }

    array_sort(&replacements,(int (*)(const void *,const void *))flt_spellcheck_replacement_compare);

    cpos = 0;
    str_init(&html_out);
    str_init(&tmp);
    for(j = 0; (size_t)j < replacements.elements; j++) {
      r = (flt_spellcheck_replacement_t *)array_element_at(&replacements,j);

      for(i = cpos; (size_t)i < p->content.len && i < r->start; i++) {
        ptr = p->content.content + i;

        if(*ptr == '<') {
          while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
          str_char_append(&html_out,'\n');
        }
        else if(*ptr == '\x7f') str_cstr_append(&html_out,v->values[0]);
        else str_char_append(&html_out,*ptr);
      }

      for(i = cpos; (size_t)i < p->content.len && i < r->start; i++) str_char_append(&tmp,p->content.content[i]);

      str_cstr_append(&html_out,r->replacement);
      str_cstr_append(&tmp,r->replacement);
      cpos = r->start + r->len;

      if((size_t)cpos > p->content.len) break;
    }

    for(i = cpos; (size_t)i < p->content.len; i++) {
      ptr = p->content.content + i;

      if(*ptr == '<') {
        while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
        str_char_append(&html_out,'\n');
      }
      else if(*ptr == '\x7f') str_cstr_append(&html_out,v->values[0]);
      else str_char_append(&html_out,*ptr);
    }

    for(i = cpos; (size_t)i < p->content.len; i++) str_char_append(&tmp,p->content.content[i]);

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

  // Initialize ASPELL
  aspell_config_replace(spell_config, "lang", flt_spellcheck_language ? (char *)flt_spellcheck_language : "en_US");
  aspell_config_replace(spell_config, "encoding", "utf-8");
  possible_err = new_aspell_speller(spell_config);
  if (aspell_error_number(possible_err) != 0) {
    fprintf(stderr, "[warning] aspell initialization error: %s\n", aspell_error_message(possible_err));
    return FLT_DECLINE;
  }

  spell_checker = to_aspell_speller(possible_err);
  possible_err = new_aspell_document_checker(spell_checker);
  if (aspell_error_number(possible_err) != 0) {
    fprintf(stderr, "[warning] aspell initialization error: %s\n", aspell_error_message(possible_err));
    return FLT_DECLINE;
  }

  document_checker = to_aspell_document_checker(possible_err);

  str_init(&spellchecker_input);
  for(i = 0; (size_t)i < p->content.len; i++) {
    ptr = p->content.content + i;

    if(*ptr == '<') { // ignore <br>
      while((size_t)i < p->content.len && p->content.content[i] != '>') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }

      str_char_append(&spellchecker_input,' ');
      had_break = 1;
      continue;
    }
    else if(*ptr == '\x7f') { // ignore cites
      while((size_t)i < p->content.len && p->content.content[i] != '<') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }

      // now, we need to make sure the <br> get's recognized by the next loop
      if(i) i--;
    }
    else if(*ptr == '&') { // only &amp;, &quot; etc.
      // never a relevant part of a word => ignore them
      while((size_t)i < p->content.len && p->content.content[i] != ';') {
        str_char_append(&spellchecker_input,' ');
        i++;
      }

      str_char_append(&spellchecker_input,' ');
    }
    else if(*ptr == '[') str_char_append(&spellchecker_input,*ptr); // we need a way to detect if this is a valid tag or not (or at least could be a valid tag)
    else str_char_append(&spellchecker_input,*ptr);

    had_break = 0;
  }

  aspell_document_checker_process(document_checker, spellchecker_input.content, spellchecker_input.len);

  cpos = 0;
  str_init(&html_out);

  while(miss = aspell_document_checker_next_misspelling(document_checker), miss.len > 0) {
    for(i = cpos; (unsigned int)i < miss.offset; i++) {
      if(p->content.content[i] == '\x7f') str_cstr_append(&html_out,v->values[0]);
      else str_char_append(&html_out,p->content.content[i]);
    }

    str_cstr_append(&html_out,"<select name=\"spelling_");
    snprintf(buf,19,"%d",miss.offset);
    str_cstr_append(&html_out,buf);
    str_char_append(&html_out,'_');
    snprintf(buf,19,"%zu",miss.len);
    str_cstr_append(&html_out,buf);
    str_cstr_append(&html_out,"\">");
    str_cstr_append(&html_out,"<option>");
    str_chars_append(&html_out,spellchecker_input.content+miss.offset,miss.len);
    str_cstr_append(&html_out,"</option>");
    suggestions = aspell_speller_suggest(spell_checker,spellchecker_input.content+miss.offset,miss.len);
    elements = aspell_word_list_elements(suggestions);
    while((word = aspell_string_enumeration_next(elements)) != NULL) {
      str_cstr_append(&html_out,"<option>");
      str_cstr_append(&html_out,word);
      str_cstr_append(&html_out,"</option>");
    }
    delete_aspell_string_enumeration(elements);
    str_cstr_append(&html_out,"</select>");
    cpos = miss.offset+miss.len;
  }

  // CLEANUP
  str_cleanup(&spellchecker_input);

  delete_aspell_document_checker(document_checker);
  delete_aspell_speller(spell_checker);
  delete_aspell_config(spell_config);

  for(i = cpos; (size_t)i < p->content.len; i++) {
    if(p->content.content[i] == '\x7f') str_cstr_append(&html_out,v->values[0]);
    else str_char_append(&html_out,p->content.content[i]);
  }

  cf_cgi_set(head,"do_spellcheck","1");
  cf_cgi_set(head,"ne_html_txt",html_out.content);
  str_cleanup(&html_out);
  str_init(&html_out);

  for(i = 0; (size_t)i < p->content.len; i++) {
    ptr = p->content.content + i;

    if(*ptr == '<') {
      while(p->content.content[i] != '>' && (size_t)i < p->content.len) i++;
      str_char_append(&html_out,'\n');
    }
    else if(*ptr == '\x7f') str_cstr_append(&html_out,v->values[0]);
    else if(*ptr == '&') {
      if(!cf_strncmp(ptr,"&amp;",5)) {
        str_cstr_append(&html_out,"&");
        i += 4;
      }
      else if(!cf_strncmp(ptr,"&lt;",4)) {
        str_cstr_append(&html_out,"<");
        i += 3;
      }
      else if(!cf_strncmp(ptr,"&gt;",4)) {
        str_cstr_append(&html_out,">");
        i += 3;
      }
      else if(!cf_strncmp(ptr,"&quot;",6)) {
        str_cstr_append(&html_out,"\"");
        i += 5;
      }
      else {
        str_char_append(&html_out,*ptr);
      }
    }
    else str_char_append(&html_out,*ptr);
  }

  cf_cgi_set(head,"orig_txt",html_out.content);
  str_cleanup(&html_out);

  display_posting_form(head,p,NULL);
  return FLT_EXIT;
}

/* }}} */

/* {{{ flt_spellcheck_variables */
int flt_spellcheck_variables(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thread,cf_template_t *tpl) {
  if(flt_spellcheck_enabled) cf_tpl_setvalue(tpl,"spellcheck_enabled",TPL_VARIABLE_INT,1);
  if(flt_spellcheck_activated_dflt) cf_tpl_setvalue(tpl,"spellcheck_activated",TPL_VARIABLE_INT,1);
  return FLT_OK;
}
/* }}} */

int flt_spellcheck_variables_posting(cf_hash_t *head,configuration_t *dc,configuration_t *pc,cf_template_t *tpl) {
  return flt_spellcheck_variables(NULL,NULL,NULL,NULL,tpl);
}

/* {{{ flt_spellcheck_cmd */
int flt_spellcheck_cmd(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_spellcheck_fn == NULL) flt_spellcheck_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_spellcheck_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"SpellCheckerEnabled") == 0) {
    flt_spellcheck_enabled = cf_strcmp(args[0],"yes") == 0;
  }
  else if(cf_strcmp(opt->name,"SpellCheckerLanguage") == 0) {
    if(flt_spellcheck_language) free(flt_spellcheck_language);
    flt_spellcheck_language = strdup(args[0]);
  }
  else flt_spellcheck_activated_dflt = cf_strcmp(args[0],"yes") == 0;

  return 0;
}
/* }}} */

conf_opt_t flt_spellcheck_config[] = {
  { "SpellCheckerEnabled",          flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "SpellCheckerLanguage",         flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_NEEDED|CFG_OPT_LOCAL, NULL },
  { "SpellCheckerDefaultActivated", flt_spellcheck_cmd, CFG_OPT_CONFIG|CFG_OPT_LOCAL|CFG_OPT_USER,   NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_spellcheck_handlers[] = {
  { NEW_POST_HANDLER,     flt_spellcheck_execute },
  { POSTING_HANDLER,      flt_spellcheck_variables },
  { POST_DISPLAY_HANDLER, flt_spellcheck_variables_posting },
  { 0, NULL }
};

module_config_t flt_spellcheck = {
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
