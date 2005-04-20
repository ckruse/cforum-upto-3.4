/*
 * parsetpl.lex
 * The temaplte parser
 */
%{
#include "config.h"
#include "defines.h"

#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

typedef struct s_token {
  int type;
  t_string *data;
} t_token;

#define YY_DECL int parsetpl_lex(void)

/*
 * token constants
 */
#define PARSETPL_TOK_EOF                 0x00
#define PARSETPL_TOK_TAGSTART            0x01
#define PARSETPL_TOK_TAGEND              0x02
#define PARSETPL_TOK_WHITESPACE          0x03
#define PARSETPL_TOK_VARIABLE            0x04
#define PARSETPL_TOK_ASSIGNMENT          0x05
#define PARSETPL_TOK_STRING              0x06
#define PARSETPL_TOK_INTEGER             0x07
#define PARSETPL_TOK_ARRAYSTART          0x08
#define PARSETPL_TOK_ARRAYSEP            0x09
#define PARSETPL_TOK_ARRAYEND            0x10
#define PARSETPL_TOK_IF                  0x11
#define PARSETPL_TOK_COMPARE             0x12
#define PARSETPL_TOK_NOT                 0x13
#define PARSETPL_TOK_ELSIF               0x14
#define PARSETPL_TOK_ELSE                0x15
#define PARSETPL_TOK_ENDIF               0x16
#define PARSETPL_TOK_INCLUDE             0x17
#define PARSETPL_TOK_FOREACH             0x18
#define PARSETPL_TOK_AS                  0x19
#define PARSETPL_TOK_ENDFOREACH          0x20
#define PARSETPL_TOK_MODIFIER_ESCAPE     0x21
#define PARSETPL_TOK_LOOPVAR             0x22
#define PARSETPL_TOK_HASHASSIGNMENT      0x23
#define PARSETPL_TOK_CONCAT              0x24

#define PARSETPL_INCLUDE_EXT     ".html"

#define PARSETPL_ERR -1
#define PARSETPL_ERR_FILENOTFOUND          -1
#define PARSETPL_ERR_UNRECOGNIZEDCHARACTER -2
#define PARSETPL_ERR_UNTERMINATEDSTRING    -3 
#define PARSETPL_ERR_UNTERMINATEDTAG       -4 
#define PARSETPL_ERR_INVALIDTAG            -5
#define PARSETPL_ERR_NOTINLOOP             -6

static long lineno                   = 0;
static t_string  string              = { 0, 0, CF_BUFSIZ, NULL };
static t_string  content             = { 0, 0, CF_BUFSIZ, NULL };
static t_string  content_backup      = { 0, 0, CF_BUFSIZ, NULL };
static t_string  output              = { 0, 0, CF_BUFSIZ, NULL };
static t_string  output_mem          = { 0, 0, CF_BUFSIZ, NULL };
static t_string  current_file        = { 0, 0, CF_BUFSIZ, NULL };
static t_array   foreach_var_stack;
static t_array   if_level_stack;
static long n_assign_vars            = 0;
static long n_cur_assign_vars        = 0;
static long n_foreach_vars           = 0;
static long n_cur_foreach_vars       = 0;
static long n_if_vars                = 0;
static long n_cur_if_vars            = 0;
static long n_if_iters               = 0;
static long n_cur_if_iters           = 0;
static int  uses_include             = 0;
static int  uses_print               = 0;
static int  uses_iter_print          = 0;
static int  uses_clonevar            = 0;
static int  uses_loopassign          = 0;
static int  uses_tmpstring           = 0;

/*

t_cf_tpl_variable  *v:            all-purpose variable pointer
t_cf_tpl-variable  *va0;          variables dynamically generated for assignment
t_cf_tpl-variable  *vf0;          variables dynamically generated for foreach
t_cf_tpl-variable  *vi0;          variables dynamically generated for if

*/

%}

/* states */
%x TAG STRING

%option stack noyywrap

%%

\n   {
  ++lineno; /* count line numbers */
  str_char_append(&content,'\n');
}

\{      {
  yy_push_state(TAG);
  if(content_backup.content) free(content_backup.content);
  str_init(&content_backup);
  str_chars_append(&content_backup,yytext,yyleng);
  return PARSETPL_TOK_TAGSTART;
}

<TAG>{
  \n                  {
    str_chars_append(&content_backup,yytext,yyleng);
    ++lineno;
    return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
  }
  [\t\r ]             {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_WHITESPACE;
  }
  \"             {
    str_chars_append(&content_backup,yytext,yyleng);
    if(string.content) free(string.content);
    str_init(&string);
    yy_push_state(STRING);
  }
  -?[0-9]+            {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_INTEGER;
  }
  \$[A-Za-z0-9_]+     {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_VARIABLE;
  }
  @[A-Za-z0-9_]+     {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_LOOPVAR;
  }
  \}             {
    str_chars_append(&content_backup,yytext,yyleng);
    yy_pop_state();
    if(content.content) free(content.content);
    str_init(&content);
    return PARSETPL_TOK_TAGEND;
  }
  =>                {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_HASHASSIGNMENT;
  }
  \+                {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_CONCAT;
  }
  ->html            {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_MODIFIER_ESCAPE;
  }
  ==                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_COMPARE;
  }
  !=                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_COMPARE;
  }
  =                   {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ASSIGNMENT;
  }
  \[                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYSTART;
  }
  ,                   {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYSEP;
  }
  \]                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYEND;
  }
  !                   {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NOT;
  }
  else?[ ]?if         {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ELSIF;
  }
  if                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IF;
  }
  else                {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ELSE;
  }
  endif               {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ENDIF;
  }
  include             {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_INCLUDE;
  }
  foreach             {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FOREACH;
  }
  as                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_AS;
  }
  endforeach          {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ENDFOREACH;
  }
  <<EOF>>        {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_ERR_UNTERMINATEDTAG;
  }
  .                   {
    //str_chars_append(&content_backup,yytext,yyleng);
    unput(*yytext);
    return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
  }
}

<STRING>{
  \n                  {
    str_chars_append(&content_backup,yytext,yyleng);
    ++lineno; 
    return PARSETPL_ERR_UNTERMINATEDSTRING;
  }
  \\n                 {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,'\n');
  }
  \\t                 {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,'\t');
  }
  \\r                 {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,'\r');
  }
  \\0[0-7]{1,3}       {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,(char)strtol(yytext,NULL,8));
  }
  \\x[0-9A-Fa-f]{1,2} {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,(char)strtol(yytext,NULL,16));
  }
  \\\\                {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,'\\');
  }
  \\\"                {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,'"');
  }
  \"                  {
    str_chars_append(&content_backup,yytext,yyleng);
    yy_pop_state();
    return PARSETPL_TOK_STRING;
  }
  <<EOF>> {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_ERR_UNTERMINATEDSTRING;
  }
  .                   {
    str_chars_append(&content_backup,yytext,yyleng);
    str_char_append(&string,*yytext);
  }
}

. {
  str_char_append(&content,*yytext);
}

%%

void destroy_token(void *t) {
  t_token *token = (t_token *)t;
  str_cleanup(token->data);
}

void append_escaped_string(t_string *dest,t_string *src) {
  size_t i;
  for(i = 0; i < src->len; i++) {
    if(src->content[i] == '\n') {
      str_char_append(dest,'\\');
      str_char_append(dest,'n');
      continue;
    }
    if(src->content[i] == '\x0D') {
      str_chars_append(dest,"\\x0D",4);
      continue;
    }
    if(src->content[i] == '\\' || src->content[i] == '"') {
      str_char_append(dest,'\\');
    }
    str_char_append(dest,src->content[i]);
  }
}


