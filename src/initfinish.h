/**
 * \file initfinish.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
 * The initialization function forwards and datatypes.
 *
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef __INITFINISH_H
#define __INITFINISH_H

/**
 * This function creates the forum tree.
 * \param cfg The default configuration
 * \param head The head variable
 */
void make_forumtree(t_configuration *cfg,t_head *head);

/**
 * This function creates recursively a thread structure.
 * \param posting_newdoc A node to the posting ('Message')
 * \param posting_index A node to the posting in the index file (for unid)
 * \param contents A node list of the contents ('MessageContent')
 * \param thread A pointer to the thread object
 * \param post A pointer to the actual thread
 * \param level The level of the posting
 * \param pos The position in the content list
 */
void make_thread_tree(GdomeNode *posting_newdoc, GdomeNode *posting_index, GdomeNodeList *contents, t_thread *thread, t_posting *post, int level,int pos);


/**
 * This function reads the header data of a posting from the xml structure
 * \param p A pointer to the posting
 * \param n The 'Header' node
 * \param tid The thread id
 */
void handle_header(t_posting *p,GdomeNode *n,u_int64_t tid);

/**
 * This function free()s all for the thread tree reserved memory.
 */
void cleanup_forumtree();

#endif

/* eof */
