/*
 * parsetpl.lex
 * The temaplte parser
 */
%{
#include "../utils.h"
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

#define PARSETPL_INCLUDE_EXT     ".html"

#define PARSETPL_ERR -1
#define PARSETPL_ERR_FILENOTFOUND          -1
#define PARSETPL_ERR_UNRECOGNIZEDCHARACTER -2
#define PARSETPL_ERR_UNTERMINATEDSTRING    -3 
#define PARSETPL_ERR_UNTERMINATEDTAG       -4 
#define PARSETPL_ERR_INVALIDTAG            -5
#define PARSETPL_ERR_NOTINLOOP             -6

static long lineno                   = 0;
static t_string  string              = { 0, 0, NULL };
static t_string  content             = { 0, 0, NULL };
static t_string  content_backup      = { 0, 0, NULL };
static t_string  output              = { 0, 0, NULL };
static t_string  output_mem          = { 0, 0, NULL };
static t_string  current_file        = { 0, 0, NULL };
static t_array   foreach_var_stack;

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
  
  str_chars_append(out_str,"v = (t_cf_tpl_variable *)cf_tpl_getvar(tpl,\"",44);
  str_chars_append(out_str,var->data->content+1,var->data->len-1);
  str_chars_append(out_str,"\");\n",4);
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
    if(token->type != PARSETPL_TOK_INTEGER || !data->elements) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
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
    str_chars_append(out_str,"if(v && v->type == TPL_VARIABLE_ARRAY) {\n",41);
    str_chars_append(out_str,"v = (t_cf_tpl_variable *)array_element_at(&v->data.d_array,",59);
    str_chars_append(out_str,arrayidx->content,arrayidx->len);
    str_cleanup(arrayidx);
    free(arrayidx);
    str_chars_append(out_str,");\n",3);
    level++;
  }
  str_str_append(out_str,c_var);
  str_chars_append(out_str," = v;\n",6);
  for(;level > 0;level--) {
    str_chars_append(out_str,"}\n",2);
  }
  return 0;
}

int dereference_iterator(t_string *out_str,t_token *var,t_array *data,t_string *c_var) {
  if(strcmp(var->data->content,"@first") && strcmp(var->data->content,"@last") && strcmp(var->data->content,"@iterator")) {
    return PARSETPL_ERR_INVALIDTAG;
  }
  if(!foreach_var_stack.elements) {
    return PARSETPL_ERR_NOTINLOOP;
  }
  if(!strcmp(var->data->content,"@first")) {
    str_str_append(out_str,c_var);
    str_chars_append(out_str," = (i == 0) ? 1 : 0;\n",21);
  } else if(!strcmp(var->data->content,"@last")) {
    str_str_append(out_str,c_var);
    str_chars_append(out_str," = (i == va->data.d_array.elements - 1) ? 1 : 0;\n",49);
  } else if(!strcmp(var->data->content,"@iterator")) {
    str_str_append(out_str,c_var);
    str_chars_append(out_str," = i;\n",6);
  }
  return 0;
}