int dereference_variable(t_string *out_str,t_token *var,t_array *data,t_string *c_var) {
  int level;
  t_token *token;
  t_string *arrayidx;
  int is_hash;
  
  str_cstr_append(out_str,"v = (t_cf_tpl_variable *)cf_tpl_getvar(tpl,\"");
  str_chars_append(out_str,var->data->content+1,var->data->len-1);
  str_cstr_append(out_str,"\");\n");
  token = NULL;
  level = 0;
  // get array indexes
  while(data->elements) {
    token = (t_token *)array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) {
      break;
    }
    token = (t_token *)array_shift(data);
    destroy_token(token); free(token);
    if(!data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token *)array_shift(data);
    if((token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER) || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(token->type == PARSETPL_TOK_INTEGER) {
      is_hash = 0;
    } else {
      is_hash = 1;
    }
    arrayidx = token->data; free(token);
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_ARRAYEND) {
      destroy_token(token); free(token);
      str_cleanup(arrayidx);
      free(arrayidx);
      return PARSETPL_ERR_INVALIDTAG;
    }
    destroy_token(token); free(token);
    if(is_hash) {
      str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_HASH) {\n");
      str_cstr_append(out_str,"v = (t_cf_tpl_variable *)cf_hash_get(v->data.d_hash,\"");
      append_escaped_string(out_str,arrayidx);
      str_cstr_append(out_str,"\",strlen(\"");
      append_escaped_string(out_str,arrayidx);
      str_cstr_append(out_str,"\")");
    } else {
      str_cstr_append(out_str,"if(v && v->type == TPL_VARIABLE_ARRAY) {\n");
      str_cstr_append(out_str,"v = (t_cf_tpl_variable *)array_element_at(&v->data.d_array,");
      str_str_append(out_str,arrayidx);
    }
    str_cleanup(arrayidx);
    free(arrayidx);
    str_cstr_append(out_str,");\n");
    level++;
  }
  str_str_append(out_str,c_var);
  str_cstr_append(out_str," = v;\n");
  for(;level > 0;level--) {
    str_cstr_append(out_str,"}\n");
  }
  return 0;
}

int dereference_iterator(t_string *out_str,t_token *var,t_array *data,t_string *c_var) {
  long idx = -1;
  t_token *token;
  char idxbuf[20];
  char idx2buf[20];
  
  if(strcmp(var->data->content,"@first") && strcmp(var->data->content,"@last") && strcmp(var->data->content,"@iterator")) {
    return PARSETPL_ERR_INVALIDTAG;
  }
  // get array indexes
  while(data->elements) {
    token = (t_token *)array_element_at(data,0);
    if(token->type != PARSETPL_TOK_ARRAYSTART) {
      break;
    }
    token = (t_token *)array_shift(data);
    destroy_token(token); free(token);
    if(!data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_INTEGER || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    idx = strtol(token->data->content,NULL,10); free(token);
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_ARRAYEND) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    destroy_token(token); free(token);
    // only one loop pass!
    break;
  }
  
  if(!foreach_var_stack.elements) {
    return PARSETPL_ERR_NOTINLOOP;
  }
  if(idx >= 0 && foreach_var_stack.elements <= idx) {
    return PARSETPL_ERR_NOTINLOOP;
  }
  if(idx < 0 && foreach_var_stack.elements < -idx) {
    return PARSETPL_ERR_NOTINLOOP;
  }
  if(idx < 0) {
    idx += foreach_var_stack.elements;
  }
  snprintf(idxbuf,19,"%ld",idx);
  snprintf(idx2buf,19,"%ld",idx*2);

  if(!strcmp(var->data->content,"@first")) {
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = (i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str," == 0) ? 1 : 0;\n");
  } else if(!strcmp(var->data->content,"@last")) {
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = (i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str," == vf");
    str_cstr_append(out_str,idx2buf);
    str_cstr_append(out_str,"->data.d_array.elements - 1) ? 1 : 0;\n");
  } else if(!strcmp(var->data->content,"@iterator")) {
    str_str_append(out_str,c_var);
    str_cstr_append(out_str," = i");
    str_cstr_append(out_str,idxbuf);
    str_cstr_append(out_str,";\n");
  }
  return 0;
}

int peek_next_nws_type(t_array *data) {
  int i;
  int t = -1;
  t_token *tok;
  
  for (i = 0; i < data->elements; i++) {
    tok = (t_token *)array_element_at(data,i);
    if(tok->type == PARSETPL_TOK_WHITESPACE) {
      continue;
    }
    t = tok->type;
    break;
  }
  return t;
}

// returns: 0 - no hash, 1 - hash, -1 invalid tag
int peek_for_hash(t_array *data) {
  int i;
  t_token *tok;
  int had_str = 0;
  
  for (i = 0; i < data->elements; i++) {
    tok = (t_token *)array_element_at(data,i);
    if(tok->type == PARSETPL_TOK_WHITESPACE) {
      continue;
    }
    if(tok->type == PARSETPL_TOK_STRING) {
      if(had_str) {
        return -1;
      }
      had_str = 1;
      continue;
    }
    if(tok->type == PARSETPL_TOK_HASHASSIGNMENT) {
      if(had_str) {
        return 1;
      }
      return -1;
    }
    // no whitespace, no string, no hash assignment => no hash
    break;
  }
  return 0;
}

