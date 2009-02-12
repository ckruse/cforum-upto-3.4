/**
 * \file parsetpl.lex
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * The template parser
 */
%{
#include "cfconfig.h"
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
cf_array_t *defined_function_list = NULL;
context_t  *current_context    = NULL;
cf_string_t   string              = CF_STRING_INITIALIZER;
cf_string_t   content             = CF_STRING_INITIALIZER;
cf_string_t   content_backup      = CF_STRING_INITIALIZER;
cf_string_t   current_file        = CF_STRING_INITIALIZER;
context_t  global_context;

%}

/* states */
%x TAG STRING

%option stack noyywrap

%%

\n   {
  ++lineno; /* count line numbers */
  if (!current_context->iws && !current_context->nle) {
    cf_str_char_append(&content,'\n');
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
  cf_str_init(&content_backup);
  cf_str_chars_append(&content_backup,yytext,yyleng);
  return PARSETPL_TOK_TAGSTART;
}

<TAG>{
  \n                  {
    if(!current_context->iws && !current_context->nle) {
      cf_str_chars_append(&content_backup,yytext,yyleng);
      ++lineno;
      return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
    }
  }

  [\t\r ]             {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_WHITESPACE;
  }

  \"             {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    if(string.content) free(string.content);
    cf_str_init(&string);
    yy_push_state(STRING);
  }

  -?[0-9]+            {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_INTEGER;
  }

  \$[A-Za-z0-9_.-]+     {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_VARIABLE;
  }

  @[A-Za-z0-9_.-]+     {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_LOOPVAR;
  }

  \}             {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    yy_pop_state();
    if(content.content) free(content.content);
    cf_str_init(&content);
    return PARSETPL_TOK_TAGEND;
  }

  =>                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_HASHASSIGNMENT;
  }

  \+                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_CONCAT;
  }

  ->html            {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_MODIFIER_ESCAPE;
  }

  ==                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_COMPARE;
  }

  !=                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_COMPARE;
  }

  =                   {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ASSIGNMENT;
  }

  \[                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYSTART;
  }

  ,                   {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYSEP;
  }

  \]                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ARRAYEND;
  }

  \(                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_PARAMS_START;
  }

  \)                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_PARAMS_END;
  }

  !                   {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NOT;
  }

  else?[ ]?if         {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ELSIF;
  }

  if                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IF;
  }

  else                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ELSE;
  }

  endif               {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ENDIF;
  }

  include             {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_INCLUDE;
  }

  foreach             {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FOREACH;
  }

  as                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_AS;
  }

  endforeach          {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_ENDFOREACH;
  }

  iws                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IWS_START;
  }

  endiws              {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_IWS_END;
  }

  nle                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NLE_START;
  }

  endnle              {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_NLE_END;
  }

  def                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC;
  }

  enddef              {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC_END;
  }

  call                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_TOK_FUNC_CALL;
  }

  <<EOF>>        {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_ERR_UNTERMINATEDTAG;
  }

  .                   {
    //cf_str_chars_append(&content_backup,yytext,yyleng);
    unput(*yytext);
    return PARSETPL_ERR_UNRECOGNIZEDCHARACTER;
  }
}

<STRING>{
  \n                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    ++lineno; 
    return PARSETPL_ERR_UNTERMINATEDSTRING;
  }

  \\n                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,'\n');
  }

  \\t                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,'\t');
  }

  \\r                 {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,'\r');
  }

  \\0[0-7]{1,3}       {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,(char)strtol(yytext,NULL,8));
  }

  \\x[0-9A-Fa-f]{1,2} {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,(char)strtol(yytext,NULL,16));
  }

  \\\\                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,'\\');
  }

  \\\"                {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,'"');
  }

  \"                  {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    yy_pop_state();
    return PARSETPL_TOK_STRING;
  }

  <<EOF>> {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    return PARSETPL_ERR_UNTERMINATEDSTRING;
  }

  .                   {
    cf_str_chars_append(&content_backup,yytext,yyleng);
    cf_str_char_append(&string,*yytext);
  }
}

[ \r\t] {
  if (!current_context->iws) cf_str_char_append(&content,*yytext);
}

. {
  cf_str_char_append(&content,*yytext);
}

%%

