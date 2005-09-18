/**
 * \file validate.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Validation functions
 *
 * This file contains validation function definitions (e.g. link and mail validation)
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

#ifndef _CF_VALIDATE_H
#define _CF_VALIDATE_H

/**
 * scheme list data structure
 */
typedef struct s_scheme_list {
  u_char scheme[20]; /**< The name of the scheme */
  int (*validator)(const u_char *); /**< Pointer to the validation function */
} scheme_list_t;

/**
 * General link checking function. Tries to get the scheme and calls the
 * validation function for that scheme
 * \param link The link to check
 * \return 0 if link is valid, -1 if it isn't
 */
int is_valid_link(const u_char *link);

/**
 * Validation function for HTTP links
 * \param link The link to check
 * \param strict Strict mode or no strict mode. If strict, no anchors are allowed
 * \return 0 if link is valid, -1 if it isn't
 */
int is_valid_http_link(const u_char *link,int strict);

/**
 * Validation function for a mail address
 * \param addr The mail address
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_mailaddress(const u_char *addr);

/**
 * Checks if a link is a valid mailto link
 * \param addr The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_mailto_link(const u_char *addr);

/**
 * Checks if a hostname is valid
 * \param hostname The name of the host
 * \return 0 if it is valid, -1 if it isn't
 */
int is_valid_hostname(const u_char *hostname);

/**
 * Checks if a link is a valid telnet link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_telnet_link(const u_char *link);

/**
 * Checks if a link is a valid nntp link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_nntp_link(const u_char *link);

/**
 * Checks if a link is a valid news link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_news_link(const u_char *link);

/**
 * Checks if a link is a valid ftp link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_ftp_link(const u_char *link);

/*
 * by Christian Seiler:
 */

/**
 * Checks if a link is a valid prospero link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_prospero_link(const u_char *link);

/**
 * Checks if a link is a valid wais link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_wais_link(const u_char *link);

/**
 * Checks if a link is a valid gopher link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_gopher_link(const u_char *link);

/**
 * Checks if a link is a valid file:// link
 * \param link The link string to check
 * \return 0 if the address is valid, -1 if it isn't
 */
int is_valid_file_link(const u_char *link);

#endif

/* eof */