int process_array_assignment(t_array *data,t_string *tmp) {
  t_token *token;
  t_string varn;
  long v1n, v2n;
  int had_sep, ret;
  char buf[20];
  char v1nb[20],v2nb[20];
  int n_elems, is_hash, is_hval, nt, is_concat, jh_concat;
  t_string *hkey;
  
  v1n = n_cur_assign_vars++;
  snprintf(v1nb,19,"%ld",v1n);
  v2n = n_cur_assign_vars;
  if(n_cur_assign_vars > n_assign_vars) {
    n_assign_vars = n_cur_assign_vars;
  }
  snprintf(v2nb,19,"%ld",v2n);
  
  is_hash = peek_for_hash(data);
  if(is_hash == -1) { // invalid tag
    --n_cur_assign_vars;
    return PARSETPL_ERR_INVALIDTAG;
  }
  
  str_cstr_append(tmp,"va");
  str_cstr_append(tmp,v1nb);
  str_cstr_append(tmp," = fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);\n");
  str_cstr_append(tmp,"cf_tpl_var_init(va");
  str_cstr_append(tmp,v1nb);
  if(is_hash) {
    str_cstr_append(tmp,",TPL_VARIABLE_HASH);\n");
  } else {
    str_cstr_append(tmp,",TPL_VARIABLE_ARRAY);\n");
  }
  had_sep = 1;
  n_elems = 0;
  is_hval = 0;
  is_concat = 0;
  jh_concat = 0;
  
  while(data->elements) {
    token = (t_token *)array_shift(data);
    if(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      continue;
    }
    if(had_sep && token->type != PARSETPL_TOK_ARRAYSTART && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_VARIABLE && token->type != PARSETPL_TOK_LOOPVAR) {
      destroy_token(token); free(token);
      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
      }
      --n_cur_assign_vars;
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!jh_concat && !had_sep && !is_hval && token->type != PARSETPL_TOK_ARRAYEND && token->type != PARSETPL_TOK_ARRAYSEP) {
      destroy_token(token); free(token);
      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
      }
      --n_cur_assign_vars;
      return PARSETPL_ERR_INVALIDTAG;
    }
    jh_concat = 0;
    if(token->type == PARSETPL_TOK_ARRAYEND) {
      destroy_token(token); free(token);
      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
        --n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      break;
    }
    if(token->type == PARSETPL_TOK_ARRAYSEP) {
      destroy_token(token); free(token);
      if(is_hval) {
        str_cleanup(hkey);
        free(hkey);
        --n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      had_sep = 1;
      continue;
    }
    had_sep = 0;
    if(token->type == PARSETPL_TOK_STRING) {
      // peek to see if this is a concatentation or a hash assignment
      nt = peek_next_nws_type(data);
      if(nt == PARSETPL_TOK_CONCAT || is_concat) {
        // ok, we need to concatenate a string
        uses_tmpstring = 1;
        if(!is_concat) {
          str_cstr_append(tmp,"str_init(&tmp_string);");
          is_concat = 1;
        }
        str_cstr_append(tmp,"str_chars_append(&tmp_string,\"");
        append_escaped_string(tmp,token->data);
        str_cstr_append(tmp,"\",");
        snprintf(buf,19,"%ld",token->data->len);
        str_cstr_append(tmp,buf);
        str_cstr_append(tmp,");\n");
        if(nt == PARSETPL_TOK_CONCAT) {
          // we already know that there will be a token of this type
          while(token->type != PARSETPL_TOK_CONCAT) {
            destroy_token(token); free(token);
            token = (t_token *)array_shift(data);
          }
          jh_concat = 1;
        } else {
          // end of concatenations
          if(is_hash && !is_hval) {
            destroy_token(token); free(token);
            --n_cur_assign_vars;
            return PARSETPL_ERR_INVALIDTAG;
          }
          if(is_hval) {
            str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",\"");
            append_escaped_string(tmp,hkey);
            str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            is_hval = 0;
            str_cleanup(hkey);
            free(hkey);
          } else {
            str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
          }
          str_cstr_append(tmp,"str_cleanup(&tmp_string);\n");
          is_concat = 0;
          jh_concat = 0;
        }
      }
      else if(nt == PARSETPL_TOK_HASHASSIGNMENT) {
        // if we already had elements and this is no hash
        if(n_elems && !is_hash) {
          destroy_token(token); free(token);
          --n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }
        // if this is a concatenation (["bla". => ] or ["bla"."blub" => ])
        if(is_concat) {
          destroy_token(token); free(token);
          --n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }
        is_hash = 1;
        is_hval = 1;
        hkey = token->data;
        free(token);
        token = (t_token *)array_shift(data);
        // we already know that there will be a token of this type
        while(token->type != PARSETPL_TOK_HASHASSIGNMENT) {
          destroy_token(token); free(token);
          token = (t_token *)array_shift(data);
        }
      }
      else {
        if(is_hash && !is_hval) {
          destroy_token(token); free(token);
          --n_cur_assign_vars;
          return PARSETPL_ERR_INVALIDTAG;
        }
        if(is_hval) {
          str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
          str_cstr_append(tmp,v1nb);
          str_cstr_append(tmp,",\"");
          append_escaped_string(tmp,hkey);
          str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,\"");
          append_escaped_string(tmp,token->data);
          str_cstr_append(tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(tmp,buf);
          str_cstr_append(tmp,");\n");
          is_hval = 0;
          str_cleanup(hkey);
          free(hkey);
        } else {
          str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
          str_cstr_append(tmp,v1nb);
          str_cstr_append(tmp,",TPL_VARIABLE_STRING,\"");
          append_escaped_string(tmp,token->data);
          str_cstr_append(tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(tmp,buf);
          str_cstr_append(tmp,");\n");
        }
      }
    } else if(token->type == PARSETPL_TOK_INTEGER) {
      if(is_concat) {
        destroy_token(token); free(token);
        if(is_hval) {
          str_cleanup(hkey);
          free(hkey);
        }
        --n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",TPL_VARIABLE_INT,");
        str_str_append(tmp,token->data);
        str_cstr_append(tmp,");\n");
      } else {
        str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",TPL_VARIABLE_INT,");
        str_str_append(tmp,token->data);
        str_cstr_append(tmp,");\n");
      }
    } else if(token->type == PARSETPL_TOK_ARRAYSTART) {
      if(is_concat) {
        destroy_token(token); free(token);
        if(is_hval) {
          str_cleanup(hkey);
          free(hkey);
        }
        --n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      ret = process_array_assignment(data,tmp);
      if(ret < 0) {
        destroy_token(token); free(token);
        --n_cur_assign_vars;
        return ret;
      }
      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_set(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",va");
        str_cstr_append(tmp,v2nb);
        str_cstr_append(tmp,");\n");
        is_hval = 0;
        str_cleanup(hkey);
        free(hkey);
      } else {
        str_cstr_append(tmp,"cf_tpl_var_add(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",va");
        str_cstr_append(tmp,v2nb);
        str_cstr_append(tmp,");\n");
      }
      str_cstr_append(tmp,"free(va");
      str_cstr_append(tmp,v2nb);
      str_cstr_append(tmp,");\n");
    } else if(token->type == PARSETPL_TOK_VARIABLE) {
      uses_clonevar = 1;
      str_cstr_append(tmp,"vc = NULL;\n");
      str_init(&varn);
      str_cstr_append(&varn,"vc");
      ret = dereference_variable(tmp,token,data,&varn);
      str_cleanup(&varn);
      if(ret < 0) {
        --n_cur_assign_vars;
        return ret;
      }
      // peek to see if this is a concatentation or a hash assignment
      nt = peek_next_nws_type(data);
      if(nt == PARSETPL_TOK_CONCAT || is_concat) {
        // ok, we need to concatenate a string
        uses_tmpstring = 1;
        if(!is_concat) {
          str_cstr_append(tmp,"str_init(&tmp_string);");
          is_concat = 1;
        }
        str_cstr_append(tmp,"vc = cf_tpl_var_convert(NULL,vc,TPL_VARIABLE_STRING);\n");
        str_cstr_append(tmp,"if(vc) {\n");
        str_cstr_append(tmp,"str_str_append(&tmp_string,&vc->data.d_string);\n");
        str_cstr_append(tmp,"cf_tpl_var_destroy(vc);\n");
        str_cstr_append(tmp,"free(vc);\n");
        str_cstr_append(tmp,"}\n");
        if(nt == PARSETPL_TOK_CONCAT) {
          // we already know that there will be a token of this type
          while(token->type != PARSETPL_TOK_CONCAT) {
            destroy_token(token); free(token);
            token = (t_token *)array_shift(data);
          }
          jh_concat = 1;
        } else {
          // end of concatenations
          if(is_hash && !is_hval) {
            destroy_token(token); free(token);
            --n_cur_assign_vars;
            return PARSETPL_ERR_INVALIDTAG;
          }
          if(is_hval) {
            str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",\"");
            append_escaped_string(tmp,hkey);
            str_cstr_append(tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            is_hval = 0;
            str_cleanup(hkey);
            free(hkey);
          } else {
            str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
            str_cstr_append(tmp,v1nb);
            str_cstr_append(tmp,",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
          }
          str_cstr_append(tmp,"str_cleanup(&tmp_string);\n");
          is_concat = 0;
          jh_concat = 0;
        }
      }
      else if(nt == PARSETPL_TOK_HASHASSIGNMENT) {
        // invalid
        destroy_token(token); free(token);
        --n_cur_assign_vars;
        return PARSETPL_ERR_INVALIDTAG;
      }
      else {
        str_cstr_append(tmp,"vc = cf_tpl_var_clone(vc);\n");
        str_cstr_append(tmp,"if(!vc) {\n");
        str_cstr_append(tmp,"vc = (t_cf_tpl_variable *)fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);\n");
        str_cstr_append(tmp,"cf_tpl_var_init(vc,TPL_VARIABLE_INVALID);\n");
        str_cstr_append(tmp,"}\n");
        str_cstr_append(tmp,"cf_tpl_var_add(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",vc);\n");
        str_cstr_append(tmp,"free(vc);\n");
      }
    } else if(token->type == PARSETPL_TOK_LOOPVAR) {
      uses_loopassign = 1;
      str_cstr_append(tmp,"ic = 0;\n");
      str_init(&varn);
      str_cstr_append(&varn,"ic");
      ret = dereference_iterator(tmp,token,data,&varn);
      str_cleanup(&varn);
      if(ret < 0) {
        --n_cur_assign_vars;
        return ret;
      }
      if(is_hval) {
        str_cstr_append(tmp,"cf_tpl_hashvar_setvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",\"");
        append_escaped_string(tmp,hkey);
        str_cstr_append(tmp,"\",TPL_VARIABLE_INT,ic);\n");
        is_hval = 0;
        str_cleanup(hkey);
        free(hkey);
      } else {
        str_cstr_append(tmp,"cf_tpl_var_addvalue(va");
        str_cstr_append(tmp,v1nb);
        str_cstr_append(tmp,",TPL_VARIABLE_INT,ic);\n");
      }
    }
    destroy_token(token); free(token);
    n_elems++;
  }
  --n_cur_assign_vars;
  return 0;
}

