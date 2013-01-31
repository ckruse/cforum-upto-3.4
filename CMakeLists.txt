cmake_minimum_required(VERSION 2.4)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

project (cforum)

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")
include (CheckLibraryExists)
include (CheckSymbolExists)
include (CheckIncludeFile)
include (CheckTypeSize)
include (UsePkgConfig)
include (CMakeInstallDirs)

option (ENABLE_DEBUG "enable debugging mode" OFF)
option (ENABLE_SORTING "enable sorting APIs (beware of the performance consequences!)" OFF)
option (ENABLE_SHARED_MEM "enable shared memory (*much* more performance)" OFF)
set (SOCKET_PATH "/var/run/cforum_socket" CACHE FILEPATH "The path to the cforum UNIX domain socket")
set (PROTOCOL_FILE "/var/log/cf_sigprotocol.txt" CACHE FILEPATH "the path where the protocol file for bad signals is located (used for debugging)")

# Copy flags for config.h
if (ENABLE_DEBUG STREQUAL "ON")
  set (DEBUG ON)
else (ENABLE_DEBUG STREQUAL "ON")
  set (DEBUG OFF)
endif (ENABLE_DEBUG STREQUAL "ON")

if (ENABLE_SORTING STREQUAL "ON")
  set (CF_NO_SORTING OFF)
else (ENABLE_SORTING STREQUAL "ON")
  set (CF_NO_SORTING ON)
endif (ENABLE_SORTING STREQUAL "ON")

if (ENABLE_SHARED_MEM STREQUAL "ON")
  set (CF_SHARED_MEM ON)
else (ENABLE_SHARED_MEM STREQUAL "ON")
  set (CF_SHARED_MEM OFF)
endif (ENABLE_SHARED_MEM STREQUAL "ON")

add_definitions (-Wall -W -D_REENTRANT -D_GNU_SOURCE -funsigned-char -Wno-pointer-sign -Wno-unused-parameter)

if (ENABLE_DEBUG STREQUAL "ON")
  add_definitions (-g)
endif (ENABLE_DEBUG STREQUAL "ON")

# Differences between UNIX systems:
#   Some define dlopen()/... in -ldl
#   Others define it in the C library
#   Same with iconv() in -liconv or in C library

check_library_exists (dl dlopen "" HAVE_LIBDL)
if (NOT HAVE_LIBDL)
  check_library_exists ("" dlopen "" HAVE_DLOPEN)
  if (NOT HAVE_DLOPEN)
    message (FATAL_ERROR "Could not find dlopen() in -ldl or in standard library")
  endif (NOT HAVE_DLOPEN)
endif (NOT HAVE_LIBDL)
check_library_exists (iconv iconv "" HAVE_LIBICONV)
if (NOT HAVE_LIBICONV)
  check_library_exists ("" iconv "" HAVE_ICONV)
  if (NOT HAVE_ICONV)
    message (FATAL_ERROR "Could not find iconv() in -liconv or in standard library")
  endif (NOT HAVE_ICONV)
endif (NOT HAVE_LIBICONV)

# Don't do the same stupid stuff autoconf does in checking for
# lot's of functions that modern OS have anyway. Should anybody
# want to port this software to some obscure architecture, OS or
# compiler, then simply add the necessary checks in order to
# make sure the software runs. Otherwise, don't bother adding
# tests that will pass on any OS somebody tries this software.

# Test for these functions since replacements exist
set (CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists (strdup "string.h" HAVE_STRDUP)
check_symbol_exists (strndup "string.h" HAVE_STRNDUP)
check_symbol_exists (getline "stdio.h" HAVE_GETLINE)
check_symbol_exists (getdelim "stdio.h" HAVE_GETDELIM)

set (CMAKE_EXTRA_INCLUDE_FILES sys/types.h)
check_type_size (u_char U_CHAR)
check_type_size (bool BOOL)
set (CMAKE_EXTRA_INCLUDE_FILES)

find_package (Threads REQUIRED)
if (NOT CMAKE_USE_PTHREADS_INIT)
  message (FATAL_ERROR "Only pthreads are supported currently")
endif (NOT CMAKE_USE_PTHREADS_INIT)
#use: TARGET_LINK_LIBRARIES(myexe ${CMAKE_THREAD_LIBS_INIT})

find_package (BerkeleyDB REQUIRED)
find_package (PerlLibs REQUIRED)
find_package (Flex REQUIRED)
find_package (Gdome REQUIRED)
find_package (PCRE REQUIRED)
find_package (SWIG REQUIRED)
find_package (CURL REQUIRED)
find_package (IDN REQUIRED)
find_package (ZLIB REQUIRED)
find_package (ICU REQUIRED)

add_definitions (${GDOME_CFLAGS})

include_directories (${BERKELEY_DB_INCLUDE_DIR} ${PERL_INCLUDE_PATH}
  ${FLEX_INCLUDE_DIR} ${GDOME_INCLUDE_DIR} ${PCRE_INCLUDE_DIR}
  ${CURL_INCLUDE_DIR} ${IDN_INCLUDE_DIR} ${ICU_INCLUDE})

# Make sure rpath is ok
SET(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
IF (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/cforum")
ELSE (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cforum")
ENDIF (${CMAKE_INSTALL_LIBDIR} MATCHES "^/")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set (ADJUST_EXPORTS_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/scripts/adjust-exports.sh)

add_subdirectory (src)
add_subdirectory (scripts)
add_subdirectory (conf)

# Copy FindCForum.cmake macro (other programs may use it)
install (
  FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/FindCForum.cmake
  DESTINATION
    ${CMAKE_INSTALL_DATADIR}/cforum/cmake
)
