configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories (BEFORE ${CMAKE_CURRENT_BINARY_DIR})

add_library (cfcharconvert SHARED charconvert.c unicode_classes.c)
if (HAVE_LIBICONV)
  target_link_libraries (cfcharconvert iconv)
endif (HAVE_LIBICONV)

add_library (cfreadline SHARED readline.c)
add_library (cfhashlib SHARED hashlib.c)
add_library (cfutils SHARED utils.c arrayutils.c listutils.c memoryutils.c stringutils.c treeutils.c cacheutils.c datautils.c ipcutils.c wwwutils.c)
target_link_libraries (cfutils cfhashlib cfcharconvert ${ZLIB_LIBRARY} ${CURL_LIBRARIES})
add_library (cftemplate SHARED template.c)
target_link_libraries (cftemplate cfutils cfhashlib cfcharconvert)
if (HAVE_LIBDL)
  target_link_libraries (cftemplate dl)
endif (HAVE_LIBDL)
add_library (cfcgi SHARED cfcgi.c)
target_link_libraries (cfcgi cfhashlib)
add_library (cfvalidate SHARED validate.c)
target_link_libraries (cfvalidate cfutils ${IDN_LIBRARIES})
add_library (cfhtml SHARED htmllib.c)
target_link_libraries (cfhtml cfutils cfcharconvert cfclientlib)
add_library (cfconfigparser SHARED configparser.c)
target_link_libraries (cfconfigparser cfutils cfcharconvert)
if (HAVE_LIBDL)
  target_link_libraries (cfconfigparser dl)
endif (HAVE_LIBDL)
add_library (cfserverutils SHARED serverutils.c cf_pthread.c)
target_link_libraries (cfserverutils ${CMAKE_THREAD_LIBS_INIT})
add_library (cfserverlib SHARED serverlib.c shm_locking.c semaphores.c archiver.c cf_pthread.c)
target_link_libraries (cfserverlib cfhashlib cfutils cfreadline ${CMAKE_THREAD_LIBS_INIT})
add_library (cfclientlib SHARED clientlib.c handlerrunners.c communication.c messageutils.c semaphores.c)
target_link_libraries (cfclientlib cfconfigparser cfcharconvert cfreadline cfutils cfhashlib cfcgi cftemplate ${BERKELEY_DB_LIBRARIES})

add_executable (cf-recovery cf-recovery.c)
target_link_libraries (cf-recovery cfcharconvert cfreadline cfutils cfserverlib cfconfigparser cfserverutils cfhashlib)

add_executable (cf-tid_index cf-tid_index.c)
target_link_libraries (cf-tid_index cfutils cfconfigparser ${BERKELEY_DB_LIBRARIES})

add_executable (fo_arcview fo_arcview.c)
target_link_libraries (fo_arcview cfconfigparser cftemplate cfcharconvert cfutils cfhashlib cfclientlib cfcgi cfhtml)

add_executable (fo_feeds fo_feeds.c)
target_link_libraries (fo_feeds cfconfigparser cftemplate cfcharconvert cfreadline cfutils cfhashlib cfclientlib cfcgi cfhtml)

add_executable (fo_post fo_post.c)
target_link_libraries (fo_post cfconfigparser cftemplate cfcharconvert cfreadline cfutils cfhashlib cfclientlib cfcgi cfhtml cfvalidate ${PCRE_LIBRARIES})

add_executable (fo_server fo_server.c)
target_link_libraries (fo_server cfcharconvert cfreadline cfutils cfserverlib cfconfigparser cfserverutils cfhashlib ${CMAKE_THREAD_LIBS_INIT})

add_executable (fo_threadlookup fo_threadlookup.c)
target_link_libraries (fo_threadlookup cfclientlib cfutils cfconfigparser cfcgi ${BERKELEY_DB_LIBRARIES})

add_executable (fo_view fo_view.c)
target_link_libraries (fo_view cfconfigparser cftemplate cfcharconvert cfreadline cfutils cfhashlib cfclientlib cfcgi cfhtml)

add_executable (fo_vote fo_vote.c)
target_link_libraries (fo_vote cfutils cfhashlib cfreadline cfconfigparser cfclientlib cfcgi cftemplate ${BERKELEY_DB_LIBRARIES})

add_subdirectory (parsetpl)
add_subdirectory (modules)
add_subdirectory (perl)
add_subdirectory (swig)

set_target_properties (
    cfcharconvert
    cfreadline
    cfhashlib
    cfutils
    cftemplate
    cfcgi
    cfvalidate
    cfhtml
    cfconfigparser
    cfserverutils
    cfserverlib
    cfclientlib
  PROPERTIES
    LINK_FLAGS "-export-dynamic -avoid-version"
)

set_target_properties (
  cf-recovery
  cf-tid_index
  fo_arcview
  fo_feeds
  fo_post
  fo_server
  fo_threadlookup
  fo_view
  fo_vote
  PROPERTIES
    LINK_FLAGS "-export-dynamic"
)

install (
  TARGETS
    cfcharconvert
    cfreadline
    cfhashlib
    cfutils
    cftemplate
    cfcgi
    cfvalidate
    cfhtml
    cfconfigparser
    cfserverutils
    cfserverlib
    cfclientlib
    cf-recovery
    cf-tid_index
    fo_arcview
    fo_feeds
    fo_post
    fo_server
    fo_threadlookup
    fo_view
    fo_vote
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/cforum
)

install (
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    archiver.h
    cfcgi.h
    cf_pthread.h
    charconvert.h
    clientlib.h
    configparser.h
    defines.h
    entitytable.h
    fo_arcview.h
    fo_post.h
    fo_server.h
    fo_tid_index.h
    fo_view.h
    hashlib.h
    htmllib.h
    readline.h
    semaphores.h
    serverlib.h
    serverutils.h
    shm_locking.h
    template.h
    utils.h
    validate.h
    xml_handling.h
  DESTINATION
    ${CMAKE_INSTALL_PREFIX}/include/cforum
)
