
SUBDIRS=. modules

# fo_post not in 2.1!
bin_PROGRAMS= \
  fo_server

pkglib_LTLIBRARIES= \
	libcfcharconvert.la \
  libcfreadline.la \
  libcfhashlib.la \
  libcfutils.la \
  libconfigparser.la \
	libcfserverutils.la \
  libserverlib.la

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

libconfigparser_la_SOURCES=configparser.c
libconfigparser_la_LIBADD=libcfutils.la $(DL)
libconfigparser_la_LDFLAGS=-export-dynamic -avoid-version

libcfserverutils_la_SOURCES=serverutils.c cf_pthread.c
libcfserverutils_la_LDFLAGS=-export-dynamic -avoid-version

libserverlib_la_SOURCES=serverlib.c shm_locking.c semaphores.c archiver.c cf_pthread.c
libserverlib_la_LIBADD=libcfhashlib.la libcfutils.la libcfreadline.la $(PTHREAD_LIB)
libserverlib_la_LDFLAGS=-export-dynamic -avoid-version

fo_server_SOURCES=fo_server.c
fo_server_LDADD=libcfcharconvert.la libcfreadline.la libcfutils.la libserverlib.la libconfigparser.la libcfserverutils.la modules/flt_xmlstorage.la $(PTHREAD_LIB)
fo_server_LDFLAGS=-export-dynamic

# eof
