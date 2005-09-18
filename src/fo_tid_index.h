/**
 * \file fo_tid_index.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * \brief Forum message indexer program
 *
 * This header file defines the data types for the
 * thread-id index file
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __FO_TID_INDEX_H
#define __FO_TID_INDEX_H

/**
 * struct for the index of the thread ids
 */
typedef struct s_mid_index {
  int32_t year;     /**< year of the index entry */
  int32_t month;    /**< month of the index entry */
  u_int64_t start;  /**< start thread id */ 
  u_int64_t end;    /**< end thread id */
} tid_index_t;

#endif

/* eof */
