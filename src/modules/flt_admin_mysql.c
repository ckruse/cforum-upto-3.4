/**
 * \file flt_admin_mysql.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
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
#include "config.h"
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
#include "configparser.h"
#include "cfcgi.h"
#include "template.h"
#include "readline.h"
#include "clientlib.h"
/* }}} */

t_conf_opt flt_admin_mysql_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_admin_mysql_handlers[] = {
  { 0, NULL }
};

t_module_config flt_admin_mysql = {
  flt_admin_mysql_config,
  flt_admin_mysql_handlers,
  NULL,
  NULL,
  NULL,
  NULL
};


/* eof */
