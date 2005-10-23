/**
 * \file configparser.c
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief configuration parser functions and datatypes
 *
 * This file contains the configuration parser used by this project
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
#include <ctype.h>
#include <errno.h>

#include <dlfcn.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include <pwd.h>

#include "utils.h"
#include "hashlib.h"
#include "configparser.h"
#include "charconvert.h"
/* }}} */

/* {{{ globals */

/***************************************** CONFIGURATION OPTIONS ************************************************
 * defining the global variables for configuration
 *
 ***/

cf_configuration_t fo_default_conf;
cf_configuration_t fo_server_conf;
cf_configuration_t fo_view_conf;
cf_configuration_t fo_arcview_conf;
cf_configuration_t fo_post_conf;
cf_configuration_t fo_vote_conf;
cf_configuration_t fo_feeds_conf;
cf_configuration_t fo_userconf_conf;

cf_array_t Modules[MOD_MAX+1];

/* {{{ forum default config options */
cf_conf_opt_t default_options[] = {
  { "ExternCharset",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "TemplateMode",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "XHTMLMode",                cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "ThreadIndexFile",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "SocketName",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL,&fo_default_conf },
  { "BaseURL",                  cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UBaseURL",                 cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "PostingURL",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostingURL",              cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "PostingURL_List",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostingURL_List",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "PostingURL_Nested",        cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostingURL_Nested",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "VoteURL",                  cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "ArchiveURL",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "ArchivePostingURL",        cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "ErrorTemplate",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "PostScript",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UPostScript",              cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UserConfig",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UserManagement",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "UserRegister",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "SharedMemIds",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "Administrators",           cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                &fo_default_conf },
  { "AuthMode",                 cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },

  { "Categories",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },

  { "DateLocale",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "Language",                 cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "MessagesDatabase",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },

  { "ConfigDirectory",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "InactivityDelete",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },
  { "SendMailBeforeDelete",     cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_default_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_view config options */
cf_conf_opt_t fo_view_options[] = {
  { "DoQuote",                    cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "QuotingChars",               cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_NEEDED|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "ReadMode",                   cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_NEEDED|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "ThreadMode",                 cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_NEEDED|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "ShowThread",                 cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "ShowFlags",                  cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                             &fo_view_conf },
  { "ParamType",                  cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL,              &fo_view_conf },
  { "ShowSig",                    cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "MaxSigLines",                cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_view_conf },

  { "TemplateForumBegin",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "TemplateForumEnd",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "TemplateForumThread",        cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },

  { "DateFormatThreadList",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "DateFormatThreadView",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },
  { "DateFormatLoadTime",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_view_conf },

  { "Name",                       cf_handle_command,   CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_GLOBAL, &fo_view_conf },
  { "EMail",                      cf_handle_command,   CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_GLOBAL, &fo_view_conf },
  { "HomepageUrl",                cf_handle_command,   CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_GLOBAL, &fo_view_conf },
  { "ImageUrl",                   cf_handle_command,   CF_CFG_OPT_USER|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_GLOBAL, &fo_view_conf },
  { "AdminUseJS",                 cf_handle_command,   CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL|CF_CFG_OPT_UNIQUE,  &fo_view_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_post configuration options */
cf_conf_opt_t fo_post_options[] = {
  { "ReadMode",                   cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_NEEDED|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_post_conf },

  { "FieldConfig",               cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "FieldNeeded",               cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "RedirectOnPost",            cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL|CF_CFG_OPT_USER, &fo_post_conf },
  { "FieldValidate",             cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "QuotingChars",              cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_NEEDED|CF_CFG_OPT_UNIQUE|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "DateFormat",                cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL, &fo_post_conf },

  { "Image",                     cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },

  { "ThreadTemplate",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "FatalTemplate",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },
  { "OkTemplate",                cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_post_conf },

  {  NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_server config options */
cf_conf_opt_t fo_server_options[] = {
  { "RunArchiver",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "ErrorLog",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "StdLog",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "LogMaxSize",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "PIDFile",              cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "MaxThreads",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "MinThreads",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "SpareThreads",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },
  { "Forums",               cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL, &fo_server_conf },

  { "SortThreads",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,  &fo_server_conf },
  { "SortMessages",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,  &fo_server_conf },
  { "ArchiveOnVote",        cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                 &fo_server_conf },
  { "MainFileMaxBytes",     cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,  &fo_server_conf },
  { "MainFileMaxPostings",  cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,  &fo_server_conf },
  { "MainFileMaxThreads",   cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,  &fo_server_conf },

  { "UserGroup",            cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL,                &fo_server_conf },
  { "Chroot",               cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_GLOBAL,                &fo_server_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_arcview viewer options */
cf_conf_opt_t fo_arcview_options[] = {
  { "SortYearList",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "SortMonthList",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "SortThreadList",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "SortMessages",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "DateFormatList",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "DateFormatViewList",      cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "EnableCache",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "CacheLevel",              cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "CacheDir",                cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "QuotingChars",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },

  { "YearsTemplate",           cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "MonthsTemplate",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "ThreadTemplate",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },
  { "ThreadListTemplate",      cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,   &fo_arcview_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_vote options */
cf_conf_opt_t fo_vote_options[] = {
  { "VotingDatabase",  cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,               &fo_vote_conf },
  { "Send204",         cf_handle_command,   CF_CFG_OPT_UNIQUE|CF_CFG_OPT_CONFIG|CF_CFG_OPT_USER|CF_CFG_OPT_LOCAL,  &fo_vote_conf },
  { "OkTemplate",      cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_NEEDED|CF_CFG_OPT_LOCAL,               &fo_vote_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_feeds_options */
cf_conf_opt_t fo_feeds_options[] = {
  { "DateLocaleEn",         cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "AtomTitle",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "AtomTagline",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "RSSTitle",             cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "RSSDescription",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "RSSDescriptionThread", cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_feeds_conf },
  { "RSSCopyright",         cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                &fo_feeds_conf },
  { "FeedLang",             cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                &fo_feeds_conf },
  { "RSSWebMaster",         cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                &fo_feeds_conf },
  { "RSSCategory",          cf_handle_command,   CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL,                &fo_feeds_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* {{{ fo_userconf_options */
cf_conf_opt_t fo_userconf_options[] = {
  { "MinLength",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "MaxLength",       cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "MinVal",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "MaxVal",          cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "ModuleConfig",    cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "FromUntilFormat", cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },
  { "Edit",            cf_handle_command,   CF_CFG_OPT_NEEDED|CF_CFG_OPT_CONFIG|CF_CFG_OPT_LOCAL, &fo_userconf_conf },

  { NULL, NULL, 0, NULL }
};
/* }}} */

/* }}} */

/* {{{ cf_get_conf_file
 * Returns: u_char ** (NULL on failure, a string-array on success)
 * Parameters:
 *   - u_char **argv  a list of parameter strings
 *
 * this function expects the argv parameter list of main(). It uses
 * the first parameter, the complete path to the program and returns
 * the pathes to the configuration files
 *
 */
cf_array_t *cf_get_conf_file(const u_char **which,size_t llen) {
  cf_array_t *ary = cf_alloc(NULL,1,sizeof(*ary),CF_ALLOC_CALLOC);
  u_char *env;
  cf_string_t file;
  size_t len;
  struct stat st;
  size_t i;

  if((env = getenv("CF_CONF_DIR")) == NULL) {
    fprintf(stderr,"CF_CONF_DIR has not been set!\n");
    return NULL;
  }

  len = strlen(env);

  cf_array_init(ary,sizeof(u_char **),NULL);

  for(i=0;i<llen;i++) {
    cf_str_init(&file);

    cf_str_char_set(&file,env,len);

    if(file.content[file.len-1] != '/') {
      cf_str_char_append(&file,'/');
    }

    cf_str_chars_append(&file,which[i],strlen(which[i]));
    cf_str_chars_append(&file,".conf",5);

    memset(&st,0,sizeof(st));
    if(stat(file.content,&st) == -1) {
      cf_str_cleanup(&file);
      fprintf(stderr,"could not find config file '%s': %s\n",file.content,strerror(errno));
      return NULL;
    }

    cf_array_push(ary,&file.content);
  }

  return ary;
}
/* }}} */

/* {{{ parse_args */
/**
 * This function parses the arguments of a configuration directive
 * Parameters:
 * \param line The line in the configuration file
 * \param args A reference to a u_char ** pointer
 * \param lnum The line number
 * \return 0 on success, error code on failure
 */
int parse_args(u_char *fname,u_char *line,u_char ***args,int lnum) {
  register u_char *ptr = line;
  int len = -1,run;
  cf_string_t argument;

  *args = NULL;

  /* read until end-of-line */
  while(1) {
    /*
     * skip trailing whitespaces
     */
    for(;*ptr && isspace(*ptr) && *ptr != (u_char)012;++ptr);
    if(*ptr == (u_char)012 || *ptr == (u_char)0 || *ptr == '#') break; /* end of line or end of file or comment */

    if(*ptr != '"') {
      fprintf(stderr,"[%s:%d] unexpected character %x!\n",fname,lnum,*ptr);
      return -1;
    }

    cf_str_init(&argument);
    run = 1;

    for(++ptr;*ptr && run;++ptr) {
      switch(*ptr) {
        case '\\':
          switch(*++ptr) {
            case 'n':
              cf_str_char_append(&argument,'\n');
              break;
            case 't':
              cf_str_char_append(&argument,'\t');
              break;
            case 'r':
              cf_str_char_append(&argument,'\r');
              break;
            default:
              cf_str_char_append(&argument,*ptr);
              break;
          }
          break;
        case '"':
          /* empty arguments have to be filled up with a \0 byte */
          if(len >= 0 && !(*args)[len]) (*args)[len] = strdup("\0");

          *args = cf_alloc(*args,++len + 1,sizeof(u_char *),CF_ALLOC_REALLOC);
          (*args)[len] = argument.content;
          run = 0;
          break;
        default:
          cf_str_char_append(&argument,*ptr);
          break;
      }
    }

  }

  /* empty arguments have to be filled up with a \0 byte */
  if(len >= 0 && !(*args)[len]) (*args)[len] = strdup("\0");

  return len + 1;
}
/* }}} */

/* {{{ destroy_str_ptr */
void destroy_str_ptr(void *tmp) {
  free(*((u_char **)tmp));
}
/* }}} */

/* {{{ cf_read_config */
int cf_read_config(cf_configfile_t *conf,cf_take_default_t deflt,int mode) {
  int fd = open(conf->filename,O_RDONLY);
  u_char *buff,buff1[512];
  u_char *ptr,*ptr1;
  struct stat st;
  u_char *directive_name;
  u_char **args;
  int i,found,fatal = 0;
  cf_conf_opt_t *opt;
  unsigned int linenum = 0;
  int argnum;
  cf_list_element_t *lelem;
  u_char *context = NULL;
  cf_string_t mpath;
  size_t len,j;
  cf_hash_t *hsh;
  cf_array_t ary;
  int noload = mode & CF_CFG_MODE_NOLOAD;

  mode &= ~CF_CFG_MODE_NOLOAD;
  hsh = cf_hash_new(NULL);
  cf_array_init(&ary,sizeof(u_char **),destroy_str_ptr);

  if(mode == CF_CFG_MODE_USER) context = getenv("CF_FORUM_NAME");


  /* {{{ initializing */
  /*
   * open() could fail :)
   */
  if(fd == -1) {
    fprintf(stderr,"configparser: open: could not open configfile '%s': %s\n",conf->filename,strerror(errno));
    return 1;
  }

  if(stat(conf->filename,&st) == -1) {
    close(fd);
    fprintf(stderr,"configparser: stat: could not stat configfile '%s': %s\n",conf->filename,strerror(errno));
    return 1;
  }

  if(st.st_size == 0) {
    close(fd);
    fprintf(stderr,"configparser: configfile %s is empty!\n",conf->filename);
    return 1;
  }

  if(((void *)(buff = mmap(0,st.st_size+1,PROT_READ,MAP_FILE|MAP_SHARED,fd,0))) == (caddr_t)-1) {
    close(fd);
    fprintf(stderr,"configparser: mmap: could not map file '%s' to memory: %s\n",conf->filename,strerror(errno));
    return 1;
  }
  /* }}} */

  ptr = buff;

  cf_str_init(&mpath);

  /*
   * loop through the hole file
   */
  while(*ptr) {
    /* we're at the next line */
    ++linenum;

    /* {{{ get beginning of directive */
    /* first, lets find the beginning of the line (whitespaces have to be ignored) */
    for(;*ptr && isspace(*ptr);++ptr);

    /* eof */
    if(!*ptr) break;

    /* comment - continue with next line */
    if(*ptr == '#') {
      /* read until the end of the line */
      for(;*ptr && *ptr != (u_char)012;++ptr);
      if(!*ptr) break;
      ++ptr;
      continue;
    }

    /* got end of line - continue with next line */
    if(*ptr == (u_char)012) {
      ++ptr;
      continue;
    }
    /* }}} */

    /* ok, we're at the beginning of the directive. */
    ptr1 = ptr;

    /* now we got the end of the directive */
    for(;*ptr && !isspace(*ptr);++ptr);

    /* eof */
    if(!*ptr) break;

    /* {{{ we got an eoc (end-of-context) */
    if(*ptr1 == '}') {
      if(!context) {
        fprintf(stderr,"[%s:%d] Context closed but not open!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      free(context);
      context = NULL;

      continue;
    }
    /* }}} */

    directive_name = strndup(ptr1,ptr-ptr1);

    /* {{{ hey, we got a context line -- read context */
    if(cf_strcmp(directive_name,"Forum") == 0) {
      for(;*ptr && isspace(*ptr);++ptr);
      if(!*ptr) {
        fprintf(stderr,"[%s:%d] unexpected EOF!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(!isalnum(*ptr)) {
        fprintf(stderr,"[%s:%d] Config file syntax error! Got Forum but identifier consists of characters not alnum (%c)\n",conf->filename,linenum,*ptr);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      ptr1 = ptr;

      for(;*ptr && isalnum(*ptr);++ptr);
      if(!*ptr) {
        fprintf(stderr,"[%s:%d] unexpected EOF!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }


      if(!isspace(*ptr) && *ptr != '{') {
        fprintf(stderr,"[%s:%d] Config file syntax error!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(context) {
        fprintf(stderr,"[%s:%d] Configuration file syntax error, no nesting contexts allowed!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      context = strndup(ptr1,ptr-ptr1);

      free(directive_name);
      directive_name = strdup(context);
      cf_array_push(&ary,&directive_name);


      for(;*ptr && isspace(*ptr);++ptr);

      if(*ptr != '{') {
        fprintf(stderr,"[%s:%d] Configuration file syntax error!\n",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      /* ok, go to next character (last was }) */
      ++ptr;

      continue;
    }
    /* }}} */

    if((argnum = parse_args(conf->filename,ptr,&args,linenum)) == -1) {
      close(fd);
      munmap(buff,st.st_size);
      return 1;
    }

    /* {{{ we got module path */
    if(cf_strcmp(directive_name,"ModulePath") == 0) {
      if(argnum != 1) {
        fprintf(stderr,"[%s:%d] Hey, wrong argument count for ModulePath!",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      cf_str_char_set(&mpath,args[0],strlen(args[0]));
      free(args[0]);
      free(args);
      free(directive_name);

      /* now everything whith this directive has finished. Find end of line... */
      for(;*ptr && *ptr != (u_char)012;++ptr);

      continue;
    }
    /* }}} */

    /* {{{ we got a Load directive -- go and load the module */
    if(cf_strcmp(directive_name,"Load") == 0) {
      if(argnum != 1) {
        fprintf(stderr,"[%s:%d] Hey, wrong argument count for Load directive!",conf->filename,linenum);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }

      if(noload == 0) {
        i = mpath.len;
        cf_str_chars_append(&mpath,args[0],strlen(args[0]));
        cf_str_chars_append(&mpath,".so",3);

        if(cf_add_module(conf,mpath.content,args[0]) != 0) {
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }

        mpath.len = i;
      }

      free(args[0]);
      free(args);
      free(directive_name);

      /* now everything whith this directive has finished. Find end of line... */
      for(;*ptr && *ptr != (u_char)012;++ptr);

      continue;
    }
    /* }}} */

    for(i=0;i<argnum;++i) {
      if(is_valid_utf8_string(args[i],strlen(args[i])) != 0) {
        fprintf(stderr,"[%s:%d] Sorry, argument %d for directive '%s' is not valid UTF-8!\n",conf->filename,linenum,i+1,opt->name);
        close(fd);
        munmap(buff,st.st_size);
        return 1;
      }
    }

    found = 0;
    if((opt = cf_hash_get(conf->options,directive_name,ptr-ptr1)) != NULL) {
      /* {{{ is the directive allowed? */
      if(context && opt->flags) {
        if((opt->flags & CF_CFG_OPT_LOCAL) == 0 && mode != CF_CFG_MODE_USER) {
          fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in local context!\n",conf->filename,linenum,directive_name);
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }
      }
      else {
        if((opt->flags & CF_CFG_OPT_GLOBAL) == 0 && mode != CF_CFG_MODE_USER) {
          fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in global context!\n",conf->filename,linenum,directive_name);
          close(fd);
          munmap(buff,st.st_size);
          return 1;
        }
      }
      /* }}} */

      if((opt->flags & mode)) {
        /* mark option as seen */
        if(context) {
          len = snprintf(buff1,512,"%s_%s",context,directive_name);
          cf_hash_set(hsh,buff1,len,"1",1);
        }
        else opt->flags |= CF_CFG_OPT_SEEN;

        if(opt->callback) found = opt->callback(conf,opt,context,args,argnum);
      }
      else {
        fprintf(stderr,"[%s:%d] Configuration directive %s not allowed in this mode!\n",conf->filename,linenum,directive_name);
        munmap(buff,st.st_size);
        close(fd);
        return 1;
      }
    }
    else {
      if(deflt) found = deflt(conf,context,directive_name,args,argnum);
      else {
        fprintf(stderr,"[%s:%d] Configuration entry for directive %s not found!\n",conf->filename,linenum,directive_name);
        return 1;
      }
    }

    if(found != -1) {
      /* arguments are no longer needed */
      if(argnum > 0) for(i=1;i<argnum;i++) free(args[i]);
      free(args[0]);
      free(args);

      if(found != 0) {
        fprintf(stderr,"[%s:%d] %s: Callback function returned not 0 or -1!\n",conf->filename,linenum,directive_name);
        return 1;
      }
    }

    free(directive_name);

    /* now everything whith this directive has finished. Find end of line... */
    for(;*ptr && *ptr != (u_char)012;++ptr);

    /* eof */
    if(!*ptr) break;

    ptr += 1;
  }

  close(fd);
  munmap(buff,st.st_size);

  if(mode != CF_CFG_MODE_USER) {
    for(lelem=conf->options_list.elements;lelem;lelem = lelem->next) {
      opt = (cf_conf_opt_t *)lelem->data;
      if(opt->flags & CF_CFG_OPT_NEEDED) {
        if(opt->flags & mode) {
          /* ah, this directive has to be in every context */
          if(opt->flags & CF_CFG_OPT_LOCAL) {
            for(j=0;j<ary.elements;++j) {
              context = *((u_char **)cf_array_element_at(&ary,j));
              len = snprintf(buff1,512,"%s_%s",context,opt->name);
              if(cf_hash_get(hsh,buff1,len) == NULL) {
                fatal = 1;
                fprintf(stderr,"missing configuration entry %s in %s for context %s\n",opt->name,conf->filename,context);
              }
            }
          }
          /* global directive */
          else {
            if((opt->flags & CF_CFG_OPT_SEEN) == 0) {
              fatal = 1;
              fprintf(stderr,"missing configuration entry %s in %s\n",opt->name,conf->filename);
            }
          }
        }
      }
    }
  }

  cf_str_cleanup(&mpath);
  cf_hash_destroy(hsh);
  cf_array_destroy(&ary);

  return fatal;
}
/* }}} */

/* {{{ cf_cfg_compare */
int cf_cfg_compare(cf_tree_dataset_t *dt1,cf_tree_dataset_t *dt2) {
  return strcmp(dt1->key,dt2->key);
}
/* }}} */

/* {{{ cf_add_module */
int cf_add_module(cf_configfile_t *cfile,const u_char *path,const u_char *name) {
  cf_array_t *modules = Modules;
  void *mod = dlopen(path,RTLD_LAZY);
  void *mod_cfg_p;
  char *error;
  cf_module_t module = { NULL, NULL, NULL };
  cf_module_config_t *mod_cfg;
  int i;


  if(mod) {
    mod_cfg_p = dlsym(mod,name);

    if((error = (u_char *)dlerror()) == NULL) {
      mod_cfg = (cf_module_config_t *)mod_cfg_p;

      if(!mod_cfg_p) {
        fprintf(stderr,"ERROR: cannot load plugin configuration!\n");
        return -1;
      }

      if(mod_cfg->module_magic_cookie != MODULE_MAGIC_COOKIE) {
        #ifdef DEBUG
        fprintf(stderr,"module magic number: %lu, modcfg: %lu\n",mod_cfg->module_magic_cookie,MODULE_MAGIC_COOKIE);
        #endif

        /* check what's the problem */
        if((mod_cfg->module_magic_cookie >> 16) == MODULE_MAGIC_NUMBER_MAJOR) fprintf(stderr,"CAUTION! module '%s': bad minor magic number, you should really update the module!\n",name);
        else {
          fprintf(stderr,"FATAL ERROR! bad magic number! Maybe module '%s' is to old or to new?\n",name);
          return -1;
        }
      }

      if(mod_cfg->config_init) {
        if(mod_cfg->config_init(cfile) != 0) return -1;
      }

      if(mod_cfg->cfgopts) cf_cfg_register_options(cfile,mod_cfg->cfgopts);

      /* register the module in the module list */
      if(!modules[0].element_size) cf_array_init(&modules[0],sizeof(cf_module_t),cf_cfg_destroy_module);

      module.module = mod;
      module.cfg    = mod_cfg;

      cf_array_push(&modules[0],&module);

      /* register all handlers */
      for(i=0;mod_cfg->handlers[i].handler;i++) {
        if(!modules[mod_cfg->handlers[i].handler].element_size) cf_array_init(&modules[mod_cfg->handlers[i].handler],sizeof(cf_handler_config_t),NULL);

        cf_array_push(&modules[mod_cfg->handlers[i].handler],&mod_cfg->handlers[i]);
      }
    }
    else {
      dlclose(mod);
      fprintf(stderr,"could not get module conmfig: %s\n",error);
      return -1;
    }
  }
  else {
    fprintf(stderr,"%s\n",dlerror());
  }

  return 0;
}

/* }}} */

/* {{{ destroy_directive */
void destroy_directive(void *arg) {
  cf_name_value_t *val = (cf_name_value_t *)arg;
  size_t i;

  free(val->name);
  for(i=0;i<val->valnum;i++) free(val->values[i]);
  free(val->values);
}
/* }}} */

/* {{{ destroy_directive_list */
void destroy_directive_list(cf_tree_dataset_t *dt) {
  cf_list_head_t *head = (cf_list_head_t *)dt->data;

  cf_list_destroy(head,destroy_directive);

  free(dt->key);
  free(dt->data);
}
/* }}} */

/* {{{ destroy_forums_list */
void destroy_forums_list(void *data) {
  cf_internal_config_t *config = (cf_internal_config_t *)data;

  free(config->name);
  cf_tree_destroy(&config->directives);
  free(config);
}
/* }}} */

/* {{{ cf_handle_command */
int cf_handle_command(cf_configfile_t *cfile,cf_conf_opt_t *opt,const u_char *context,u_char **args,size_t argnum) {
  cf_configuration_t *conf = (cf_configuration_t *)opt->data;
  cf_name_value_t tmp,*tmp1;
  cf_tree_dataset_t dt;
  cf_tree_t *tr;
  const cf_tree_dataset_t *dt1;
  cf_list_head_t *head;
  size_t i;
  cf_list_element_t *elem;
  cf_internal_config_t *icfg = NULL;

  tmp.values = args;
  tmp.valnum = argnum;
  tmp.name   = strdup(opt->name);

  dt.key = opt->name;

  if(context) {
    for(elem=conf->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (cf_internal_config_t *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) {
      icfg = cf_alloc(NULL,1,sizeof(*icfg),CF_ALLOC_MALLOC);
      icfg->name = strdup(context);
      cf_tree_init(&icfg->directives,cf_cfg_compare,destroy_directive_list);
      cf_list_append_static(&conf->forums,icfg,sizeof(*icfg));
    }

    tr = &icfg->directives;
  }
  else tr = &conf->global_directives;


  if((dt1 = cf_tree_find(tr,tr->root,&dt)) != NULL) {
    if(opt->flags & CF_CFG_OPT_UNIQUE) {
      head = dt1->data;
      tmp1 = head->elements->data;

      for(i=0;i<tmp1->valnum;i++) free(tmp1->values[i]);
      free(tmp1->values);

      tmp1->values = args;
    }
    else {
      head = dt1->data;
      cf_list_append(head,&tmp,sizeof(tmp));
    }
  }
  else {
    head = cf_alloc(NULL,1,sizeof(*head),CF_ALLOC_CALLOC);
    cf_list_append(head,&tmp,sizeof(tmp));

    dt.key = strdup(opt->name);
    dt.data = head;

    cf_tree_insert(tr,NULL,&dt);
  }

  return -1;
}
/* }}} */

/* {{{ cf_cfg_cleanup */
void cf_cfg_cleanup(cf_configuration_t *cfg) {
  cf_tree_destroy(&cfg->global_directives);
  cf_list_destroy(&cfg->forums,destroy_forums_list);
}
/* }}} */

/* {{{ cf_cfg_destroy_module */
void cf_cfg_destroy_module(void *element) {
  cf_module_t *mod = (cf_module_t *)element;

  if(mod->module) {
    if(mod->cfg->finish) mod->cfg->finish();
    dlclose(mod->module);
  }
}
/* }}} */

/* {{{ cf_cleanup_modules
 * Returns: nothing
 * Parameters:
 *   - cf_module_t *mod   the module structure
 *
 * this function cleans up the modules
 *
 */
void cf_cleanup_modules(cf_array_t *modules) {
  int i;

  for(i=0;i<=MOD_MAX;++i) {
    cf_array_destroy(&modules[i]);
  }
}
/* }}} */

/* {{{ cf_cfg_get_first_value */
cf_name_value_t *cf_cfg_get_first_value(cf_configuration_t *cfg,const u_char *context,const u_char *name) {
  const cf_tree_dataset_t *dt;
  cf_tree_dataset_t dt1;
  cf_list_head_t *head;
  cf_tree_t *tr;
  cf_list_element_t *elem;
  cf_internal_config_t *icfg = NULL;

  dt1.key = (void *)name;

  if(context) {
    for(elem=cfg->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (cf_internal_config_t *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) return NULL;

    tr = &icfg->directives;
  }
  else tr = &cfg->global_directives;


  if((dt = cf_tree_find(tr,tr->root,&dt1)) != NULL) {
    head = dt->data;
    return head->elements->data;
  }

  return NULL;
}
/* }}} */

/* {{{ cf_cfg_get_value */
cf_list_head_t *cf_cfg_get_value(cf_configuration_t *cfg,const u_char *context,const u_char *name) {
  const cf_tree_dataset_t *dt;
  cf_tree_dataset_t dt1;
  cf_list_element_t *elem;
  cf_internal_config_t *icfg = NULL;
  cf_tree_t *tr;

  dt1.key = (void *)name;

  if(context) {
    for(elem=cfg->forums.elements;elem;elem=elem->next,icfg=NULL) {
      icfg = (cf_internal_config_t *)elem->data;
      if(cf_strcmp(icfg->name,context) == 0) break;
    }

    if(!icfg) return NULL;

    tr = &icfg->directives;
  }
  else tr = &cfg->global_directives;

  if((dt = cf_tree_find(tr,tr->root,&dt1)) != NULL) return dt->data;

  return NULL;
}
/* }}} */

/* {{{ cf_cfg_init_file */
void cf_cfg_init_file(cf_configfile_t *conf,u_char *filename) {
  conf->filename = strdup(filename);
  conf->options  = cf_hash_new(NULL);
  cf_list_init(&conf->options_list);
}
/* }}} */

/* {{{ cf_cfg_register_options */
int cf_cfg_register_options(cf_configfile_t *conf,cf_conf_opt_t *opts) {
  int i;

  for(i=0;opts[i].name;i++) {
    if(cf_hash_get(conf->options,(u_char *)opts[i].name,strlen(opts[i].name)) != NULL) {
      errno = EINVAL;
      return -1;
    }

    /*
     * be sure that seen has not been set, yet -- programmers have really
     * silly ideas, sometimes
     */
    opts[i].flags &= ~CF_CFG_OPT_SEEN;

    cf_hash_set_static(conf->options,(u_char *)opts[i].name,strlen(opts[i].name),&opts[i]);
    cf_list_append_static(&conf->options_list,&opts[i],sizeof(opts[i]));
  }

  return 0;
}
/* }}} */

/* {{{ cf_cfg_cleanup_file */
void cf_cfg_cleanup_file(cf_configfile_t *conf) {
  cf_hash_destroy(conf->options);
  cf_list_destroy(&conf->options_list,NULL);
  free(conf->filename);
}
/* }}} */

/* {{{ cf_cfg_init */
void cf_cfg_init(void) {
  cf_tree_init(&fo_default_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_server_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_view_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_arcview_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_post_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_vote_conf.global_directives,cf_cfg_compare,destroy_directive_list);
  cf_tree_init(&fo_feeds_conf.global_directives,cf_cfg_compare,destroy_directive_list);

  cf_list_init(&fo_default_conf.forums);
  cf_list_init(&fo_server_conf.forums);
  cf_list_init(&fo_view_conf.forums);
  cf_list_init(&fo_arcview_conf.forums);
  cf_list_init(&fo_post_conf.forums);
  cf_list_init(&fo_vote_conf.forums);
  cf_list_init(&fo_feeds_conf.forums);
}
/* }}} */

/* {{{ cf_cfg_destroy */
void cf_cfg_destroy(void) {
  cf_cfg_cleanup(&fo_default_conf);
  cf_cfg_cleanup(&fo_server_conf);
  cf_cfg_cleanup(&fo_view_conf);
  cf_cfg_cleanup(&fo_arcview_conf);
  cf_cfg_cleanup(&fo_post_conf);
  cf_cfg_cleanup(&fo_vote_conf);
  cf_cfg_cleanup(&fo_feeds_conf);
}
/* }}} */

/* eof */
