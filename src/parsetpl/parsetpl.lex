/**
 * \file parsetpl.lex
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * The template parser
 */
%{
#include "config.h"
#include "defines.h"

#include "utils.h"
#include "hashlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "parsetpl.h"

long lineno                    = 0;
cf_hash_t  *defined_functions  = NULL;
array_t *defined_function_list = NULL;
context_t  *current_context    = NULL;
string_t   string              = STRING_INITIALIZER;
string_t   content             = STRING_INITIALIZER;
string_t   content_backup      = STRING_INITIALIZER;
string_t   current_file        = STRING_INITIALIZER;
context_t  global_context;

%}

/* states */
%x TAG STRING

%option stack noyywrap

%%

\n   {
  ++lineno; /* count line numbers */
  if (!current_context->iws && !current_context->nle) {
    str_char_append(&content,'\n');
  }
}

"{*"    { /* comment */
  register int c;

  while((c = input()) != '}' && c != EOF) ;    /* eat up text of comment */

  if(c != '}') return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
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
    if(!current_context->iws && !current_context->nle) {
      str_chars_append(&content_backup,yytext,yyleng);
      ++lineno;
      return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
    }
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

  \$[A-Za-z0-9_.]+     {
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

  \(                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_PARAMS_START;
  }

  \)                  {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_PARAMS_END;
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

  iws                 {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IWS_START;
  }

  endiws              {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IWS_END;
  }

  nle                 {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NLE_START;
  }

  endnle              {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NLE_END;
  }

  def                 {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC;
  }

  enddef              {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC_END;
  }

  call                {
    str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC_CALL;
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

[ \r\t] {
  if (!current_context->iws) str_char_append(&content,*yytext);
}

. {
  str_char_append(&content,*yytext);
}

%%

/* {{{ parse_file */
int parse_file(const u_char *filename) {
  u_char *basename, *p;
  string_t output_name;
  string_t tmp;
  FILE *ifp, *ofp;
  int ret;
  long i;
  char buf[20];
  array_t data;
  int tag_started = 0;
  token_t token;
  
  basename = strdup((u_char *)filename);
  if(!strcmp(PARSETPL_INCLUDE_EXT,basename+strlen(basename)-strlen(PARSETPL_INCLUDE_EXT))) basename[strlen(basename)-strlen(PARSETPL_INCLUDE_EXT)] = '\0';
  
  str_init(&output_name);
  str_init(&current_file); // global variable
  str_char_set(&output_name,basename,strlen(basename));
  str_chars_append(&output_name,".c",2);
  p = strrchr(basename,'/');

  if(!p) p = basename;
  else p++;

  str_char_set(&current_file,p,strlen(p));
  free(basename);
  
  current_context = &global_context;
  init_context(current_context);
  defined_functions = cf_hash_new(destroy_function);
  defined_function_list = fo_alloc(NULL,sizeof(array_t),1,FO_ALLOC_MALLOC);
  array_init(defined_function_list,sizeof(function_t*),NULL);
  
  // now open input file
  if((ifp = fopen(filename,"rb")) == NULL) {
    fprintf(stderr,"Error opening %s for reading\n", filename);
    str_cleanup(&output_name);
    str_cleanup(&current_file);
    return -1;
  }

  // open output file
  if((ofp = fopen(output_name.content,"wb")) == NULL) {
    fprintf(stderr,"Error opening %s for writing\n", output_name.content);
    fclose(ifp);
    str_cleanup(&output_name);
    str_cleanup(&current_file);
    return -1;
  }

  str_cleanup(&output_name); // we don't need it anymore
  
  yyin = ifp;
  str_init(&content);
  array_init(&data,sizeof(token_t),destroy_token);
  
  do {
    ret = parsetpl_lex();

    if(ret < PARSETPL_TOK_EOF) {
      if(!tag_started) break; // error

      yy_pop_state();

      if(content.content) free(content.content);

      str_init(&content);
      str_str_append(&content,&content_backup);
      array_destroy(&data);
      array_init(&data,sizeof(token_t),destroy_token);
      tag_started = 0;
      ret = PARSETPL_TOK_TAGEND; // dummy

      continue;
    }

    if(tag_started) {
      if(ret == PARSETPL_TOK_TAGEND) {
        ret = process_tag(&data);

        if(ret < PARSETPL_TOK_EOF) str_str_append(&content,&content_backup); // no error, append content backup

        array_destroy(&data);
        array_init(&data,sizeof(token_t),destroy_token);
        tag_started = 0;
      }
      else {
        token.type = ret;
        token.data = fo_alloc(NULL,sizeof(string_t),1,FO_ALLOC_MALLOC);
        str_init(token.data);

        if(ret == PARSETPL_TOK_STRING) str_str_set(token.data,&string);
        else str_char_set(token.data,yytext,yyleng);

        array_push(&data,&token);
      }

      // dummy: so that the loop doesn't end
      ret = 1;
      continue;
    }

    // a new tag starts or EOF
    if(content.len) {
      str_chars_append(&current_context->output,"my_write(\"",10);
      str_chars_append(&current_context->output_mem,"str_chars_append(&tpl->parsed,\"",31);
      append_escaped_string(&current_context->output,&content);
      append_escaped_string(&current_context->output_mem,&content);
      str_chars_append(&current_context->output,"\");\n",4);
      str_chars_append(&current_context->output_mem,"\",", 2);
      snprintf(buf,19,"%ld",content.len);
      str_chars_append(&current_context->output_mem,buf,strlen(buf));
      str_chars_append(&current_context->output_mem,");\n",3);
    }

    if(ret == PARSETPL_TOK_TAGSTART) tag_started = 1;
  } while(ret > PARSETPL_TOK_EOF);
  
  fclose(ifp); // we don't need it any more
  
  if(ret < 0 || current_context != &global_context) {
    fprintf(stderr,"Error processing %s: %d\n", filename, ret);
    str_cleanup(&content);
    str_cleanup(&current_file);
    array_destroy(&data);
    destroy_context(current_context);
    cf_hash_destroy(defined_functions);
    array_destroy(defined_function_list);
    free(defined_function_list);
    fclose(ofp);
    return ret;
  }
  
  fprintf(ofp, "/*\n * this is an automatically generated template file, generated by cforum template parser v %s\n *\n */\n\n" \
    "#include \"config.h\"\n"\
    "#include \"defines.h\"\n\n" \
    "#include <stdio.h>\n" \
    "#include <stdlib.h>\n" \
    "#include <string.h>\n\n" \
    "#include \"utils.h\"\n" \
    "#include \"hashlib.h\"\n" \
    "#include \"charconvert.h\"\n" \
    "#include \"template.h\"\n\n"\
    "static void my_write(const u_char *s) {\n" \
    "  register u_char *ptr;\n\n"\
    "  for(ptr = (u_char *)s;*ptr;ptr++) {\n"\
    "    fputc(*ptr,stdout);\n" \
    "  }\n" \
    "}\n",
    CF_PARSER_VER
  );

  for(i = 0; (size_t)i < defined_function_list->elements; i++) {
    function_t **func = (function_t **)array_element_at(defined_function_list,i);
    str_init(&tmp);
    str_cstr_append(&tmp,"tpl_func_");
    str_str_append(&tmp,&(*func)->name);
    write_parser_functions_def(ofp, &tmp, (*func)->ctx, &(*func)->params);
    str_cleanup(&tmp);
  }

  for(i = 0; (size_t)i < defined_function_list->elements; i++) {
    function_t **func = (function_t **)array_element_at(defined_function_list,i);
    str_init(&tmp);
    str_cstr_append(&tmp,"tpl_func_");
    str_str_append(&tmp,&(*func)->name);
    write_parser_functions(ofp, &tmp, (*func)->ctx, &(*func)->params);
    str_cleanup(&tmp);
  }

  str_init(&tmp);
  str_cstr_append(&tmp,"parse");
  write_parser_functions(ofp, &tmp, current_context, NULL);
  str_cleanup(&tmp);
  
  fclose(ofp);
  str_cleanup(&content);
  str_cleanup(&current_file);
  array_destroy(&data);
  cf_hash_destroy(defined_functions);
  array_destroy(defined_function_list);
  free(defined_function_list);
  destroy_context(current_context);
  return 0;
}
/* }}} */

int main(int argc,char **argv) {
  if(argc != 2) {
    fprintf(stderr,"Usage: %s filename\n",argv[0]);
    return -1;
  }

  return parse_file(argv[1]);
}

/* eof */
