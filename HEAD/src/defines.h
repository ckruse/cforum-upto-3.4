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

#define LOG_ERR 1 /**< Used by the logging function. Log an error. */
#define LOG_STD 2 /**< Used by the logging function. Log a standard message. */
#define LOG_DBG 3 /**< Used by the logging function. Log a debugging message. */

#define TIMER 5L /**< Timer value. Check every 5 seconds. */

/* the dtd uri */
#define FORUM_DTD "http://cforum.teamone.de/forum.dtd" /**< The URL of the selfforum DTD */

/* some limits */
#define PRERESERVE 5 /**< Limit in lists or for bigger arrays to reserve before used (to avoid malloc() calls) */
#define MAXLINE    BUFSIZ /**< The size of a buffer for lines */

#define LISTENQ 1024 /**< The size of the listen() queque. */

#define INIT_HANDLER            1 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_HANDLER            2 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_INIT_HANDLER       3 /**< Handler hook number. Look in the programmers manual section for more information. */
#define VIEW_LIST_HANDLER       4 /**< Handler hook number. Look in the programmers manual section for more information. */
#define POSTING_HANDLER         5 /**< Handler hook number. Look in the programmers manual section for more information. */
#define CONNECT_INIT_HANDLER    6 /**< Handler hook number. Look in the programmers manual section for more information. */
#define AUTH_HANDLER            7 /**< Handler hook number. Look in the programmers manual section for more information. */
#define ARCHIVE_HANDLER         8 /**< Handler hook number. Look in the programmers manual section for more information. */
#define NEW_POST_HANDLER        9 /**< Handler hook number. Look in the programmers man... argh, fuck off, you know what I mean */
#define AFTER_POST_HANDLER     10 /**< Handler hook number. .... */

#define MOD_MAX                10 /**< The maximum hook value. */

#define FLT_OK       0 /**< Returned by a plugin function if everything as ok. */
#define FLT_DECLINE -1 /**< Returned by a plugin function if this request is not for the plugin. */
#define FLT_EXIT    -2 /**< Context dependend. */

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