int process_variable_assignment_tag(t_token *variable,t_array *data) {
  t_string tmp;
  t_token *token = NULL;
  t_string varn;
  char buf[20];
  int ret,nt,is_concat = 0,n = 0;
  
  str_init(&tmp);
  
  // remove all whitespaces
  while(data->elements) {
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE) {
      break;
    }
  }
  do {
    if(token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_ARRAYSTART && token->type != PARSETPL_TOK_VARIABLE && token->type != PARSETPL_TOK_LOOPVAR) {
      str_cleanup(&tmp);
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(n && !is_concat) {
      str_cleanup(&tmp);
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    switch(token->type) {
      case PARSETPL_TOK_STRING:
        // peek to see if this is a concatentation or a hash assignment
        nt = peek_next_nws_type(data);
        if(nt == PARSETPL_TOK_CONCAT || is_concat) {
          // ok, we need to concatenate a string
          uses_tmpstring = 1;
          if(!is_concat) {
            str_cstr_append(&tmp,"str_init(&tmp_string);");
            is_concat = 1;
          }
          str_cstr_append(&tmp,"str_chars_append(&tmp_string,\"");
          append_escaped_string(&tmp,token->data);
          str_cstr_append(&tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(&tmp,buf);
          str_cstr_append(&tmp,");\n");
          if(nt == PARSETPL_TOK_CONCAT) {
            // we already know that there will be a token of this type
            while(token->type != PARSETPL_TOK_CONCAT) {
              destroy_token(token); free(token);
              token = (t_token *)array_shift(data);
            }
            while(data->elements && ((t_token *)array_element_at(data,0))->type == PARSETPL_TOK_WHITESPACE) {
              destroy_token(token); free(token);
              token = (t_token *)array_shift(data);
            }
          } else {
            str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
            str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
            str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            str_cstr_append(&tmp,"str_cleanup(&tmp_string);\n");
            is_concat = 0;
          }
        } else {
          str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
          str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
          str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,\"");
          append_escaped_string(&tmp,token->data);
          str_cstr_append(&tmp,"\",");
          snprintf(buf,19,"%ld",token->data->len);
          str_cstr_append(&tmp,buf);
          str_cstr_append(&tmp,");\n");
        }
        break;
      case PARSETPL_TOK_INTEGER:
        str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",TPL_VARIABLE_INT,");
        str_str_append(&tmp,token->data);
        str_cstr_append(&tmp,");\n");
        break;
      case PARSETPL_TOK_VARIABLE:
        uses_clonevar = 1;
        str_cstr_append(&tmp,"vc = NULL;\n");
        str_init(&varn);
        str_cstr_append(&varn,"vc");
        ret = dereference_variable(&tmp,token,data,&varn);
        str_cleanup(&varn);
        if(ret < 0) {
          str_cleanup(&tmp);
          return ret;
        }
        // peek to see if this is a concatentation or a hash assignment
        nt = peek_next_nws_type(data);
        if(nt == PARSETPL_TOK_CONCAT || is_concat) {
          // ok, we need to concatenate a string
          uses_tmpstring = 1;
          if(!is_concat) {
            str_cstr_append(&tmp,"str_init(&tmp_string);");
            is_concat = 1;
          }
          str_cstr_append(&tmp,"vc = cf_tpl_var_convert(NULL,vc,TPL_VARIABLE_STRING);\n");
          str_cstr_append(&tmp,"if(vc) {\n");
          str_cstr_append(&tmp,"str_str_append(&tmp_string,&vc->data.d_string);\n");
          str_cstr_append(&tmp,"cf_tpl_var_destroy(vc);\n");
          str_cstr_append(&tmp,"free(vc);\n");
          str_cstr_append(&tmp,"}\n");
          if(nt == PARSETPL_TOK_CONCAT) {
            // we already know that there will be a token of this type
            while(token->type != PARSETPL_TOK_CONCAT) {
              destroy_token(token); free(token);
              token = (t_token *)array_shift(data);
            }
            while(data->elements && ((t_token *)array_element_at(data,0))->type == PARSETPL_TOK_WHITESPACE) {
              destroy_token(token); free(token);
              token = (t_token *)array_shift(data);
            }
          } else {
            str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
            str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
            str_cstr_append(&tmp,"\",TPL_VARIABLE_STRING,tmp_string.content,tmp_string.len);\n");
            str_cstr_append(&tmp,"str_cleanup(&tmp_string);\n");
            is_concat = 0;
          }
        } else {
          str_cstr_append(&tmp,"vc = cf_tpl_var_clone(vc);\n");
          str_cstr_append(&tmp,"if(!vc) {\n");
          str_cstr_append(&tmp,"vc = (t_cf_tpl_variable *)fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);\n");
          str_cstr_append(&tmp,"cf_tpl_var_init(vc,TPL_VARIABLE_INVALID);\n");
          str_cstr_append(&tmp,"}\n");
          str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
          str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
          str_cstr_append(&tmp,"\",vc);\n");
          str_cstr_append(&tmp,"free(vc);\n");
        }
        break;
      case PARSETPL_TOK_LOOPVAR:
        uses_loopassign = 1;
        str_cstr_append(&tmp,"ic = 0;\n");
        str_init(&varn);
        str_cstr_append(&varn,"ic");
        ret = dereference_iterator(&tmp,token,data,&varn);
        str_cleanup(&varn);
        if(ret < 0) {
          destroy_token(token); free(token);
          str_cleanup(&tmp);
          return ret;
        }
        str_cstr_append(&tmp,"cf_tpl_setvalue(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",TPL_VARIABLE_INT,ic);\n");
        break;
      case PARSETPL_TOK_ARRAYSTART:
        ret = process_array_assignment(data,&tmp);
        if(ret < 0) {
          destroy_token(token); free(token);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }
        if(data->elements) {
          destroy_token(token); free(token);
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
        }
        str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
        str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
        str_cstr_append(&tmp,"\",va0);\n");
        str_cstr_append(&tmp,"free(va0);\n");
        break;
    }
    destroy_token(token); free(token);
    if(data->elements) {
      token = (t_token *)array_shift(data);
    } else {
      token = NULL;
    }
    n++;
  } while(token);
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  return 0;
}

