/**
 * \file archiver.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief The archiver functions
 *
 * This file contains the archiver functions. The archiver is complex enough to
 * give him an own file.
 *
 * \todo Archive only if voting is high enough
 * \todo Implement indexer for the "SELFHTML Suche" and the "Neue SELFHTML Suche"
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include <gdome.h>

struct sockaddr_un;

#include "cf_pthread.h"

#include "hashlib.h"
#include "utils.h"
#include "configparser.h"
#include "readline.h"

#include "serverlib.h"
#include "fo_server.h"
#include "archiver.h"
/* }}} */


void cf_run_archiver(void) {
}

void cf_archive_threads(t_forum *forum,t_thread **to_archive,int len) {
  return 0;
}

int cf_archive_thread(t_forum *forum,u_int64_t tid) {
  return 0;
}

/* eof */