int process_array_assignment(t_array *data,t_string *tmp) {
  t_token *token;
  int had_sep, ret;
  char buf[20];
  
  str_chars_append(tmp,"{\nt_cf_tpl_variable *tv;\n",25);
  str_chars_append(tmp,"tv = fo_alloc(NULL,sizeof(t_cf_tpl_variable),1,FO_ALLOC_MALLOC);\n",65);
  str_chars_append(tmp,"cf_tpl_var_init(tv,TPL_VARIABLE_ARRAY);\n",40);
  had_sep = 1;
  while(data->elements) {
    token = (t_token *)array_shift(data);
    if(had_sep && token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      continue;
    }
    if(had_sep && token->type != PARSETPL_TOK_ARRAYSTART && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_STRING) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(!had_sep && token->type != PARSETPL_TOK_ARRAYEND && token->type != PARSETPL_TOK_ARRAYSEP) {
      destroy_token(token); free(token);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(token->type == PARSETPL_TOK_ARRAYEND) {
      destroy_token(token); free(token);
      break;
    }
    if(token->type == PARSETPL_TOK_ARRAYSEP) {
      had_sep = 1;
      destroy_token(token); free(token);
      continue;
    }
    had_sep = 0;
    if(token->type == PARSETPL_TOK_STRING) {
      str_chars_append(tmp,"cf_tpl_var_addvalue(tv,TPL_VARIABLE_STRING,\"",44);
      append_escaped_string(tmp,token->data);
      str_chars_append(tmp,"\",",2);
      snprintf(buf,19,"%ld",token->data->len);
      str_chars_append(tmp,buf,strlen(buf));
      str_chars_append(tmp,");\n",3);
    } else if(token->type == PARSETPL_TOK_INTEGER) {
      str_chars_append(tmp,"cf_tpl_var_addvalue(tv,TPL_VARIABLE_INT,",40);
      str_chars_append(tmp,token->data->content,strlen(token->data->content));
      str_chars_append(tmp,");\n",3);
    } else if(token->type == PARSETPL_TOK_ARRAYSTART) {
      ret = process_array_assignment(data,tmp);
      if(ret < 0) {
        destroy_token(token); free(token);
        return ret;
      }
      str_chars_append(tmp,"cf_tpl_var_add(tv,nv);\n",23);
    }
    destroy_token(token); free(token);
  }
  str_chars_append(tmp,"nv = tv;\n",9);
  str_chars_append(tmp,"}\n",2);
  return 0;
}

int process_variable_assignment_tag(t_token *variable,t_array *data) {
  t_string tmp;
  t_token *token;
  char buf[20];
  int ret;
  
  str_init(&tmp);
  
  // remove all whitespaces
  while(data->elements) {
    token = (t_token *)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE) {
      break;
    }
  }
  if(token->type != PARSETPL_TOK_STRING && token->type != PARSETPL_TOK_INTEGER && token->type != PARSETPL_TOK_ARRAYSTART) {
    str_cleanup(&tmp);
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }
  switch(token->type) {
    case PARSETPL_TOK_STRING:
      str_chars_append(&tmp,"cf_tpl_setvalue(tpl,\"",21);
      str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
      str_chars_append(&tmp,"\",TPL_VARIABLE_STRING,\"",23);
      append_escaped_string(&tmp,token->data);
      str_chars_append(&tmp,"\",",2);
      snprintf(buf,19,"%ld",token->data->len);
      str_chars_append(&tmp,buf,strlen(buf));
      str_chars_append(&tmp,");\n",3);
      break;
    case PARSETPL_TOK_INTEGER:
      str_chars_append(&tmp,"cf_tpl_setvalue(tpl,\"",21);
      str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
      str_chars_append(&tmp,"\",TPL_VARIABLE_INT,",19);
      str_chars_append(&tmp,token->data->content,strlen(token->data->content));
      str_chars_append(&tmp,");\n",3);
      break;
    case PARSETPL_TOK_ARRAYSTART:
      ret = process_array_assignment(data,&tmp);
      if(ret < 0) {
        destroy_token(token); free(token);
        return PARSETPL_ERR_INVALIDTAG;
      }
      if(data->elements) {
        destroy_token(token); free(token);
        return PARSETPL_ERR_INVALIDTAG;
      }
      str_chars_append(&tmp,"cf_tpl_setvar(tpl,\"",19);
      str_chars_append(&tmp,variable->data->content+1,variable->data->len-1);
      str_chars_append(&tmp,"\",nv);\n",7);
      break;
  }
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
  str_chars_append(&tmp,"{\nt_cf_tpl_variable *vp = NULL;\n",32);
  ret = dereference_variable(&tmp,variable,data,&c_var);
  str_cleanup(&c_var);
  if(ret < 0) {
    ret = 0;
  }
  if(data->elements) {
    token = (t_token *)array_shift(data);
    destroy_token(token); free(token);
    if(token->type != PARSETPL_TOK_MODIFIER_ESCAPE || data->elements) {
      str_cleanup(&tmp);
      return PARSETPL_ERR_INVALIDTAG;
    }
    escape_html = 1;
  }
  str_chars_append(&tmp,"if(vp) {\n",9);
  str_chars_append(&tmp,"if(vp->type != TPL_VARIABLE_STRING) {\n",38);
  str_chars_append(&tmp,"vp = cf_tpl_var_convert(NULL,vp,TPL_VARIABLE_STRING);\n}\n",56);
  str_chars_append(&tmp,"if(vp && vp->type == TPL_VARIABLE_STRING) {\n",44);
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  str_cleanup(&tmp);
  
  if(escape_html) {
    str_chars_append(&output,"print_htmlentities_encoded(vp->data.d_string.content,0,stdout);\n}\n}\n",68);
    str_chars_append(&output_mem,"tmp = htmlentities(vp->data.d_string.content,0);\nstr_chars_append(&tpl->parsed,tmp,strlen(tmp));\nfree(tmp);\n}\n}\n",112);
  } else {
    str_chars_append(&output,"my_write(vp->data.d_string.content);\n}\n}\n",41);
    str_chars_append(&output_mem,"str_chars_append(&tpl->parsed,vp->data.d_string.content,strlen(vp->data.d_string.content));\n}\n}\n",96);
  }
  str_chars_append(&output,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n}\n",62);
  str_chars_append(&output_mem,"if(vp && vp->temporary) {\ncf_tpl_var_destroy(vp); free(vp);\n}\n",62);
  str_chars_append(&output,"}\n",2);
  str_chars_append(&output_mem,"}\n",2);
  return 0;
}