int process_variable_print_tag (t_token *variable,t_array *data) {
  t_string tmp;
  t_string c_var;
  t_token *token;
  int escape_html = 0;
  int ret;
  
  str_init(&tmp);
  str_init(&c_var);
  str_char_set(&c_var,"vp",2);
  str_cstr_append(&tmp,"vp = NULL;\n");
  ret = dereference_variable(&tmp,variable,data,&c_var);
  str_cleanup(&c_var);
  if(ret < 0) {
    ret = 0;
  }
  if(data->elements) {
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_MODIFIER_ESCAPE || data->elements) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }
    destroy_token(token); free(token);
    escape_html = 1;
  }
  str_cstr_append(&tmp,"if(vp) {\n");
  str_cstr_append(&tmp,"if(vp->type != TPL_VARIABLE_STRING) {\n");
  str_cstr_append(&tmp,"vp = cf_tpl_var_convert(NULL,vp,TPL_VARIABLE_STRING);\n}\n");
  str_cstr_append(&tmp,"if(vp && vp->type == TPL_VARIABLE_STRING) {\n");
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  str_cleanup(&tmp);
  
  if(escape_html) {
    str_cstr_append(&output,"print_htmlentities_encoded(vp->data.d_string.content,0,stdout);\n}\n}\n");
    str_cstr_append(&output_mem,"tmp = htmlentities(vp->data.d_string.content,0);\nstr_chars_append(&tpl->parsed,tmp,strlen(tmp));\nfree(tmp);\n}\n}\n");
  } else {
    str_cstr_append(&output,"my_write(vp->data.d_string.content);\n}\n}\n");
    str_cstr_append(&output_mem,"str_chars_append(&tpl->parsed,vp->data.d_string.content,strlen(vp->data.d_string.content));\n}\n}\n");
  }
  str_cstr_append(&output,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n");
  str_cstr_append(&output_mem,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n");
  str_cstr_append(&output,"}\n");
  str_cstr_append(&output_mem,"}\n");
  uses_print = 1;
  return 0;
}

int process_iterator_print_tag (t_token *iterator,t_array *data) {
  t_string tmp;
  t_string c_var;
  int ret;
  
  str_init(&tmp);
  str_init(&c_var);
  str_char_set(&c_var,"iter_var",8);
  ret = dereference_iterator(&tmp,iterator,data,&c_var);
  str_cleanup(&c_var);
  if(ret < 0) {
    str_cleanup(&tmp);
    return ret;
  }
  if(data->elements) {
    str_cleanup(&tmp);
    return PARSETPL_ERR_INVALIDTAG;
  }
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  str_cleanup(&tmp);
  str_cstr_append(&output,"printf(\"%ld\",iter_var);\n");
  str_cstr_append(&output_mem,"snprintf(iter_buf,19,\"%ld\",iter_var);\n");
  str_cstr_append(&output_mem,"str_chars_append(&tpl->parsed,iter_buf,strlen(iter_buf));\n");
  uses_iter_print = 1;
  return 0;
}

int process_include_tag(t_string *file) {
  t_string tmp;
  char buf[20];
  if(!strcmp(PARSETPL_INCLUDE_EXT,file->content+file->len-strlen(PARSETPL_INCLUDE_EXT))) {
    file->content[file->len-strlen(PARSETPL_INCLUDE_EXT)] = '\0';
    file->len -= strlen(PARSETPL_INCLUDE_EXT);
  }
  str_init(&tmp);
  str_chars_append(&tmp,"inc_filename = fo_alloc(NULL,sizeof(t_string),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"inc_filepart = fo_alloc(NULL,sizeof(t_string),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"inc_fileext = fo_alloc(NULL,sizeof(t_string),1,FO_ALLOC_MALLOC);\n",65);
  str_chars_append(&tmp,"inc_tpl = fo_alloc(NULL,sizeof(t_cf_template),1,FO_ALLOC_MALLOC);\n",66);
  str_chars_append(&tmp,"str_init(inc_filename);\n",24);
  str_chars_append(&tmp,"str_init(inc_filepart);\n",24);
  str_chars_append(&tmp,"str_init(inc_fileext);\n",23);
  str_chars_append(&tmp,"str_char_set(inc_filename,tpl->filename,strlen(tpl->filename));\n",64);
  str_chars_append(&tmp,"str_char_set(inc_filepart,\"",27);
  append_escaped_string(&tmp,&current_file);
  str_chars_append(&tmp,"\",",2);
  snprintf(buf,19,"%ld",current_file.len);
  str_chars_append(&tmp,buf,strlen(buf));
  str_chars_append(&tmp,");\n",3);
  str_chars_append(&tmp,"p = inc_filename->content+inc_filename->len-1;\n",47);
  str_chars_append(&tmp,"while(strncmp(p,inc_filepart->content,inc_filepart->len) && p > inc_filename->content) p--;\n",92);
  str_chars_append(&tmp,"if(!strncmp(p,inc_filepart->content,inc_filepart->len)) {\n",58);
  str_chars_append(&tmp,"*p = '\\0'; inc_filename->len = p - inc_filename->content;\n",58);
  str_chars_append(&tmp,"str_char_set(inc_fileext,p+inc_filepart->len,strlen(p+inc_filepart->len));\n",75);
  str_chars_append(&tmp,"str_chars_append(inc_filename,\"",31);
  append_escaped_string(&tmp,file);
  str_chars_append(&tmp,"\",",2);
  snprintf(buf,19,"%ld",file->len);
  str_chars_append(&tmp,buf,strlen(buf));  
  str_chars_append(&tmp,");\n",3);
  str_chars_append(&tmp,"str_str_append(inc_filename,inc_fileext);\n",42);
  str_chars_append(&tmp,"ret = cf_tpl_init(inc_tpl,inc_filename->content);\n",50);
  str_chars_append(&tmp,"if(!ret) {\n",11);
  // evil, we copy the varlist - but i don't have a better idea that doesn't cost much code
  str_chars_append(&tmp,"ov = inc_tpl->varlist;\n",23);
  str_chars_append(&tmp,"inc_tpl->varlist = tpl->varlist;\n",33);
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  str_chars_append(&output,"cf_tpl_parse(inc_tpl);\n",23);
  str_chars_append(&output_mem,"cf_tpl_parse_to_mem(inc_tpl);\n",30);
  str_chars_append(&output_mem,"if(inc_tpl->parsed.len) str_str_append(&tpl->parsed,&inc_tpl->parsed);\n",71);
  str_cleanup(&tmp);
  str_init(&tmp);
  str_chars_append(&tmp,"inc_tpl->varlist = ov;\n",23);
  str_chars_append(&tmp,"}\n",2);
  str_chars_append(&tmp,"cf_tpl_finish(inc_tpl);\n",24);
  str_chars_append(&tmp,"}\n",2);
  str_chars_append(&tmp,"str_cleanup(inc_filepart);\n",27);
  str_chars_append(&tmp,"str_cleanup(inc_filename);\n",27);
  str_chars_append(&tmp,"str_cleanup(inc_fileext);\n",26);
  str_chars_append(&tmp,"free(inc_tpl);\n",15);
  str_chars_append(&tmp,"free(inc_filepart);\n",20);
  str_chars_append(&tmp,"free(inc_filename);\n",20);
  str_chars_append(&tmp,"free(inc_fileext);\n",19);
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  uses_include = 1;
  return 0;
}

int process_foreach_tag(t_array *data) {
  t_string tmp,vs,varn;
  t_token *token,*var2;
  t_string *tvs;
  long v1n, v2n, ivn;
  char v1nb[20],v2nb[20],ivnb[20];
  int ret;
  
  token = (t_token*)array_shift(data);
  if(token->type == PARSETPL_TOK_ENDFOREACH) {
    destroy_token(token); free(token);
    if(data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!foreach_var_stack.elements) { // impossible
      return PARSETPL_ERR_INVALIDTAG;
    }
    tvs = (t_string*)array_pop(&foreach_var_stack);
    ivn = --n_cur_foreach_vars;
    v1n = ivn*2;
    v2n = v1n + 1;
    snprintf(v1nb,19,"%ld",v1n);
    snprintf(v2nb,19,"%ld",v2n);
    snprintf(ivnb,19,"%ld",ivn);
    // make sure that the variable is invalid because the memory structure will be freed by cf_tpl_setvar
    // please note that getvar IS necessary because we can NOT use the old value of vf3 since that will
    // overwrite the *ORIGINAL* template variable (array member of vf2)
    str_init(&tmp);
    str_cstr_append(&tmp,"}\n}\n");
    str_str_append(&output,&tmp);
    str_str_append(&output_mem,&tmp);
    str_cleanup(&tmp);
    str_cleanup(tvs);
    free(tvs);
  } else if(token->type == PARSETPL_TOK_FOREACH) {
    destroy_token(token); free(token);
    if(!data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type != PARSETPL_TOK_VARIABLE || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    ivn = n_cur_foreach_vars++;
    v1n = ivn*2;
    v2n = v1n + 1;
    if(n_cur_foreach_vars > n_foreach_vars) {
      n_foreach_vars = n_cur_foreach_vars;
    }
    str_init(&tmp);
    snprintf(v1nb,19,"%ld",v1n);
    snprintf(v2nb,19,"%ld",v2n);
    snprintf(ivnb,19,"%ld",ivn);
    str_init(&varn);
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp," = NULL;\n");
    str_cstr_append(&varn,"vf");
    str_cstr_append(&varn,v1nb);
    ret = dereference_variable(&tmp,token,data,&varn);
    str_cleanup(&varn);
    destroy_token(token); free(token);
    if(ret < 0) {
      str_cleanup(&tmp);
      n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        str_cleanup(&tmp);
        n_cur_foreach_vars--;
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type != PARSETPL_TOK_AS || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    destroy_token(token); free(token);
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        str_cleanup(&tmp);
        n_cur_foreach_vars--;
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type != PARSETPL_TOK_VARIABLE || data->elements) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      n_cur_foreach_vars--;
      return PARSETPL_ERR_INVALIDTAG;
    }
    var2 = token;
    str_cstr_append(&tmp,"i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," = 0;\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp," = NULL;\n");
    str_cstr_append(&tmp,"if(vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp," && vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->type == TPL_VARIABLE_ARRAY) {\n");
    str_cstr_append(&tmp,"for(i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," = 0;i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp," < vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->data.d_array.elements;i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp,"++) {\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp," = (t_cf_tpl_variable*)array_element_at(&vf");
    str_cstr_append(&tmp,v1nb);
    str_cstr_append(&tmp,"->data.d_array,i");
    str_cstr_append(&tmp,ivnb);
    str_cstr_append(&tmp,");\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,"->arrayref = 1;\n");
    str_cstr_append(&tmp,"cf_tpl_setvar(tpl,\"");
    str_chars_append(&tmp,var2->data->content+1,var2->data->len-1);
    str_cstr_append(&tmp,"\",vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,");\n");
    str_cstr_append(&tmp,"vf");
    str_cstr_append(&tmp,v2nb);
    str_cstr_append(&tmp,"->arrayref = 0;\n");
    str_init(&vs);
    str_str_set(&vs,var2->data);
    array_push(&foreach_var_stack,&vs); // do not clean it up, this will be done by the array destroy function
    str_str_append(&output,&tmp);
    str_str_append(&output_mem,&tmp);
    destroy_token(var2); free(var2);
  } else {
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }
  return 0;
}

int process_if_tag(t_array *data) {
  t_string tmp;
  t_string *compop;
  t_token *token;
  t_string iv1,iv2,v1,v2;
  int invert, level, ret;
  int type1, type2;
  int *ilevel;
  t_token *tok1 = NULL, *tok2 = NULL;
  char buf[20];
  
  token = (t_token*)array_shift(data);
  if(token->type == PARSETPL_TOK_ENDIF) {
    destroy_token(token); free(token);
    if(data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!if_level_stack.elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    ilevel = array_pop(&if_level_stack);
    for(level = 0;level <= *ilevel;level++) {
      str_cstr_append(&output,"}\n");
      str_cstr_append(&output_mem,"}\n");
    }
    free(ilevel);
    return 0;
  } else if(token->type == PARSETPL_TOK_ELSE) {
    destroy_token(token); free(token);
    if(data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!if_level_stack.elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    str_cstr_append(&output,"} else {\n");
    str_cstr_append(&output_mem,"} else {\n");
    return 0;
  } else if(token->type == PARSETPL_TOK_ELSIF) {
    if(!if_level_stack.elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token->type = PARSETPL_TOK_IF;
    array_unshift(data,token);
    str_cstr_append(&output,"} else {\n");
    str_cstr_append(&output_mem,"} else {\n");
    ret = process_if_tag(data);
    if(ret < 0) {
      // length of } else {\n is 9 chars
      // remove the else again
      output.len -= 9;
      output.content[output.len] = '\0';
      output_mem.len -= 9;
      output_mem.content[output_mem.len] = '\0';
      return ret;
    }
    (*((int *)array_element_at(&if_level_stack,if_level_stack.elements-1)))++;
    return 0;
  } else if(token->type == PARSETPL_TOK_IF) {
    destroy_token(token); free(token);
    if(!data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type == PARSETPL_TOK_NOT) {
      invert = 1;
      destroy_token(token); free(token);
      if(!data->elements) {
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    } else {
      invert = 0;
    }
    str_init(&tmp);
    str_init(&iv1);
    str_init(&iv2);
    str_init(&v1);
    str_init(&v2);
    str_cstr_append(&tmp,"cmp_res = 0;\n");
    if(token->type == PARSETPL_TOK_VARIABLE) {
      type1 = PARSETPL_TOK_VARIABLE;
      str_char_set(&v1,"vi",2);
      snprintf(buf,19,"%ld",n_cur_if_vars++);
      str_cstr_append(&v1,buf);
      if(n_cur_if_vars > n_if_vars) {
        n_if_vars = n_cur_if_vars;
      }
      ret = dereference_variable(&tmp,token,data,&v1);
      destroy_token(token); free(token);
      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        return PARSETPL_ERR_INVALIDTAG;
      }
    } else if (token->type == PARSETPL_TOK_STRING || token->type == PARSETPL_TOK_INTEGER) {
      type1 = token->type;
      tok1 = token;
    } else if (token->type == PARSETPL_TOK_LOOPVAR) {
      type1 = token->type;
      str_char_set(&iv1,"ii",2);
      snprintf(buf,19,"%ld",n_cur_if_iters++);
      str_cstr_append(&iv1,buf);
      if(n_cur_if_iters > n_if_iters) {
        n_if_iters = n_cur_if_iters;
      }
      str_str_append(&tmp,&iv1);
      str_cstr_append(&tmp," = 0;\n");
      ret = dereference_iterator(&tmp,token,data,&iv1);
      destroy_token(token); free(token);
      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        return ret;
      }
    } else {
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!data->elements) {
      switch(type1) {
        case PARSETPL_TOK_STRING:
        case PARSETPL_TOK_INTEGER:
          str_cleanup(&tmp);
          str_cleanup(&iv1);
          str_cleanup(&iv2);
          str_cleanup(&v1);
          str_cleanup(&v2);
          destroy_token(tok1); free(tok1);
          return PARSETPL_ERR_INVALIDTAG;
        case PARSETPL_TOK_VARIABLE:
          if(invert) {
            str_cstr_append(&tmp,"if(!");
          } else {
            str_cstr_append(&tmp,"if(");
          }
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,") {\n");
          str_cstr_append(&tmp,"cmp_res = 1;\n");
          str_cstr_append(&tmp,"} else {\n");
          str_cstr_append(&tmp,"cmp_res = 0;\n");
          str_cstr_append(&tmp,"}\n");
          str_cstr_append(&tmp,"if(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp," && ");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,"); free(");
          str_str_append(&tmp,&v1);
          str_cstr_append(&tmp,"); }\n");
          str_cstr_append(&tmp,"if(cmp_res) {\n");
          n_cur_if_vars -= 1;
          break;
        case PARSETPL_TOK_LOOPVAR:
          if(invert) {
            str_cstr_append(&tmp,"if(!");
          } else {
            str_cstr_append(&tmp,"if(");
          }
          str_str_append(&tmp,&iv1);
          str_cstr_append(&tmp,") {\n");
          n_cur_if_iters -= 1;
          break;
        default:
          if(tok1) {
            destroy_token(tok1); free(tok1);
          }
          str_cleanup(&tmp);
          str_cleanup(&iv1);
          str_cleanup(&iv2);
          str_cleanup(&v1);
          str_cleanup(&v2);
          return PARSETPL_ERR_INVALIDTAG;
      }
      str_str_append(&output,&tmp);
      str_str_append(&output_mem,&tmp);
      str_cleanup(&tmp);
      level = 0;
      array_push(&if_level_stack,&level);
      return 0;
    }
    token = (t_token*)array_shift(data);
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        if(tok1) {
          destroy_token(tok1); free(tok1);
        }
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(!data->elements || token->type != PARSETPL_TOK_COMPARE) {
      destroy_token(token); free(token);
      str_cleanup(&tmp);
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);
      if(tok1) {
        destroy_token(tok1); free(tok1);
      }
      return PARSETPL_ERR_INVALIDTAG;
    }
    compop = token->data;
    free(token);
    token = (t_token*)array_shift(data);
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        if(tok1) {
          destroy_token(tok1); free(tok1);
        }
        str_cleanup(compop); free(compop);
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type == PARSETPL_TOK_VARIABLE) {
      type2 = PARSETPL_TOK_VARIABLE;
      str_char_set(&v2,"vi",2);
      snprintf(buf,19,"%ld",n_cur_if_vars++);
      str_cstr_append(&v2,buf);
      if(n_cur_if_vars > n_if_vars) {
        n_if_vars = n_cur_if_vars;
      }
      ret = dereference_variable(&tmp,token,data,&v2);
      destroy_token(token); free(token);
      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        if(tok1) {
          destroy_token(tok1); free(tok1);
        }
        str_cleanup(compop); free(compop);
        return PARSETPL_ERR_INVALIDTAG;
      }
    } else if (token->type == PARSETPL_TOK_STRING || token->type == PARSETPL_TOK_INTEGER) {
      type2 = token->type;
      tok2 = token;
    } else if (token->type == PARSETPL_TOK_LOOPVAR) {
      type2 = PARSETPL_TOK_LOOPVAR;
      str_char_set(&iv2,"ii",2);
      snprintf(buf,19,"%ld",n_cur_if_iters++);
      str_cstr_append(&iv2,buf);
      if(n_cur_if_iters > n_if_iters) {
        n_if_iters = n_cur_if_iters;
      }
      str_str_append(&tmp,&iv2);
      str_cstr_append(&tmp," = 0;\n");
      ret = dereference_iterator(&tmp,token,data,&iv2);
      destroy_token(token); free(token);
      if(ret < 0) {
        str_cleanup(&tmp);
        str_cleanup(&iv1);
        str_cleanup(&iv2);
        str_cleanup(&v1);
        str_cleanup(&v2);
        if(tok1) {
          destroy_token(tok1); free(tok1);
        }
        str_cleanup(compop); free(compop);
        return ret;
      }
    } else {
      str_cleanup(&tmp);
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);
      if(tok1) {
        destroy_token(tok1); free(tok1);
      }
      str_cleanup(compop); free(compop);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(data->elements) {
      str_cleanup(&iv1);
      str_cleanup(&iv2);
      str_cleanup(&v1);
      str_cleanup(&v2);
      str_cleanup(&tmp);
      if(tok1) {
        destroy_token(tok1); free(tok1);
      }
      if(tok2) {
        destroy_token(tok2); free(tok2);
      }
      str_cleanup(compop); free(compop);
      return PARSETPL_ERR_INVALIDTAG;
    }
    level = 0;
    switch(type2) {
      case PARSETPL_TOK_VARIABLE:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // if both are arrays, just compare the number of elements
            str_cstr_append(&tmp,"if(!");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              // two nonexistant variables are the same
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else ");
            str_cstr_append(&tmp,"if((!");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,") || (");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,")) {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"} else ");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->type == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_ARRAY) {\n");
            str_cstr_append(&tmp,"cmp_res = (");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_array.elements == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_array.elements) ? ");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"0 : 1;\n");
            } else {
              str_cstr_append(&tmp,"1 : 0;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ((");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_STRING && !strcmp(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_string.content,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_string.content)) || (");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->type == TPL_VARIABLE_INT && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int))) {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); }\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); }\n");
            str_cstr_append(&tmp,"}\n");
            n_cur_if_vars -= 2;
            break;
          case PARSETPL_TOK_LOOPVAR:
            // convert it to integer
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            n_cur_if_iters -= 1;
            break;
          case PARSETPL_TOK_INTEGER:
            // convert it to integer
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,tok1->data);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            break;
          case PARSETPL_TOK_STRING:
            // convert it to string
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,",TPL_VARIABLE_STRING);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && !strcmp(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->data.d_string.content,\"");
            append_escaped_string(&tmp,tok1->data);
            str_cstr_append(&tmp,"\")) {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v2);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            break;
          default:
            str_chars_append(&tmp,"cmp_res = 0;\n",13);
            break;
        }
        break;
      case PARSETPL_TOK_LOOPVAR:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to integer
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            n_cur_if_iters -= 1;
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            n_cur_if_iters -= 2;
            break;
          case PARSETPL_TOK_INTEGER:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,tok1->data);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            n_cur_if_iters -= 1;
            break;
          case PARSETPL_TOK_STRING:
            str_cstr_append(&tmp,"if(strtol(\"");
            append_escaped_string(&tmp,tok1->data);
            str_cstr_append(&tmp,"\",NULL,10) == ");
            str_str_append(&tmp,&iv2);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            n_cur_if_iters -= 1;
            break;
          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;
      case PARSETPL_TOK_STRING:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to string
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_STRING);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && !strcmp(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_string.content,\"");
            append_escaped_string(&tmp,tok2->data);
            str_cstr_append(&tmp,"\")) {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(strtol(\"");
            append_escaped_string(&tmp,tok2->data);
            str_cstr_append(&tmp,"\",NULL,10) == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            n_cur_if_iters -= 1;
            break;
          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(!strcmp(tok1->data->content,tok2->data->content)) {
              if(compop->content[0] == '!') {
                str_cstr_append(&tmp,"cmp_res = 0;\n");
              } else {
                str_cstr_append(&tmp,"cmp_res = 1;\n");
              }
            } else {
              if(compop->content[0] == '!') {
                str_cstr_append(&tmp,"cmp_res = 1;\n");
              } else {
                str_cstr_append(&tmp,"cmp_res = 0;\n");
              }
            }
            break;
          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;
      case PARSETPL_TOK_INTEGER:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to integer
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,",TPL_VARIABLE_INT);\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->data.d_int == ");
            str_str_append(&tmp,tok2->data);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp," && ");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"->temporary) { cf_tpl_var_destroy(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); free(");
            str_str_append(&tmp,&v1);
            str_cstr_append(&tmp,"); }\n");
            n_cur_if_vars -= 1;
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_cstr_append(&tmp,"if(");
            str_str_append(&tmp,tok2->data);
            str_cstr_append(&tmp," == ");
            str_str_append(&tmp,&iv1);
            str_cstr_append(&tmp,") {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            }
            str_cstr_append(&tmp,"} else {\n");
            if(compop->content[0] == '!') {
              str_cstr_append(&tmp,"cmp_res = 1;\n");
            } else {
              str_cstr_append(&tmp,"cmp_res = 0;\n");
            }
            str_cstr_append(&tmp,"}\n");
            n_cur_if_iters -= 1;
            break;
          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(strtol(tok1->data->content,NULL,10) == strtol(tok2->data->content,NULL,10)) {
              if(compop->content[0] == '!') {
                str_cstr_append(&tmp,"cmp_res = 0;\n");
              } else {
                str_cstr_append(&tmp,"cmp_res = 1;\n");
              }
            } else {
              if(compop->content[0] == '!') {
                str_cstr_append(&tmp,"cmp_res = 1;\n");
              } else {
                str_cstr_append(&tmp,"cmp_res = 0;\n");
              }
            }
            break;
          default:
            str_cstr_append(&tmp,"cmp_res = 0;\n");
            break;
        }
        break;
      default:
        str_cstr_append(&tmp,"cmp_res = 0;\n");
        break;
    }
    str_cstr_append(&tmp,"if(cmp_res) {\n");
    str_str_append(&output,&tmp);
    str_str_append(&output_mem,&tmp);
    str_cleanup(&tmp);
    str_cleanup(&iv1);
    str_cleanup(&iv2);
    str_cleanup(&v1);
    str_cleanup(&v2);
    str_cleanup(compop); free(compop);
    if(tok1) {
      destroy_token(tok1); free(tok1);
    }
    if(tok2) {
      destroy_token(tok2); free(tok2);
    }
    level = 0;
    array_push(&if_level_stack,&level);
    return 0;
  } else { // elseif is not supported currently!
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }
  return PARSETPL_ERR_INVALIDTAG;
}

