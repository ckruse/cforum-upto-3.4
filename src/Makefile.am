
SUBDIRS=. modules templates

bin_PROGRAMS= \
  fo_view \
  fo_post \
  fo_server \
  fo_cleanup \
  fo_threadlookup \
  fo_tid_index \
  fo_arcview

pkglib_LTLIBRARIES= \
  libcfcharconvert.la \
  libcfutils.la \
  libconfigparser.la \
  libcftemplate.la \
  libcfhashlib.la \
  libcfcgi.la \
  libserverlib.la \
  libcfclientlib.la \
  libcfreadline.la \
  libcfvalidate.la

libcfreadline_la_SOURCES=readline.c
libcfreadline_la_LDFLAGS=-export-dynamic -avoid-version

libcfhashlib_la_SOURCES=hashlib.c
libcfhashlib_la_LDFLAGS=-export-dynamic -avoid-version

libcfcharconvert_la_SOURCES=charconvert.c
libcfcharconvert_la_LIBADD=$(ICONV)
libcfcharconvert_la_LDFLAGS=-export-dynamic -avoid-version

libcfutils_la_SOURCES=utils.c arrayutils.c listutils.c memoryutils.c stringutils.c treeutils.c cacheutils.c
libcfutils_la_LIBADD=libcfhashlib.la libcfcharconvert.la
libcfutils_la_LDFLAGS=-export-dynamic -avoid-version

libconfigparser_la_SOURCES=configparser.c
libconfigparser_la_LIBADD=libcfutils.la $(DL)
libconfigparser_la_LDFLAGS=-export-dynamic -avoid-version

libcftemplate_la_SOURCES=template.c
libcftemplate_la_LIBADD=libcfutils.la libcfhashlib.la libcfcharconvert.la
libcftemplate_la_LDFLAGS=-export-dynamic -avoid-version

libcfcgi_la_SOURCES=cfcgi.c
libcfcgi_la_LIBADD=libcfhashlib.la
libcfcgi_la_LDFLAGS=-export-dynamic -avoid-version

libcfvalidate_la_SOURCES=validate.c
libcfvalidate_la_LIBADD=libcfutils.la $(IDN)
libcfvalidate_la_LDFLAGS=-export-dynamic -avoid-version

libcfclientlib_la_SOURCES=clientlib.c shm_locking.c semaphores.c
libcfclientlib_la_LIBADD=libcfutils.la libcfreadline.la libcfcharconvert.la libcftemplate.la
libcfclientlib_la_LDFLAGS=-export-dynamic -avoid-version

libserverlib_la_SOURCES=serverlib.c shm_locking.c semaphores.c archiver.c cf_pthread.c xml_handling.c
libserverlib_la_LIBADD=libcfhashlib.la libcfutils.la libcfreadline.la $(GDOME_LIBS) $(PTHREAD_LIB)
libserverlib_la_LDFLAGS=-export-dynamic -avoid-version

fo_view_SOURCES=fo_view.c
fo_view_LDADD=libcfcharconvert.la libcfutils.la libcfcgi.la libcftemplate.la libconfigparser.la libcfclientlib.la
fo_view_LDFLAGS=-export-dynamic

fo_post_SOURCES=fo_post.c
fo_post_LDADD=libcfcharconvert.la libcfutils.la libcfcgi.la libcftemplate.la libconfigparser.la libcfclientlib.la libcfvalidate.la
fo_post_LDFLAGS=-export-dynamic

fo_arcview_SOURCES=fo_arcview.c
fo_arcview_LDADD=libcfcharconvert.la libcfutils.la libcfcgi.la libconfigparser.la libcfclientlib.la $(GDOME_LIBS)

fo_server_SOURCES=fo_server.c initfinish.c
fo_server_LDADD=libcfcharconvert.la libcfreadline.la libcfutils.la libserverlib.la libconfigparser.la $(PTHREAD_LIB)
fo_server_LDFLAGS=-export-dynamic

fo_cleanup_SOURCES=fo_cleanup.c
fo_cleanup_LDADD=libcfutils.la libcfreadline.la libconfigparser.la libcfclientlib.la $(BDB_LIB)
fo_cleanup_LDFLAGS=-export-dynamic

fo_threadlookup_SOURCES=fo_threadlookup.c
fo_threadlookup_LDADD=libcfclientlib.la libcfutils.la libconfigparser.la libcfcgi.la
fo_threadlookup_LDFLAGS=-export-dynamic

fo_tid_index_SOURCES=fo_tid_index.c
fo_tid_index_LDADD=libcfutils.la libconfigparser.la
fo_tid_index_LDFLAGS=-export-dynamic


# eof
