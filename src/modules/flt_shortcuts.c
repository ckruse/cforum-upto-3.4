/**
 * \file flt_shortcuts.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * This plugin implements basic user features
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
#include <ctype.h>
#include <time.h>
#include <sys/types.h>

#include "readline.h"
#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "clientlib.h"
/* }}} */

#define FLT_SHORTCUTS_MOD_NONE  0x0
#define FLT_SHORTCUTS_MOD_SHIFT 0x1
#define FLT_SHORTCUTS_MOD_ALT   0x2
#define FLT_SHORTCUTS_MOD_CTL   0x4

typedef struct {
  u_char shortcut;
  int modifiers;
  u_char *fname;
} flt_shortcuts_t;

static int flt_shortcuts_activate = 0;

#define FLT_SHORTCUTS_THREADLISTTABLE_LENGTH 8
static flt_shortcuts_t flt_shortcuts_threadlisttable[] = {
  { 'b', FLT_SHORTCUTS_MOD_NONE,  "add_to_blacklist" }, /* add to blacklist */
  { 'b', FLT_SHORTCUTS_MOD_SHIFT, "remove_from_blacklist" }, /* remove from blacklist */
  { 'w', FLT_SHORTCUTS_MOD_NONE,  "add_to_whitelist" }, /* add to whitelist */
  { 'w', FLT_SHORTCUTS_MOD_SHIFT, "remove_from_whitelist" }, /* remove from whitelist */
  { 'k', FLT_SHORTCUTS_MOD_NONE,  "add_to_highlightcats"  }, /* add to highlightcats */
  { 'k', FLT_SHORTCUTS_MOD_SHIFT, "remove_from_highlightcats" },  /* remove from highlightcats */
  { 'a', FLT_SHORTCUTS_MOD_NONE,  "mark_all_visited" }, /* mark all postings visited */
  { 'l', FLT_SHORTCUTS_MOD_NONE,  "wikipedia_lookup" }
};

#define FLT_SHORTCUTS_POSTINGTABLE_LENGTH 10
static flt_shortcuts_t flt_shortcuts_postingtable[] = {
  { 'n', FLT_SHORTCUTS_MOD_NONE,  "next_posting" }, /* next posting */
  { 'p', FLT_SHORTCUTS_MOD_NONE,  "prev_posting" }, /* previous posting */
  { 'h', FLT_SHORTCUTS_MOD_NONE,  "back_to_threadlist" }, /* back to threadlist */
  { 'r', FLT_SHORTCUTS_MOD_NONE,  "focus_reply" }, /* scroll to textarea and focus */
  { 'g', FLT_SHORTCUTS_MOD_NONE,  "vote_good" }, /* vote good */
  { 'b', FLT_SHORTCUTS_MOD_NONE,  "vote_bad" },  /* vote bad */
  { 'a', FLT_SHORTCUTS_MOD_NONE,  "focus_active" }, /* mark active posting */
  { 'k', FLT_SHORTCUTS_MOD_NONE,  "kill_post" }, /* mark thread as deleted */
  { 'r', FLT_SHORTCUTS_MOD_SHIFT, "mark_visited" }, /* mark thread visited */
  { 'l', FLT_SHORTCUTS_MOD_NONE,  "wikipedia_lookup" }
};

static u_char *flt_shortcuts_fn = NULL;

/* {{{ flt_shortcuts_sc_to_js */
void flt_shortcuts_sc_to_js(string_t *js,flt_shortcuts_t *cut) {
  int first = 0;

  str_chars_append(js,"register_keybinding(flt_shortcuts_keystable,'",45);
  str_char_append(js,cut->shortcut);
  str_chars_append(js,"',",2);

  if(cut->modifiers & FLT_SHORTCUTS_MOD_SHIFT) {
    first = 1;
    str_chars_append(js,"MODIFIER_SHIFT",14);
  }

  if(cut->modifiers & FLT_SHORTCUTS_MOD_ALT) {
    if(first == 1) str_char_append(js,'|');
    first = 1;
    str_chars_append(js,"MODIFIER_ALT",12);
  }

  if(cut->modifiers & FLT_SHORTCUTS_MOD_CTL) {
    if(first == 1) str_char_append(js,'|');
    first = 1;
    str_chars_append(js,"MODIFIER_CTL",12);
  }

  if(first == 0) str_char_append(js,'0');

  str_char_append(js,',');
  str_cstr_append(js,cut->fname);
  str_chars_append(js,");\n",3);
}
/* }}} */

