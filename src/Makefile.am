
SUBDIRS=. modules parsetpl swig

bin_PROGRAMS= \
	fo_server \
	fo_view \
	fo_vote \
	fo_tid_index \
	fo_post \
	fo_threadlookup

pkglib_LTLIBRARIES= \
	libcfcharconvert.la \
	libcfreadline.la \
	libcfhashlib.la \
	libcfutils.la \
	libcfconfigparser.la \
	libcfserverutils.la \
	libcfclientlib.la \
	libcfcgi.la \
	libcfvalidate.la \
	libcfhtml.la \
	libcftemplate.la \
	libcfserverlib.la

libcfcharconvert_la_SOURCES=charconvert.c
libcfcharconvert_la_LIBADD=$(ICONV)
libcfcharconvert_la_LDFLAGS=-export-dynamic -avoid-version

libcfreadline_la_SOURCES=readline.c
libcfreadline_la_LDFLAGS=-export-dynamic -avoid-version

libcfhashlib_la_SOURCES=hashlib.c
libcfhashlib_la_LDFLAGS=-export-dynamic -avoid-version

libcfutils_la_SOURCES=utils.c arrayutils.c listutils.c memoryutils.c stringutils.c treeutils.c cacheutils.c datautils.c
libcfutils_la_LIBADD=libcfhashlib.la libcfcharconvert.la -lz
libcfutils_la_LDFLAGS=-export-dynamic -avoid-version

libcftemplate_la_SOURCES=template.c
libcftemplate_la_LIBADD=libcfutils.la libcfhashlib.la libcfcharconvert.la
libcftemplate_la_LDFLAGS=-export-dynamic -avoid-version

libcfcgi_la_SOURCES=cfcgi.c
libcfcgi_la_LIBADD=libcfhashlib.la
libcfcgi_la_LDFLAGS=-export-dynamic -avoid-version

libcfvalidate_la_SOURCES=validate.c
libcfvalidate_la_LIBADD=libcfutils.la $(IDN)
libcfvalidate_la_LDFLAGS=-export-dynamic -avoid-version

libcfhtml_la_SOURCES=htmllib.c
libcfhtml_la_LIBADD=libcfutils.la libcfcharconvert.la libcfclientlib.la
libcfhtml_la_LDFLAGS=-export-dynamic -avoid-version

libcfconfigparser_la_SOURCES=configparser.c
libcfconfigparser_la_LIBADD=libcfutils.la $(DL)
libcfconfigparser_la_LDFLAGS=-export-dynamic -avoid-version

libcfserverutils_la_SOURCES=serverutils.c cf_pthread.c
libcfserverutils_la_LDFLAGS=-export-dynamic -avoid-version

libcfserverlib_la_SOURCES=serverlib.c shm_locking.c semaphores.c archiver.c cf_pthread.c
libcfserverlib_la_LIBADD=libcfhashlib.la libcfutils.la libcfreadline.la $(PTHREAD_LIB)
libcfserverlib_la_LDFLAGS=-export-dynamic -avoid-version

libcfclientlib_la_SOURCES=clientlib.c handlerrunners.c communication.c messageutils.c semaphores.c
libcfclientlib_la_LIBADD=libcfconfigparser.la libcfcharconvert.la libcfreadline.la libcfutils.la libcfhashlib.la libcfcgi.la libcftemplate.la $(BDB_LIB)
libcfclientlib_la_LDFLAGS=-export-dynamic -avoid-version

fo_threadlookup_SOURCES=fo_threadlookup.c
fo_threadlookup_LDADD=libcfclientlib.la libcfutils.la libcfconfigparser.la libcfcgi.la $(BDB_LIB)
fo_threadlookup_LDFLAGS=-export-dynamic

fo_tid_index_SOURCES=fo_tid_index.c
fo_tid_index_LDADD=libcfutils.la libcfconfigparser.la $(BDB_LIB)
fo_tid_index_LDFLAGS=-export-dynamic

fo_vote_SOURCES=fo_vote.c
fo_vote_LDADD=libcfutils.la libcfhashlib.la libcfreadline.la libcfconfigparser.la libcfclientlib.la libcfcgi.la libcftemplate.la $(BDB_LIB)
fo_vote_LDFLAGS=-export-dynamic

fo_post_SOURCES=fo_post.c
fo_post_LDADD=libcfconfigparser.la libcftemplate.la libcfcharconvert.la libcfreadline.la libcfutils.la libcfhashlib.la libcfclientlib.la libcfcgi.la libcfhtml.la libcfvalidate.la
fo_post_LDFLAGS=-export-dynamic

fo_view_SOURCES=fo_view.c
fo_view_LDADD=libcfconfigparser.la libcftemplate.la libcfcharconvert.la libcfreadline.la libcfutils.la libcfhashlib.la libcfclientlib.la libcfcgi.la libcfhtml.la
fo_view_LDFLAGS=-export-dynamic

fo_server_SOURCES=fo_server.c
fo_server_LDADD=libcfcharconvert.la libcfreadline.la libcfutils.la libcfserverlib.la libcfconfigparser.la libcfserverutils.la libcfhashlib.la $(PTHREAD_LIB)
fo_server_LDFLAGS=-export-dynamic

# eof
