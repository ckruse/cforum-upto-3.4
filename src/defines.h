/**
 * \file defines.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief the standard defines
 *
 * This file contains some standard definitions needed in nearly any source file of the Classic Forum.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __DEFINES_H
#define __DEFINES_H

#define CF_SORT_ASCENDING 1
#define CF_SORT_DESCENDING 2
#define CF_SORT_NEWESTFIRST 3

#define CF_VERSION "3.1.1"

#define CF_BUFSIZ 2048

#define CF_ERR (1<<1) /**< Used by the logging function. Log an error. */
#define CF_STD (1<<2) /**< Used by the logging function. Log a standard message. */
#define CF_DBG (1<<3) /**< Used by the logging function. Log a debugging message. */
#define CF_FLSH (1<<4) /**< Used by the logging fuction. If bit is set, stream will be flusehd */

#define TIMER 5L /**< Timer value. Check every 5 seconds. */

/* some limits */
#define PRERESERVE 5 /**< Limit in lists or for bigger arrays to reserve before used (to avoid malloc() calls) */
#define MAXLINE    BUFSIZ /**< The size of a buffer for lines */

#define LISTENQ 1024 /**< The size of the listen() queque. */

#define INIT_HANDLER             1 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_HANDLER             2 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_INIT_HANDLER        3 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_LIST_HANDLER        4 /**< Handler hook number. Look in the programmers manual section for more information. */
#define POSTING_HANDLER          5 /**< Handler hook number. Look in the programmers manual section for more information. */
#define CONNECT_INIT_HANDLER     6 /**< Handler hook number. Look in the programmers manual section for more information. */
#define AUTH_HANDLER             7 /**< Handler hook number. Look in the programmers manual section for more information. */
#define ARCHIVE_HANDLER          8 /**< Handler hook number. Look in the programmers manual section for more information. */
#define NEW_POST_HANDLER         9 /**< Handler hook number. Look in the programmers man... argh, fuck off, you know what I mean */
#define AFTER_POST_HANDLER      10 /**< Handler hook number. .... */
#define HANDLE_404_HANDLER      11 /**< Handler hook .... */
#define DIRECTIVE_FILTER        12 /**< Handler hook */
#define PRE_CONTENT_FILTER      13 /**< Handler hook */
#define POST_CONTENT_FILTER     14 /**< Handler hook */
#define NEW_THREAD_HANDLER      15 /**< Handler h... */
#define DATA_LOADING_HANDLER    16 /**< ... */
#define THRDLST_WRITE_HANDLER   17 /**< ... */
#define ARCHIVE_THREAD_HANDLER  18 /**< ... */
#define SORTING_HANDLER         19 /**< ... */
#define POST_DISPLAY_HANDLER    20 /**< ... */
#define URL_REWRITE_HANDLER     21
#define THREAD_SORTING_HANDLER  22
#define THRDLST_WRITTEN_HANDLER 23
#define PERPOST_VAR_HANDLER     24

#define MOD_MAX                24 /**< The maximum hook value. */

#define FLT_OK       0 /**< Returned by a plugin function if everything as ok. */
#define FLT_DECLINE -1 /**< Returned by a plugin function if this request is not for the plugin. */
#define FLT_EXIT    -2 /**< Context dependend. */
#define FLT_ERROR   -3 /**< Error value */

#define init_modules() memset(&Modules,0,(MOD_MAX+1) * sizeof(t_array)) /**< Initialization macro for the plugins */

#define CF_KILL_DELETED 0 /**< kill deleted messages constant */
#define CF_KEEP_DELETED 1 /**< keep deleted messages constant */

#ifdef DONT_HAS_u_char
typedef unsigned char u_char; /**< We only use unsigned char (u_char) instead of char */
#endif

#if SIZEOF_SHORT != 2
#error "short has to be 2 bytes!"
#endif

#if SIZEOF_LONG < 4
#error "long has to be at least 4 bytes!"
#endif

#if SIZEOF_LONG_LONG < 8
#error "long long has to be at least 8 bytes!"
#endif

#ifdef DONT_HAS_int_16_t
typedef short int16_t;               /**< we need a datatype with 2 bytes */
#endif

#ifdef DONT_HAS_u_int16_t
typedef unsigned short u_int16_t;     /**< we need an unsigned datatype with 2 bytes */
#endif

#ifdef DONT_HAS_int32_t
typedef long int32_t;                /**< we need a datatype with 4 bytes */
#endif

#ifdef DONT_HAS_u_int32_t
typedef unsigned long u_int32_t;      /**< we need an unsigned datatype with 4 bytes */
#endif

#ifdef DONT_HAS_int64_t
typedef long long int64_t;           /**< we need a datatype with 8 bytes */
#endif

#ifdef DONT_HAS_u_int64_t
typedef unsigned long long u_int64_t; /**< we need an unsigned datatype with 8 bytes */
#endif

#ifndef HAVE_STRFTIME
#error "We need strftime()!"
#endif

#ifndef HAVE_GETENV
#error "getenv() is needed!"
#endif

#ifndef HAVE_GETTIMEOFDAY
#error "gettimeofday() is needed!"
#endif

#ifndef HAVE_MKDIR
#error "mkdir() is needed!"
#endif

#ifndef HAVE_MKTIME
#error "mktime() is needed!"
#endif

#ifndef HAVE_SNPRINTF
#error "snprintf() is needed!"
#endif

#ifndef HAVE_SOCKET
#error "socket() is needed!"
#endif

#ifndef HAVE_STRERROR
#error "strerror() is needed!"
#endif

#ifndef HAVE_STRTOL
#error "strtol() is needed!"
#endif

#ifndef HAVE_STRTOUL
#error "strtoul() is needed!"
#endif

#ifndef HAVE_STRTOLL
#error "strtoll() is needed!"
#endif

#ifndef HAVE_STRTOULL
#error "strtoull() is needed!"
#endif

//#ifndef size_t
//#define size_t unsigned
//#endif

#endif

/* eof */