/* {{{ parse_file */
int parse_file(const u_char *filename) {
  u_char *basename, *p;
  cf_string_t output_name;
  cf_string_t tmp;
  FILE *ifp, *ofp;
  int ret;
  long i;
  char buf[20];
  cf_array_t data;
  int tag_started = 0;
  token_t token;
  
  basename = strdup((u_char *)filename);
  if(!strcmp(PARSETPL_INCLUDE_EXT,basename+strlen(basename)-strlen(PARSETPL_INCLUDE_EXT))) basename[strlen(basename)-strlen(PARSETPL_INCLUDE_EXT)] = '\0';
  
  cf_str_init(&output_name);
  cf_str_init(&current_file); // global variable
  cf_str_char_set(&output_name,basename,strlen(basename));
  cf_str_chars_append(&output_name,".c",2);
  p = strrchr(basename,'/');

  if(!p) p = basename;
  else p++;

  cf_str_char_set(&current_file,p,strlen(p));
  free(basename);
  
  current_context = &global_context;
  init_context(current_context);
  defined_functions = cf_hash_new(destroy_function);
  defined_function_list = cf_alloc(NULL,sizeof(cf_array_t),1,CF_ALLOC_MALLOC);
  cf_array_init(defined_function_list,sizeof(function_t*),NULL);
  
  // now open input file
  if((ifp = fopen(filename,"rb")) == NULL) {
    fprintf(stderr,"Error opening %s for reading\n", filename);
    cf_str_cleanup(&output_name);
    cf_str_cleanup(&current_file);
    return -1;
  }

  // open output file
  if((ofp = fopen(output_name.content,"wb")) == NULL) {
    fprintf(stderr,"Error opening %s for writing\n", output_name.content);
    fclose(ifp);
    cf_str_cleanup(&output_name);
    cf_str_cleanup(&current_file);
    return -1;
  }

  cf_str_cleanup(&output_name); // we don't need it anymore
  
  yyin = ifp;
  cf_str_init(&content);
  cf_array_init(&data,sizeof(token_t),destroy_token);
  
  do {
    ret = parsetpl_lex();

    if(ret < PARSETPL_TOK_EOF) {
      if(!tag_started) break; // error

      yy_pop_state();

      if(content.content) free(content.content);

      cf_str_init(&content);
      cf_str_str_append(&content,&content_backup);
      cf_array_destroy(&data);
      cf_array_init(&data,sizeof(token_t),destroy_token);
      tag_started = 0;
      ret = PARSETPL_TOK_TAGEND; // dummy

      continue;
    }

    if(tag_started) {
      if(ret == PARSETPL_TOK_TAGEND) {
        ret = process_tag(&data);

        if(ret < PARSETPL_TOK_EOF) cf_str_str_append(&content,&content_backup); // no error, append content backup

        cf_array_destroy(&data);
        cf_array_init(&data,sizeof(token_t),destroy_token);
        tag_started = 0;
      }
      else {
        token.type = ret;
        token.data = cf_alloc(NULL,sizeof(cf_string_t),1,CF_ALLOC_MALLOC);
        cf_str_init(token.data);

        if(ret == PARSETPL_TOK_STRING) cf_str_str_set(token.data,&string);
        else cf_str_char_set(token.data,yytext,yyleng);

        cf_array_push(&data,&token);
      }

      // dummy: so that the loop doesn't end
      ret = 1;
      continue;
    }

    // a new tag starts or EOF
    if(content.len) {
      cf_str_chars_append(&current_context->output,"my_write(\"",10);
      cf_str_chars_append(&current_context->output_mem,"cf_str_chars_append(&tpl->parsed,\"",34);
      append_escaped_string(&current_context->output,&content);
      append_escaped_string(&current_context->output_mem,&content);
      cf_str_chars_append(&current_context->output,"\");\n",4);
      cf_str_chars_append(&current_context->output_mem,"\",", 2);
      snprintf(buf,19,"%ld",content.len);
      cf_str_chars_append(&current_context->output_mem,buf,strlen(buf));
      cf_str_chars_append(&current_context->output_mem,");\n",3);
    }

    if(ret == PARSETPL_TOK_TAGSTART) tag_started = 1;
  } while(ret > PARSETPL_TOK_EOF);
  
  fclose(ifp); // we don't need it any more
  
  if(ret < 0 || current_context != &global_context) {
    fprintf(stderr,"Error processing %s: %d\n", filename, ret);
    cf_str_cleanup(&content);
    cf_str_cleanup(&current_file);
    cf_array_destroy(&data);
    destroy_context(current_context);
    cf_hash_destroy(defined_functions);
    cf_array_destroy(defined_function_list);
    free(defined_function_list);
    fclose(ofp);
    return ret;
  }
  
  fprintf(ofp, "/*\n * this is an automatically generated template file, generated by cforum template parser v %s\n *\n */\n\n" \
    "#include \"cfconfig.h\"\n"\
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
    function_t **func = (function_t **)cf_array_element_at(defined_function_list,i);
    cf_str_init(&tmp);
    cf_str_cstr_append(&tmp,"tpl_func_");
    cf_str_str_append(&tmp,&(*func)->name);
    write_parser_functions_def(ofp, &tmp, (*func)->ctx, &(*func)->params);
    cf_str_cleanup(&tmp);
  }

  for(i = 0; (size_t)i < defined_function_list->elements; i++) {
    function_t **func = (function_t **)cf_array_element_at(defined_function_list,i);
    cf_str_init(&tmp);
    cf_str_cstr_append(&tmp,"tpl_func_");
    cf_str_str_append(&tmp,&(*func)->name);
    write_parser_functions(ofp, &tmp, (*func)->ctx, &(*func)->params);
    cf_str_cleanup(&tmp);
  }

  cf_str_init(&tmp);
  cf_str_cstr_append(&tmp,"parse");
  write_parser_functions(ofp, &tmp, current_context, NULL);
  cf_str_cleanup(&tmp);
  
  fclose(ofp);
  cf_str_cleanup(&content);
  cf_str_cleanup(&current_file);
  cf_array_destroy(&data);
  cf_hash_destroy(defined_functions);
  cf_array_destroy(defined_function_list);
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
