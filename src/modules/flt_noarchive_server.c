/**
 * \file flt_noarchive_server.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Returns FLT_EXIT if the thread has the no-archive flag
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

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverutils.h"
#include "serverlib.h"
#include "fo_server.h"
/* }}} */

int flt_noarchive_server_exec(t_forum *forum,t_thread *thread) {
  t_posting_flag *flag = cf_get_flag_by_name(&thread->postings->flags,"no-archive");
  if(flag) return FLT_EXIT;

  return FLT_DECLINE;
}

t_conf_opt flt_noarchive_server_config[] = {
  { NULL, NULL, 0, NULL }
};

t_handler_config flt_noarchive_server_handlers[] = {
  { ARCHIVE_HANDLER, flt_noarchive_server_exec },
  { 0, NULL }
};

t_module_config flt_noarchive_server = {
  MODULE_MAGIC_COOKIE,
  flt_noarchive_server_config,
  flt_noarchive_server_handlers,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

/* eof */