int process_iterator_print_tag (t_token *iterator,t_array *data) {
  t_string tmp;
  t_string c_var;
  t_token *token;
  int ret;
  
  str_init(&tmp);
  str_init(&c_var);
  str_char_set(&c_var,"iter_var",8);
  str_chars_append(&tmp,"{\nlong iter_var = 0;\n",21);
  str_chars_append(&tmp,"char buf[20];\n",14);
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
  str_chars_append(&output,"printf(\"%ld\",iter_var);\n",24);
  str_chars_append(&output_mem,"snprintf(buf,19,\"%ld\",iter_var);\n",33);
  str_chars_append(&output_mem,"str_chars_append(&tpl->parsed,buf,strlen(buf));\n",48);
  str_chars_append(&output,"}\n",2);
  str_chars_append(&output_mem,"}\n",2);
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
  str_chars_append(&tmp,"{\n",2);
  str_chars_append(&tmp,"t_cf_template *inc_tpl;\n",24);
  str_chars_append(&tmp,"t_string *inc_filename, *inc_filepart, *inc_fileext;\n",53);
  str_chars_append(&tmp,"u_char *p;\n",11);
  str_chars_append(&tmp,"t_cf_hash *ov;\n",15);
  str_chars_append(&tmp,"int ret;\n",9);
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
  str_chars_append(&tmp,"}\n",2);
  str_str_append(&output,&tmp);
  str_str_append(&output_mem,&tmp);
  return 0;
}

int process_foreach_tag(t_array *data) {
  t_string tmp,vs;
  t_token *token,*var1,*var2;
  t_string *tvs;
  
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
    // make sure that the variable is invalid because the memory structure will be freed by cf_tpl_setvar
    str_init(&tmp);
    str_chars_append(&tmp,"v3 = (t_cf_tpl_variable *)cf_tpl_getvar(tpl,\"",45);
    str_chars_append(&tmp,tvs->content+1,tvs->len-1);
    str_chars_append(&tmp,"\");\n",4);
    str_chars_append(&tmp,"if(v3) v3->type = TPL_VARIABLE_INVALID;\n",40);
    str_chars_append(&tmp,"}\n}\n}\n",6);
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
    var1 = token;
    token = (t_token*)array_shift(data);
    if(token->type != PARSETPL_TOK_WHITESPACE || !data->elements) {
      destroy_token(token); free(token);
      destroy_token(var1); free(var1);
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        destroy_token(var1); free(var1);
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
      destroy_token(var1); free(var1);
      return PARSETPL_ERR_INVALIDTAG;
    }
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        destroy_token(var1); free(var1);
        return PARSETPL_ERR_INVALIDTAG;
      }
      token = (t_token*)array_shift(data);
    }
    if(token->type != PARSETPL_TOK_VARIABLE || data->elements) {
      destroy_token(token); free(token);
      destroy_token(var1); free(var1);
      return PARSETPL_ERR_INVALIDTAG;
    }
    var2 = token;
    str_init(&tmp);
    str_chars_append(&tmp,"{\n",2);
    str_chars_append(&tmp,"int i = 0;\n",11);
    str_chars_append(&tmp,"t_cf_tpl_variable *va = NULL;\n",30);
    str_chars_append(&tmp,"t_cf_tpl_variable *v3 = NULL;\n",30);
    str_chars_append(&tmp,"va = (t_cf_tpl_variable *)cf_tpl_getvar(tpl,\"",45);
    str_chars_append(&tmp,var1->data->content+1,var1->data->len-1);
    str_chars_append(&tmp,"\");\n",4);
    str_chars_append(&tmp,"if(va && va->type == TPL_VARIABLE_ARRAY) {\n",43);
    str_chars_append(&tmp,"for(i = 0; i < va->data.d_array.elements; i++) {\n",49);
    str_chars_append(&tmp,"v3 = (t_cf_tpl_variable*)array_element_at(&va->data.d_array,i);\n",64);
    str_chars_append(&tmp,"cf_tpl_setvar(tpl,\"",19);
    str_chars_append(&tmp,var2->data->content+1,var2->data->len-1);
    str_chars_append(&tmp,"\",v3);\n",7);
    str_init(&vs);
    str_str_set(&vs,var2->data);
    array_push(&foreach_var_stack,&vs); // do not clean it up, this will be done by the array destroy function
        
    str_str_append(&output,&tmp);
    str_str_append(&output_mem,&tmp);
  } else {
    destroy_token(token); free(token);
    return PARSETPL_ERR_INVALIDTAG;
  }
  return 0;
}

