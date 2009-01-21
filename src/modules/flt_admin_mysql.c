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

static MYSQL *flt_amsql_dbh = NULL;

/* {{{ flt_admin_mysql_init */
int flt_admin_mysql_init(cf_hash_t *cgi,cf_configuration_t *cfg) {
  MYSQL *ret;
  MYSQL_RES *result;
  MYSQL_ROW row;

  cf_string_t query;
  u_char *buff,*uname = cf_hash_get(GlobalValues,"UserName",8);
  unsigned int len;

  cf_cfg_config_value_t *host = cf_cfg_get_value(cfg,"MySqlAdmin:Host"),
    *user = cf_cfg_get_value(cfg,"MySqlAdmin:User"),
    *pass = cf_cfg_get_value(cfg,"MySqlAdmin:Password"),
    *db = cf_cfg_get_value(cfg,"MySqlAdmin:Database"),
    *port = cf_cfg_get_value(cfg,"MySqlAdmin:Port"),
    *socket = cf_cfg_get_value(cfg,"MySqlAdmin:Socket");

  if(!uname) return FLT_DECLINE;

  /* {{{ create connection */
  if((flt_amsql_dbh = mysql_init(NULL)) == NULL) return FLT_EXIT;
  mysql_options(flt_amsql_dbh,MYSQL_READ_DEFAULT_GROUP,"cforum");

  if((ret = mysql_real_connect(flt_amsql_dbh,host?host->sval:NULL,user?user->sval:NULL,pass?pass->sval:NULL,db?db->sval:NULL,port?port->ival:0,socket?socket->sval:NULL,0)) == NULL) return FLT_EXIT;
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

/* {{{ flt_admin_mysql_cleanup */
void flt_admin_mysql_cleanup(void) {
  if(flt_amsql_dbh) mysql_close(flt_amsql_dbh);
}
/* }}} */

/**
 * config values:
 * MySqlAdmin:Host = "hostname";
 * MySqlAdmin:User = "user";
 * MySqlAdmin:Password = "password";
 * MySqlAdmin:Database = "database";
 * MySqlAdmin:Socket = "socket";
 * MySqlAdmin:Table = "table";
 * MySqlAdmin:Field = "field";
 * MySqlAdmin:WhereField = "wherefield";
 * MySqlAdmin:Port = <portnum>;
*/

cf_handler_config_t flt_admin_mysql_handlers[] = {
  { INIT_HANDLER,  flt_admin_mysql_init },
  { 0, NULL }
};

cf_module_config_t flt_admin_mysql = {
  MODULE_MAGIC_COOKIE,
  flt_admin_mysql_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  flt_admin_mysql_cleanup
};


/* eof */
