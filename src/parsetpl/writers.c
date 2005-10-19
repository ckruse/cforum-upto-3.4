/**
 * \file helpers.c
 * \author Christian Seiler, <self@christian-seiler.de>
 *
 * Helper functions for the template parser
 */

#include "config.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "utils.h"
#include "hashlib.h"
#include "parsetpl.h"

/* {{{ write_parser_functions_def */
void write_parser_functions_def(FILE *ofp, cf_string_t *func_name, context_t *ctx, cf_array_t *params) {
  size_t i;

  fprintf(ofp,"void %s(cf_template_t *%stpl", func_name->content, (params ? "o" : ""));

  if(params) {
    for(i = 0;i < params->elements; i++) fprintf(ofp,", cf_tpl_variable_t *p%u", i);
  }

  fprintf(ofp,");\n");
  fprintf(ofp,"void %s_to_mem(cf_template_t *%stpl", func_name->content, (params ? "o" : ""));

  if(params) {
    for(i = 0; i < params->elements; i++) fprintf(ofp,", cf_tpl_variable_t *p%u", i);
  }

  fprintf(ofp,");\n");
}
/* }}} */

/* {{{ write_parser_functions */
void write_parser_functions(FILE *ofp, cf_string_t *func_name, context_t *ctx, cf_array_t *params) {
  long i;
  cf_string_t *s;
  cf_string_t tmp;
  
  fprintf(ofp,"void %s(cf_template_t *%stpl", func_name->content, (params ? "o" : ""));
  if(params) {
    for(i = 0; (size_t)i < params->elements; i++) fprintf(ofp,", cf_tpl_variable_t *p%ld", i);
  }

  fprintf(ofp,") {\ncf_tpl_variable_t *v = NULL;\n");

  if(params) {
    fprintf(ofp,"int is_tmp_var = 0;");
    fprintf(ofp,"int is_arf_var = 0;");
  }

  if(ctx->uses_print)      fprintf(ofp,"cf_tpl_variable_t *vp = NULL;\n");
  if(ctx->uses_clonevar)   fprintf(ofp,"cf_tpl_variable_t *vc = NULL;\n");
  if(ctx->uses_loopassign) fprintf(ofp,"long ic = 0;\n");
  if(ctx->uses_tmpstring)  fprintf(ofp,"cf_string_t tmp_string;\n");
  if(ctx->uses_iter_print) fprintf(ofp,"long iter_var = 0;\n");

  fprintf(ofp,"long cmp_res = 0;\n");

  for(i = 0;i < ctx->n_assign_vars;i++) fprintf(ofp,"cf_tpl_variable_t *va%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_if_vars;i++)     fprintf(ofp,"cf_tpl_variable_t *vi%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_if_iters;i++)    fprintf(ofp,"long ii%ld = 0;\n",i);
  for(i = 0;i < ctx->n_call_vars;i++)   fprintf(ofp,"cf_tpl_variable_t *vfc%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_call_iters;i++)  fprintf(ofp,"long ifc%ld = 0;\n",i);

  for(i = 0;i < ctx->n_foreach_vars;i++) {
    fprintf(ofp,"cf_tpl_variable_t *vf%ld = NULL;\n",i*2);
    fprintf(ofp,"cf_tpl_variable_t *vf%ld = NULL;\n",i*2+1);
    fprintf(ofp,"int i%ld = 0;\n",i);
  }

  if(ctx->uses_include) {
    fprintf(ofp,"cf_template_t *inc_tpl;\n");
    fprintf(ofp,"cf_string_t *inc_filename, *inc_filepart, *inc_fileext;\n");
    fprintf(ofp,"u_char *p;\n");
    fprintf(ofp,"cf_hash_t *ov;\n");
    fprintf(ofp,"int ret;\n");
  }

  if(params) {
    fprintf(ofp,"cf_template_t *tpl = cf_alloc(NULL,sizeof(cf_template_t),1,CF_ALLOC_MALLOC);\n");
    fprintf(ofp,"if (cf_tpl_init(tpl,NULL)) return;\n");
    fprintf(ofp,"cf_str_cleanup(&tpl->parsed);\n");
    fprintf(ofp,"memcpy(&tpl->parsed,&otpl->parsed,sizeof(cf_string_t));\n");
    fprintf(ofp,"cf_tpl_copyvars(tpl,otpl,0);\n");

    for(i=0;(size_t)i<params->elements;i++) {
      s = (cf_string_t *)cf_array_element_at(params,i);
      cf_str_init(&tmp);
      append_escaped_string(&tmp,s);
      fprintf(ofp,"if(p%ld) {\n", i);
      fprintf(ofp,"is_arf_var = p%ld->arrayref;\n",i);
      fprintf(ofp,"if(!p%ld->temporary) {\nis_tmp_var = 1; p%ld->arrayref = 1;\n}\n", i, i);
      fprintf(ofp,"cf_tpl_setvar(tpl,\"%s\",p%ld);\n", tmp.content+1, i);
      fprintf(ofp,"if(is_tmp_var) {\nis_tmp_var = 0; p%ld->arrayref = is_arf_var;\n}\n", i);
      fprintf(ofp,"}\n");
      cf_str_cleanup(&tmp);
    }
  }

  fprintf(ofp,"\n%s\n",ctx->output.content);

  if(params) {
    fprintf(ofp,"memcpy(&otpl->parsed,&tpl->parsed,sizeof(cf_string_t));\n");
    fprintf(ofp,"cf_str_init(&tpl->parsed); cf_tpl_finish(tpl);\n");
  }

  fprintf(ofp,"}\n\n");
  fprintf(ofp,"void %s_to_mem(cf_template_t *%stpl", func_name->content, (params ? "o" : ""));

  if(params) {
    for(i = 0; (size_t)i < params->elements; i++) fprintf(ofp,", cf_tpl_variable_t *p%ld", i);
  }

  fprintf(ofp,") {\ncf_tpl_variable_t *v = NULL;\n");

  if(params) {
    fprintf(ofp,"int is_tmp_var = 0;");
    fprintf(ofp,"int is_arf_var = 0;");
  }

  if(ctx->uses_print) {
    fprintf(ofp,"cf_tpl_variable_t *vp = NULL;\n");
    fprintf(ofp,"u_char *tmp = NULL;\n");
  }

  if(ctx->uses_iter_print) fprintf(ofp,"long iter_var = 0;\n");
  if(ctx->uses_clonevar)   fprintf(ofp,"cf_tpl_variable_t *vc = NULL;\n");
  if(ctx->uses_loopassign) fprintf(ofp,"long ic = 0;\n");
  if(ctx->uses_tmpstring)  fprintf(ofp,"cf_string_t tmp_string;\n");

  fprintf(ofp,"long cmp_res = 0;\n");
  fprintf(ofp,"char iter_buf[20];\n");

  for(i = 0;i < ctx->n_assign_vars;i++) fprintf(ofp,"cf_tpl_variable_t *va%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_if_vars;i++)     fprintf(ofp,"cf_tpl_variable_t *vi%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_if_iters;i++)    fprintf(ofp,"long ii%ld = 0;\n",i);
  for(i = 0;i < ctx->n_call_vars;i++)   fprintf(ofp,"cf_tpl_variable_t *vfc%ld = NULL;\n",i);
  for(i = 0;i < ctx->n_call_iters;i++)  fprintf(ofp,"long ifc%ld = 0;\n",i);

  for(i = 0;i < ctx->n_foreach_vars;i++) {
    fprintf(ofp,"cf_tpl_variable_t *vf%ld = NULL;\n",i*2);
    fprintf(ofp,"cf_tpl_variable_t *vf%ld = NULL;\n",i*2+1);
    fprintf(ofp,"int i%ld = 0;\n",i);
  }

  if(ctx->uses_include) {
    fprintf(ofp,"cf_template_t *inc_tpl;\n");
    fprintf(ofp,"cf_string_t *inc_filename, *inc_filepart, *inc_fileext;\n");
    fprintf(ofp,"u_char *p;\n");
    fprintf(ofp,"cf_hash_t *ov;\n");
    fprintf(ofp,"int ret;\n");
  }

  if(params) {
    fprintf(ofp,"cf_template_t *tpl = cf_alloc(NULL,sizeof(cf_template_t),1,CF_ALLOC_MALLOC);\n");
    fprintf(ofp,"if (cf_tpl_init(tpl,NULL)) return;\n");
    fprintf(ofp,"cf_str_cleanup(&tpl->parsed);\n");
    fprintf(ofp,"memcpy(&tpl->parsed,&otpl->parsed,sizeof(cf_string_t));\n");
    fprintf(ofp,"cf_tpl_copyvars(tpl,otpl,0);\n");

    for(i = 0; (size_t)i < params->elements; i++) {
      s = (cf_string_t *)cf_array_element_at(params,i);
      cf_str_init(&tmp);
      append_escaped_string(&tmp,s);
      fprintf(ofp,"if(p%ld) {\n", i);
      fprintf(ofp,"is_arf_var = p%ld->arrayref;\n",i);
      fprintf(ofp,"if(!p%ld->temporary) {\nis_tmp_var = 1; p%ld->arrayref = 1;\n}\n", i, i);
      fprintf(ofp,"cf_tpl_setvar(tpl,\"%s\",p%ld);\n", tmp.content+1, i);
      fprintf(ofp,"if(is_tmp_var) {\nis_tmp_var = 0; p%ld->arrayref = is_arf_var;\n}\n", i);
      fprintf(ofp,"}\n");
      cf_str_cleanup(&tmp);
    }
  }

  fprintf(ofp,"\n%s\n",ctx->output_mem.content);

  if(params) {
    fprintf(ofp,"memcpy(&otpl->parsed,&tpl->parsed,sizeof(cf_string_t));\n");
    fprintf(ofp,"cf_str_init(&tpl->parsed); cf_tpl_finish(tpl);\n");
  }

  fprintf(ofp,"}\n\n");
}
/* }}} */

/* eof */
