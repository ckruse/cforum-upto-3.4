#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include "config.h"
#include "defines.h"

#include "utils.h"
#include "validate.h"

MODULE = CForum::Validator		PACKAGE = CForum::Validator		

PROTOTYPES: DISABLE

bool is_valid_link(url)
    const u_char *url
  CODE:
    if(is_valid_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_ftp_link(url)
    const u_char *url
  CODE:
    if(is_valid_ftp_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_gopher_link(url)
    const u_char *url
  CODE:
    if(is_valid_gopher_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_news_link(url)
    const u_char *url
  CODE:
    if(is_valid_news_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_nntp_link(url)
    const u_char *url
  CODE:
    if(is_valid_nntp_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_telnet_link(url)
    const u_char *url
  CODE:
    if(is_valid_telnet_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_prospero_link(url)
    const u_char *url
  CODE:
    if(is_valid_prospero_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_wais_link(url)
    const u_char *url
  CODE:
    if(is_valid_wais_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_mailaddress(url)
    const u_char *url
  CODE:
    if(is_valid_mailaddress(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_mailto_link(url)
    const u_char *url
  CODE:
    if(is_valid_mailto_link(url) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_http_link(url,strict=FALSE)
    const u_char *url
    bool strict
  CODE:
    if(is_valid_http_link(url,strict) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;

bool is_valid_hostname(host)
    const u_char *host
  CODE:
    if(is_valid_hostname(host) == -1) {
      XSRETURN_NO;
    }
    XSRETURN_YES;



# eof