/* {{{ flt_shortcuts_threadlist */
int flt_shortcuts_threadlist(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cf_template_t *begin,cf_template_t *end) {
  int i = 0;
  string_t jscode;

  if(flt_shortcuts_activate == 0) return FLT_DECLINE;

  str_init(&jscode);
  str_char_set(&jscode,"flt_shortcuts_keystable = new Array();\n",39);

  /* generate keybindings table */
  for(i=0;i<FLT_SHORTCUTS_THREADLISTTABLE_LENGTH;++i) {
    if(flt_shortcuts_threadlisttable[i].shortcut) {
      flt_shortcuts_sc_to_js(&jscode,&flt_shortcuts_threadlisttable[i]);
    }
  }

  if(jscode.len) cf_tpl_setvalue(begin,"shortcuts",TPL_VARIABLE_STRING,jscode.content,jscode.len);
  str_cleanup(&jscode);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_shortcuts_posting */
int flt_shortcuts_posting(cf_hash_t *head,configuration_t *dc,configuration_t *vc,cl_thread_t *thr,cf_template_t *tpl) {
  int i = 0;
  string_t jscode;

  if(flt_shortcuts_activate == 0) return FLT_DECLINE;

  str_init(&jscode);
  str_char_set(&jscode,"flt_shortcuts_keystable = new Array();\n",39);

  /* generate keybindings table */
  for(i=0;i<FLT_SHORTCUTS_POSTINGTABLE_LENGTH;++i) {
    if(flt_shortcuts_postingtable[i].shortcut) {
      flt_shortcuts_sc_to_js(&jscode,&flt_shortcuts_postingtable[i]);
    }
  }

  if(jscode.len) cf_tpl_setvalue(tpl,"shortcuts",TPL_VARIABLE_STRING,jscode.content,jscode.len);
  str_cleanup(&jscode);

  return FLT_DECLINE;
}
/* }}} */

/* {{{ flt_shortcuts_parser */
void flt_shortcuts_parser(const u_char *text,flt_shortcuts_t *shortcut) {
  register u_char *ptr;

  shortcut->modifiers = 0;
  shortcut->shortcut  = 0;

  for(ptr=(u_char *)text;*ptr;++ptr) {
    switch(*ptr) {
      case '\\':
        switch(*(ptr+1)) {
          case 'c':
          case 'C':
            shortcut->modifiers |= FLT_SHORTCUTS_MOD_CTL;
            break;
          case 'a':
          case 'A':
            shortcut->modifiers |= FLT_SHORTCUTS_MOD_ALT;
            break;
          case 's':
          case 'S':
            shortcut->modifiers |= FLT_SHORTCUTS_MOD_SHIFT;
            break;
        }

        ++ptr;
        break;

      case '-':
        continue;

      default:
        shortcut->shortcut = *ptr;
        return;
    }
  }

}
/* }}} */

/* {{{ flt_shortcuts_handle */
int flt_shortcuts_handle(configfile_t *cfile,conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  size_t i;

  if(flt_shortcuts_fn == NULL) flt_shortcuts_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_shortcuts_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"ShortcutsActivate") == 0) flt_shortcuts_activate = cf_strcmp(args[0],"yes") == 0;
  else if(cf_strcmp(opt->name,"ShortcutsThreadlist") == 0) {
    for(i=0;i<argnum;++i) {
      if(args[i] && *args[i]) {
        flt_shortcuts_parser(args[i],&flt_shortcuts_threadlisttable[i]);
      }
      else {
        flt_shortcuts_threadlisttable[i].shortcut  = 0;
        flt_shortcuts_threadlisttable[i].modifiers = 0;
      }
    }
  }
  else if(cf_strcmp(opt->name,"ShortcutsPosting") == 0) {
    for(i=0;i<argnum;++i) {
      if(args[i] && *args[i]) {
        flt_shortcuts_parser(args[i],&flt_shortcuts_postingtable[i]);
      }
      else {
        flt_shortcuts_postingtable[i].shortcut  = 0;
        flt_shortcuts_postingtable[i].modifiers = 0;
      }
    }
  }

  return 0;
}
/* }}} */

conf_opt_t flt_shortcuts_config[] = {
  { "ShortcutsActivate",    flt_shortcuts_handle, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "ShortcutsThreadlist",  flt_shortcuts_handle, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { "ShortcutsPosting",     flt_shortcuts_handle, CFG_OPT_USER|CFG_OPT_CONFIG|CFG_OPT_LOCAL, NULL },
  { NULL, NULL, 0, NULL }
};

handler_config_t flt_shortcuts_handlers[] = {
  { VIEW_INIT_HANDLER, flt_shortcuts_threadlist },
  { POSTING_HANDLER,   flt_shortcuts_posting },
  { 0, NULL }
};

module_config_t flt_shortcuts = {
  MODULE_MAGIC_COOKIE,
  flt_shortcuts_config,
  flt_shortcuts_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
