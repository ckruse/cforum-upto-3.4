/**
 * \file flt_admin_mysql.c
 * \author Christian Kruse, <cjk@wwwtech.de>
 *
 * This plugin checks if a user is admin with MySQL as storage
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
#include "cfconfig.h"
#include "defines.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <mysql.h>

#include "hashlib.h"
#include "utils.h"
#include "cfgcomp.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

static u_char *flt_amsql_host      = NULL;
static u_char *flt_amsql_user      = NULL;
static u_char *flt_amsql_passwd    = NULL;
static u_char *flt_amsql_db        = NULL;
static u_char *flt_amsql_socket    = NULL;
static u_char *flt_amsql_table     = NULL;
static u_char *flt_amsql_field     = NULL;
static u_char *flt_amsql_wfield    = NULL;

static unsigned int flt_amsql_port = 3306;

static u_char *flt_amsql_fn = NULL;
static MYSQL *flt_amsql_dbh = NULL;

/* {{{ flt_admin_mysql_init */
int flt_admin_mysql_init(cf_hash_t *cgi,cf_configuration_t *dc,cf_configuration_t *vc) {
  MYSQL *ret;
  MYSQL_RES *result;
  MYSQL_ROW row;

  cf_string_t query;
  u_char *buff,*uname = cf_hash_get(GlobalValues,"UserName",8);
  unsigned int len;

  if(!uname) return FLT_DECLINE;

  /* {{{ create connection */
  if((flt_amsql_dbh = mysql_init(NULL)) == NULL) return FLT_EXIT;
  mysql_options(flt_amsql_dbh,MYSQL_READ_DEFAULT_GROUP,"cforum");

  if((ret = mysql_real_connect(flt_amsql_dbh,flt_amsql_host,flt_amsql_user,flt_amsql_passwd,flt_amsql_db,flt_amsql_port,flt_amsql_socket,0)) == NULL) return FLT_EXIT;
  /* }}} */

  /* {{{ create query */
  cf_str_init_growth(&query,128);
  cf_str_char_set(&query,"SELECT ",7);
  cf_str_chars_append(&query,flt_amsql_field,strlen(flt_amsql_field));
  cf_str_chars_append(&query," FROM ",6);
  cf_str_chars_append(&query,flt_amsql_table,strlen(flt_amsql_table));
  cf_str_chars_append(&query," WHERE ",7);
  cf_str_chars_append(&query,flt_amsql_wfield,strlen(flt_amsql_wfield));
  cf_str_chars_append(&query," = '",4);

  len  = strlen(uname);
  buff = cf_alloc(NULL,1,len * 2 + 1,CF_ALLOC_MALLOC);
  len  = mysql_real_escape_string(flt_amsql_dbh,buff,uname,len);

  cf_str_chars_append(&query,buff,len);
  free(buff);

  cf_str_char_append(&query,'\'');
  /* }}} */

  if(mysql_real_query(flt_amsql_dbh,query.content,query.len) != 0) {
    cf_str_cleanup(&query);
    return FLT_DECLINE;
  }

  cf_str_cleanup(&query);

  if((result = mysql_store_result(flt_amsql_dbh)) == NULL) return FLT_DECLINE;

  if((row = mysql_fetch_row(result)) == NULL) {
    mysql_free_result(result);
    return FLT_DECLINE;
  }

  if(row[0] == NULL) {
    mysql_free_result(result);
    return FLT_DECLINE;
  }

  if(atoi(row[0]) != 0 || cf_strcmp(row[0],"admin") == 0) cf_hash_set(GlobalValues,"is_admin",8,"1",1);
  mysql_free_result(result);

  return FLT_OK;
}
/* }}} */

/* {{{ flt_admin_mysql_handle */
int flt_admin_mysql_handle(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  if(flt_amsql_fn == NULL) flt_amsql_fn = cf_hash_get(GlobalValues,"FORUM_NAME",10);
  if(!context || cf_strcmp(flt_amsql_fn,context) != 0) return 0;

  if(cf_strcmp(opt->name,"MysqlHost") == 0) {
    if(flt_amsql_host) free(flt_amsql_host);
    flt_amsql_host = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlUser") == 0) {
    if(flt_amsql_user) free(flt_amsql_user);
    flt_amsql_user = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlPasswd") == 0) {
    if(flt_amsql_passwd) free(flt_amsql_passwd);
    flt_amsql_passwd = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlDb") == 0) {
    if(flt_amsql_db) free(flt_amsql_db);
    flt_amsql_db = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlSocket") == 0) {
    if(flt_amsql_socket) free(flt_amsql_socket);
    flt_amsql_socket = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlTable") == 0) {
    if(flt_amsql_table) free(flt_amsql_table);
    flt_amsql_table = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlField") == 0) {
    if(flt_amsql_field) free(flt_amsql_field);
    flt_amsql_field = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlWhereField") == 0) {
    if(flt_amsql_wfield) free(flt_amsql_wfield);
    flt_amsql_wfield = strdup(args[0]);
  }
  else if(cf_strcmp(opt->name,"MysqlPort") == 0) flt_amsql_port = atoi(args[0]);

  return 0;
}
/* }}} */

/* {{{ flt_admin_mysql_cleanup */
void flt_admin_mysql_cleanup(void) {
  if(flt_amsql_dbh) mysql_close(flt_amsql_dbh);

  if(flt_amsql_host)   free(flt_amsql_host);
  if(flt_amsql_user)   free(flt_amsql_user);
  if(flt_amsql_passwd) free(flt_amsql_passwd);
  if(flt_amsql_db)     free(flt_amsql_db);
  if(flt_amsql_socket) free(flt_amsql_socket);
  if(flt_amsql_table)  free(flt_amsql_table);
  if(flt_amsql_field)  free(flt_amsql_field);
  if(flt_amsql_wfield) free(flt_amsql_wfield);
}
/* }}} */

cf_conf_opt_t flt_admin_mysql_config[] = {
  { "MysqlHost",       flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "MysqlUser",       flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "MysqlPasswd",     flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "MysqlDb",         flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_NEEDED, NULL },
  { "MysqlSocket",     flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },
  { "MysqlTable",      flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_NEEDED, NULL },
  { "MysqlField",      flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_NEEDED, NULL },
  { "MysqlWhereField", flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_NEEDED, NULL },
  { "MysqlPort",       flt_admin_mysql_handle, CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, NULL },

  { NULL, NULL, 0, NULL }
};

cf_handler_config_t flt_admin_mysql_handlers[] = {
  { INIT_HANDLER,  flt_admin_mysql_init },
  { 0, NULL }
};

cf_module_config_t flt_admin_mysql = {
  MODULE_MAGIC_COOKIE,
  flt_admin_mysql_config,
  flt_admin_mysql_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_admin_mysql_cleanup
};


/* eof */