int process_tag(t_array *data) {
  t_token *variable, *token;
  int ret, had_whitespace, rtype;
  
  if(!data->elements) {
    return PARSETPL_ERR_INVALIDTAG;
  }
  
  rtype = ((t_token*)array_element_at(data,0))->type;
  
  if(rtype == PARSETPL_TOK_VARIABLE) {
    variable = (t_token *)array_shift(data);
    // 2 possibilities:
    //   a) print $variable ($variable,$variable[0][2],$variable->escaped,$variable[0]->escaped)
    //   b) assign new value to $variable ($variable = "value",$variable = 0,$variable = ["hi","ho"],$variable = [["hi","ho"],["ha","hu"]])
    
    // no more elements or next element is => type a
    if(!data->elements) {
      // directly output
      ret = process_variable_print_tag(variable,data);
    } else {
      had_whitespace = 0;
      do {
        token = (t_token *)array_shift(data);
        if(token->type == PARSETPL_TOK_WHITESPACE) {
          had_whitespace = 1;
          continue;
        } else if(token->type == PARSETPL_TOK_ASSIGNMENT) {
          break;
        } else if(token->type == PARSETPL_TOK_ARRAYSTART || token->type == PARSETPL_TOK_MODIFIER_ESCAPE) {
          if(had_whitespace) {
            destroy_token(variable); free(variable);
            destroy_token(token); free(token);
            return PARSETPL_ERR_INVALIDTAG;
          }
          array_unshift(data,token);
          break;
        } else {
          destroy_token(variable); free(variable);
          destroy_token(token); free(token);
          return PARSETPL_ERR_INVALIDTAG;
        }
        destroy_token(token); free(token);
      } while(data->elements);
      if(token->type == PARSETPL_TOK_ASSIGNMENT) {
        ret = process_variable_assignment_tag(variable,data);
      } else {
        ret = process_variable_print_tag(variable,data);
      }
      destroy_token(token); free(token);
    }
    destroy_token(variable); free(variable);
    if (ret < 0) {
      return ret;
    }
    return 0;
  } else if(rtype == PARSETPL_TOK_LOOPVAR) {
    token = (t_token *)array_shift(data);
    // directly output
    ret = process_iterator_print_tag(token,data);
    destroy_token(token); free(token);
    if (ret < 0) {
      return ret;
    }
    return 0;
  } else if(rtype == PARSETPL_TOK_IF || rtype == PARSETPL_TOK_ELSE || rtype == PARSETPL_TOK_ELSIF || rtype == PARSETPL_TOK_ENDIF) {
    return process_if_tag(data);
  } else if(rtype == PARSETPL_TOK_INCLUDE) {
    token = (t_token*)array_shift(data);
    destroy_token(token); free(token);
    
    if(!data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type != PARSETPL_TOK_STRING || data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    return process_include_tag(token->data);
  } else if(rtype == PARSETPL_TOK_FOREACH || rtype == PARSETPL_TOK_ENDFOREACH) {
    return process_foreach_tag(data);
  }
  return PARSETPL_ERR_INVALIDTAG;
}









/*****************************************
 * parse_file
 */
int parse_file(const u_char *filename) {
  u_char *basename, *p;
  t_string output_name;
  FILE *ifp, *ofp;
  int ret;
  long i;
  char buf[20];
  t_array data;
  int tag_started = 0;
  t_token token;
  
  basename = strdup((u_char *)filename);
  if(!strcmp(PARSETPL_INCLUDE_EXT,basename+strlen(basename)-strlen(PARSETPL_INCLUDE_EXT))) {
    basename[strlen(basename)-strlen(PARSETPL_INCLUDE_EXT)] = '\0';
  }
  str_init(&output_name);
  str_init(&current_file); // global variable
  str_char_set(&output_name,basename,strlen(basename));
  str_chars_append(&output_name,".c",2);
  p = strrchr(basename,'/');
  if(!p) p = basename;
  str_char_set(&current_file,p,strlen(p));
  free(basename);
  
  // now open input file
  ifp = fopen(filename,"rb");
  if(!ifp) {
    fprintf(stderr,"Error opening %s for reading\n", filename);
    str_cleanup(&output_name);
    str_cleanup(&current_file);
    return -1;
  }
  // open output file
  ofp = fopen(output_name.content,"wb");
  if(!ofp) {
    fprintf(stderr,"Error opening %s for writing\n", output_name.content);
    fclose(ifp);
    str_cleanup(&output_name);
    str_cleanup(&current_file);
    return -1;
  }
  str_cleanup(&output_name); // we don't need it anymore
  
  yyin = ifp;
  str_init(&content);
  str_init(&output);
  array_init(&data,sizeof(t_token),destroy_token);
  array_init(&foreach_var_stack,sizeof(t_string),(void(*)(void *))str_cleanup);
  array_init(&if_level_stack,sizeof(int),NULL);
  
  do {
    ret = parsetpl_lex();
    if(ret < PARSETPL_TOK_EOF) {
      if(!tag_started) {
        break; // error
      }
      yy_pop_state();
      if(content.content) free(content.content);
      str_init(&content);
      str_str_append(&content,&content_backup);
      array_destroy(&data);
      array_init(&data,sizeof(t_token),destroy_token);
      tag_started = 0;
      ret = PARSETPL_TOK_TAGEND;// dummy
      continue;
    }
    if(tag_started) {
      if(ret == PARSETPL_TOK_TAGEND) {
        ret = process_tag(&data);
        if(ret < PARSETPL_TOK_EOF) { // no error
          // append content backup
          str_str_append(&content,&content_backup);
        }
        array_destroy(&data);
        array_init(&data,sizeof(t_token),destroy_token);
        tag_started = 0;
      } else {
        token.type = ret;
        token.data = fo_alloc(NULL,sizeof(t_string),1,FO_ALLOC_MALLOC);
        str_init(token.data);
        if(ret == PARSETPL_TOK_STRING) {
          str_str_set(token.data,&string);
        } else {
          str_char_set(token.data,yytext,yyleng);
        }
        array_push(&data,&token);
      }
      // dummy: so that the loop doesn't end
      ret = 1;
      continue;
    }
    // a new tag starts or EOF
    if(content.len) {
      str_chars_append(&output,"my_write(\"",10);
      str_chars_append(&output_mem,"str_chars_append(&tpl->parsed,\"",31);
      append_escaped_string(&output,&content);
      append_escaped_string(&output_mem,&content);
      str_chars_append(&output,"\");\n",4);
      str_chars_append(&output_mem,"\",", 2);
      snprintf(buf,19,"%ld",content.len);
      str_chars_append(&output_mem,buf,strlen(buf));
      str_chars_append(&output_mem,");\n",3);
    }
    if(ret == PARSETPL_TOK_TAGSTART) {
      tag_started = 1;
    }
  } while(ret > PARSETPL_TOK_EOF);
  
  fclose(ifp); // we don't need it any more
  
  if(ret < 0) {
    fprintf(stderr,"Error processing %s: %d\n", filename, ret);
    str_cleanup(&content);
    str_cleanup(&output);
    str_cleanup(&current_file);
    array_destroy(&data);
    array_destroy(&foreach_var_stack);
    fclose(ofp);
    return ret;
  }
  
  fprintf(ofp, "/*\n * this is a template file\n *\n */\n\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n#include \"config.h\"\n#include \"defines.h\"\n\n#include \"utils.h\"\n#include \"hashlib.h\"\n#include \"charconvert.h\"\n#include \"template.h\"\n\nstatic void my_write(const u_char *s) {\n  register u_char *ptr;\n\n  for(ptr = (u_char *)s;*ptr;ptr++) {\n    fputc(*ptr,stdout);\n  }\n}\n");
  fprintf(ofp,"void parse(t_cf_template *tpl) {\nt_cf_tpl_variable *v = NULL;\n");
  if(uses_print) {
    fprintf(ofp,"t_cf_tpl_variable *vp = NULL;\n");
  }
  if(uses_clonevar) {
    fprintf(ofp,"t_cf_tpl_variable *vc = NULL;\n");
  }
  if(uses_loopassign) {
    fprintf(ofp,"long ic = 0;\n");
  }
  if(uses_tmpstring) {
    fprintf(ofp,"t_string tmp_string;\n");
  }
  if(uses_iter_print) {
    fprintf(ofp,"long iter_var = 0;\n");
  }
  fprintf(ofp,"long cmp_res = 0;\n");
  for(i = 0;i < n_assign_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *va%ld = NULL;\n",i);
  }
  for(i = 0;i < n_if_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *vi%ld = NULL;\n",i);
  }
  for(i = 0;i < n_if_iters;i++) {
    fprintf(ofp,"long ii%ld = 0;\n",i);
  }
  for(i = 0;i < n_foreach_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *vf%ld = NULL;\n",i*2);
    fprintf(ofp,"t_cf_tpl_variable *vf%ld = NULL;\n",i*2+1);
    fprintf(ofp,"int i%ld = 0;\n",i);
  }
  if(uses_include) {
    fprintf(ofp,"t_cf_template *inc_tpl;\n");
    fprintf(ofp,"t_string *inc_filename, *inc_filepart, *inc_fileext;\n");
    fprintf(ofp,"u_char *p;\n");
    fprintf(ofp,"t_cf_hash *ov;\n");
    fprintf(ofp,"int ret;\n");
  }
  fprintf(ofp,"\n%s\n}\n\n",output.content);
  fprintf(ofp,"void parse_to_mem(t_cf_template *tpl) {\nt_cf_tpl_variable *v = NULL;\n");
  if(uses_print) {
    fprintf(ofp,"t_cf_tpl_variable *vp = NULL;\n");
    fprintf(ofp,"u_char *tmp = NULL;\n");
  }
  if(uses_iter_print) {
    fprintf(ofp,"long iter_var = 0;\n");
  }
  if(uses_clonevar) {
    fprintf(ofp,"t_cf_tpl_variable *vc = NULL;\n");
  }
  if(uses_loopassign) {
    fprintf(ofp,"long ic = 0;\n");
  }
  if(uses_tmpstring) {
    fprintf(ofp,"t_string tmp_string;\n");
  }
  fprintf(ofp,"long cmp_res = 0;\n");
  fprintf(ofp,"char iter_buf[20];\n");
  for(i = 0;i < n_assign_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *va%ld = NULL;\n",i);
  }
  for(i = 0;i < n_if_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *vi%ld = NULL;\n",i);
  }
  for(i = 0;i < n_if_iters;i++) {
    fprintf(ofp,"long ii%ld = 0;\n",i);
  }
  for(i = 0;i < n_foreach_vars;i++) {
    fprintf(ofp,"t_cf_tpl_variable *vf%ld = NULL;\n",i*2);
    fprintf(ofp,"t_cf_tpl_variable *vf%ld = NULL;\n",i*2+1);
    fprintf(ofp,"int i%ld = 0;\n",i);
  }
  if(uses_include) {
    fprintf(ofp,"t_cf_template *inc_tpl;\n");
    fprintf(ofp,"t_string *inc_filename, *inc_filepart, *inc_fileext;\n");
    fprintf(ofp,"u_char *p;\n");
    fprintf(ofp,"t_cf_hash *ov;\n");
    fprintf(ofp,"int ret;\n");
  }
  fprintf(ofp,"\n%s\n}\n\n",output_mem.content);
  fclose(ofp);
  str_cleanup(&content);
  str_cleanup(&output);
  str_cleanup(&current_file);
  array_destroy(&data);
  array_destroy(&foreach_var_stack);
  return 0;
}

int main(int argc,char **argv) {
  if(argc != 2) {
    fprintf(stderr,"Usage: %s filename\n",argv[0]);
    return -1;
  }
  return parse_file(argv[1]);
}
