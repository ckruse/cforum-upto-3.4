/**
 * \file serverlib.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 *
 * Data structure and function declarations for the server
 * library
 *
 */

struct s_cf_rwlocked_list_head {
  t_cf_rwlock lock;
  t_cf_list_head head;
} t_cf_rw_list_head;



/* eof */
