%module "CForum::Validator"
%{
#include "config.h"
#include "defines.h"

#include "utils.h"
#include "validate.h"
%}

int is_valid_link(const char *link);
int is_valid_http_link(const char *link,int strict = 0);
int is_valid_mailaddress(const char *addr);
int is_valid_mailto_link(const char *addr);
int is_valid_hostname(const char *hostname);
int is_valid_telnet_link(const char *link);
int is_valid_nntp_link(const char *link);
int is_valid_news_link(const char *link);
int is_valid_ftp_link(const char *link);
int is_valid_prospero_link(const char *link);
int is_valid_wais_link(const char *link);
int is_valid_gopher_link(const char *link);
int is_valid_file_link(const char *link);


/* eof */