int process_if_tag(t_array *data) {
  t_string tmp,varn;
  t_string *compop;
  t_token *token;
  int invert, level, ret;
  int type1, type2;
  t_token *tok1 = NULL, *tok2 = NULL;
  
  token = (t_token*)array_shift(data);
  if(token->type == PARSETPL_TOK_ENDIF) {
    destroy_token(token); free(token);
    if(data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    str_chars_append(&output,"}\n}\n",4);
    str_chars_append(&output_mem,"}\n}\n",4);
    return 0;
  } else if(token->type == PARSETPL_TOK_ELSE) {
    destroy_token(token); free(token);
    if(data->elements) {
      return PARSETPL_ERR_INVALIDTAG;
    }
    str_chars_append(&output,"} else {\n",9);
    str_chars_append(&output_mem,"} else {\n",9);
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
    str_chars_append(&tmp,"{\nt_cf_tpl_variable *v1 = NULL;\n",32);
    str_chars_append(&tmp,"t_cf_tpl_variable *v2 = NULL;\n",30);
    str_chars_append(&tmp,"long iter_var1 = 0;\n",20);
    str_chars_append(&tmp,"long iter_var2 = 0;\n",20);
    str_chars_append(&tmp,"long cmp_res = 0;\n",18);
    if(token->type == PARSETPL_TOK_VARIABLE) {
      type1 = PARSETPL_TOK_VARIABLE;
      str_init(&varn);
      str_char_set(&varn,"v1",2);
      ret = dereference_variable(&tmp,token,data,&varn);
      destroy_token(token); free(token);
      str_cleanup(&varn);
      if(ret < 0) {
        str_cleanup(&tmp);
        return PARSETPL_ERR_INVALIDTAG;
      }
    } else if (token->type == PARSETPL_TOK_STRING || token->type == PARSETPL_TOK_INTEGER) {
      type1 = token->type;
      tok1 = token;
    } else if (token->type == PARSETPL_TOK_LOOPVAR) {
      type1 = token->type;
      str_init(&varn);
      str_char_set(&varn,"iter_var1",9);
      ret = dereference_iterator(&tmp,token,data,&varn);
      destroy_token(token); free(token);
      str_cleanup(&varn);
      if(ret < 0) {
        str_cleanup(&tmp);
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
          destroy_token(tok1); free(tok1);
          return PARSETPL_ERR_INVALIDTAG;
        case PARSETPL_TOK_VARIABLE:
          if(invert) {
            str_chars_append(&tmp,"if(!v1) {\n",10);
          } else {
            str_chars_append(&tmp,"if(v1) {\n",9);
          }
          break;
        case PARSETPL_TOK_LOOPVAR:
          if(invert) {
            str_chars_append(&tmp,"if(!iter_var1) {\n",17);
          } else {
            str_chars_append(&tmp,"if(iter_var1) {\n",16);
          }
          break;
        default:
          if(tok1) {
            destroy_token(tok1); free(tok1);
          }
          str_cleanup(&tmp);
          return PARSETPL_ERR_INVALIDTAG;
      }
      str_str_append(&output,&tmp);
      str_str_append(&output_mem,&tmp);
      str_cleanup(&tmp);
      return 0;
    }
    token = (t_token*)array_shift(data);
    while(token->type == PARSETPL_TOK_WHITESPACE) {
      destroy_token(token); free(token);
      if(!data->elements) {
        str_cleanup(&tmp);
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
      str_init(&varn);
      str_char_set(&varn,"v2",2);
      ret = dereference_variable(&tmp,token,data,&varn);
      destroy_token(token); free(token);
      str_cleanup(&varn);
      if(ret < 0) {
        str_cleanup(&tmp);
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
      type2 = token->type;
      str_init(&varn);
      str_char_set(&varn,"iter_var2",9);
      ret = dereference_iterator(&tmp,token,data,&varn);
      destroy_token(token); free(token);
      str_cleanup(&varn);
      if(ret < 0) {
        str_cleanup(&tmp);
        if(tok1) {
          destroy_token(tok1); free(tok1);
        }
        str_cleanup(compop); free(compop);
        return ret;
      }
    } else {
      str_cleanup(&tmp);
      if(tok1) {
        destroy_token(tok1); free(tok1);
      }
      str_cleanup(compop); free(compop);
      return PARSETPL_ERR_INVALIDTAG;
    }
    if(data->elements) {
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
            str_chars_append(&tmp,"if(!v1 && !v2) {\n",17);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              // two nonexistant variables are the same
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else ",7);
            str_chars_append(&tmp,"if((!v1 && v2) || (v1 && !v2)) {\n",33);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"} else ",7);
            str_chars_append(&tmp,"if(v1 && v2 && v1->type == v2->type == TPL_VARIABLE_ARRAY) {\n",61);
            str_chars_append(&tmp,"cmp_res = (v1->data.d_array.elements == v2->data.d_array.elements) ? ",69);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"0 : 1;\n",7);
            } else {
              str_chars_append(&tmp,"1 : 0;\n",7);
            }
            str_chars_append(&tmp,"} else {\n",9);
            str_chars_append(&tmp,"v1 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v1,v2->type);\n",64);
            str_chars_append(&tmp,"if(v1 && ((v2->type == TPL_VARIABLE_STRING && !strcmp(v1->data.d_string.content,v2->data.d_string.content)) || (v2->type == TPL_VARIABLE_INT && v1->data.d_int == v2->data.d_int))) {\n",182);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v1 && v1->temporary) cf_tpl_var_destroy(v1);\n",48);
            str_chars_append(&tmp,"if(v2 && v2->temporary) cf_tpl_var_destroy(v2);\n",48);
            str_chars_append(&tmp,"}\n",2);
            break;
          case PARSETPL_TOK_LOOPVAR:
            // convert it to integer
            str_chars_append(&tmp,"v2 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v2,TPL_VARIABLE_INT);\n",72);
            str_chars_append(&tmp,"if(v2 && v2->data.d_int == iter_var1) {\n",40);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v2 && v2->temporary) cf_tpl_var_destroy(v2);\n",48);
            break;
          case PARSETPL_TOK_INTEGER:
            // convert it to integer
            str_chars_append(&tmp,"v2 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v2,TPL_VARIABLE_INT);\n",72);
            str_chars_append(&tmp,"if(v2 && v2->data.d_int == ",24);
            str_str_append(&tmp,tok1->data);
            str_chars_append(&tmp,") {\n",4);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v2 && v1->temporary) cf_tpl_var_destroy(v2);\n",48);
            break;
          case PARSETPL_TOK_STRING:
            // convert it to string
            str_chars_append(&tmp,"v2 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v2,TPL_VARIABLE_STRING);\n",75);
            str_chars_append(&tmp,"if(v2 && !strcmp(v2->data.d_string.content,\"",44);
            append_escaped_string(&tmp,tok1->data);
            str_chars_append(&tmp,"\")) {\n",6);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v2 && v2->temporary) cf_tpl_var_destroy(v2);\n",48);
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
            str_chars_append(&tmp,"v1 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v1,TPL_VARIABLE_INT);\n",72);
            str_chars_append(&tmp,"if(v1 && v1->data.d_int == iter_var2) {\n",40);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v1 && v1->temporary) cf_tpl_var_destroy(v1);\n",48);
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_chars_append(&tmp,"if(iter_var1 == iter_var2) {\n",29);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            break;
          case PARSETPL_TOK_INTEGER:
            str_chars_append(&tmp,"if(",3);
            str_str_append(&tmp,tok1->data);
            str_chars_append(&tmp," == iter_var2) {\n",17);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            break;
          case PARSETPL_TOK_STRING:
            str_chars_append(&tmp,"if(strtol(\"",11);
            append_escaped_string(&tmp,tok1->data);
            str_chars_append(&tmp,"\",NULL,10) == iter_var2) {\n",27);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            break;
          default:
            str_chars_append(&tmp,"cmp_res = 0;\n",13);
            break;
        }
        break;
      case PARSETPL_TOK_STRING:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to string
            str_chars_append(&tmp,"v1 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v1,TPL_VARIABLE_STRING);\n",75);
            str_chars_append(&tmp,"if(v1 && !strcmp(v1->data.d_string.content,\"",44);
            append_escaped_string(&tmp,tok2->data);
            str_chars_append(&tmp,"\")) {\n",6);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v1 && v1->temporary) cf_tpl_var_destroy(v1);\n",48);
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_chars_append(&tmp,"if(strtol(\"",11);
            append_escaped_string(&tmp,tok2->data);
            str_chars_append(&tmp,"\",NULL,10) == iter_var1) {\n",27);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            break;
          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(!strcmp(tok1->data->content,tok2->data->content)) {
              if(compop->content[0] == '!') {
                str_chars_append(&tmp,"cmp_res = 0;\n",13);
              } else {
                str_chars_append(&tmp,"cmp_res = 1;\n",13);
              }
            } else {
              if(compop->content[0] == '!') {
                str_chars_append(&tmp,"cmp_res = 1;\n",13);
              } else {
                str_chars_append(&tmp,"cmp_res = 0;\n",13);
              }
            }
            break;
          default:
            str_chars_append(&tmp,"cmp_res = 0;\n",13);
            break;
        }
        break;
      case PARSETPL_TOK_INTEGER:
        switch(type1) {
          case PARSETPL_TOK_VARIABLE:
            // convert it to integer
            str_chars_append(&tmp,"v1 = (t_cf_tpl_variable *)cf_tpl_var_convert(NULL,v1,TPL_VARIABLE_INT);\n",72);
            str_chars_append(&tmp,"if(v1 && v1->data.d_int == ",27);
            str_str_append(&tmp,tok2->data);
            str_chars_append(&tmp,") {\n",4);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            str_chars_append(&tmp,"if(v1 && v1->temporary) cf_tpl_var_destroy(v1);\n",48);
            break;
          case PARSETPL_TOK_LOOPVAR:
            str_chars_append(&tmp,"if(",3);
            str_str_append(&tmp,tok2->data);
            str_chars_append(&tmp," == iter_var1) {\n",17);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            }
            str_chars_append(&tmp,"} else {\n",9);
            if(compop->content[0] == '!') {
              str_chars_append(&tmp,"cmp_res = 1;\n",13);
            } else {
              str_chars_append(&tmp,"cmp_res = 0;\n",13);
            }
            str_chars_append(&tmp,"}\n",2);
            break;
          case PARSETPL_TOK_STRING:
          case PARSETPL_TOK_INTEGER:
            // do compare here
            if(strtol(tok1->data->content,NULL,10) == strtol(tok2->data->content,NULL,10)) {
              if(compop->content[0] == '!') {
                str_chars_append(&tmp,"cmp_res = 0;\n",13);
              } else {
                str_chars_append(&tmp,"cmp_res = 1;\n",13);
              }
            } else {
              if(compop->content[0] == '!') {
                str_chars_append(&tmp,"cmp_res = 1;\n",13);
              } else {
                str_chars_append(&tmp,"cmp_res = 0;\n",13);
              }
            }
            break;
          default:
            str_chars_append(&tmp,"cmp_res = 0;\n",13);
            break;
        }
        break;
      default:
        str_chars_append(&tmp,"cmp_res = 0;\n",13);
        break;
    }
    str_chars_append(&tmp,"if(cmp_res) {\n",14);
    str_str_append(&output,&tmp);
    str_str_append(&output_mem,&tmp);
    str_cleanup(&tmp);
    str_cleanup(compop); free(compop);
    if(tok1) {
      destroy_token(tok1); free(tok1);
    }
    if(tok2) {
      destroy_token(tok2); free(tok2);
    }
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

int parse_file(const u_char *filename) {
  u_char *basename, *p;
  t_string output_name;
  FILE *ifp, *ofp;
  int ret;
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
  array_init(&foreach_var_stack,sizeof(t_string),str_cleanup);
  
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
  fprintf(ofp,"void parse(t_cf_template *tpl) {\n\nt_cf_tpl_variable *v = NULL;\nt_cf_tpl_variable *nv = NULL;\nu_char *tmp = NULL;\n\n");
  fprintf(ofp,"%s\n}\n\n",output.content);
  fprintf(ofp,"void parse_to_mem(t_cf_template *tpl) {\n\nt_cf_tpl_variable *v = NULL;\nt_cf_tpl_variable *nv = NULL;\nu_char *tmp = NULL;\n\n");
  fprintf(ofp,"%s\n}\n\n",output_mem.content);
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
