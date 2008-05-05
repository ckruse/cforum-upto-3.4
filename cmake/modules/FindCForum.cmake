# - Find CForum
# Find the CForum includes and client library
# This module defines
#  CFORUM_INCLUDE_DIR, where to find mysql.h
#  CFORUM_*_LIBRARY, the different CForum libraries
#  CFORUM_LIBRARY_PATH
#  CFORUM_FOUND, If false, do not try to use CForum.
#
# Copyright (c) 2008, Christian Seiler <webmaster@christian-seiler.de>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

find_path(CFORUM_INCLUDE_DIR cfcgi.h
   /usr/include/cforum
   /usr/local/include/cforum
)

find_library(CFORUM_CFCGI_LIBRARY NAMES cfcgi
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)
find_library(CFORUM_CHARCONVERT_LIBRARY NAMES cfcharconvert
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_CLIENTLIB_LIBRARY NAMES cfclientlib
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_CONFIGPARSER_LIBRARY NAMES cfconfigparser
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_HASHLIB_LIBRARY NAMES cfhashlib
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_HTML_LIBRARY NAMES cfhtml
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_READLINE_LIBRARY NAMES cfreadline
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_SERVERLIB_LIBRARY NAMES cfserverlib
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_SERVERUTILS_LIBRARY NAMES cfserverutils
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_TEMPLATE_LIBRARY NAMES cftemplate
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_UTILS_LIBRARY NAMES cfutils
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

find_library(CFORUM_VALIDATE_LIBRARY NAMES cfvalidate
   PATHS
   /usr/lib/cforum
   /usr/local/lib/cforum
)

if(CFORUM_INCLUDE_DIR AND CFORUM_CFCGI_LIBRARY)
  get_filename_component (CFORUM_LIBRARY_PATH "${CFORUM_CFCGI_LIBRARY}" PATH)
   set(CFORUM_FOUND TRUE)
   message(STATUS "Found CForum: ${CFORUM_INCLUDE_DIR}, ${CFORUM_LIBRARIES}")
else(CFORUM_INCLUDE_DIR AND CFORUM_CFCGI_LIBRARY)
   set(CFORUM_FOUND FALSE)
   message(STATUS "CForum not found.")
   if (CFORUM_FIND_REQUIRED)
     message (FATAL ERROR "CForum not found!")
   endif (CFORUM_FIND_REQUIRED)
endif(CFORUM_INCLUDE_DIR AND CFORUM_CFCGI_LIBRARY)

mark_as_advanced(
  CFORUM_INCLUDE_DIR
  CF_CFCGI_LIBRARY
  CFORUM_CHARCONVERT_LIBRARY
  CFORUM_CLIENTLIB_LIBRARY
  CFORUM_CONFIGPARSER_LIBRARY
  CFORUM_HASHLIB_LIBRARY
  CFORUM_HTML_LIBRARY
  CFORUM_READLINE_LIBRARY
  CFORUM_SERVERLIB_LIBRARY
  CFORUM_SERVERUTILS_LIBRARY
  CFORUM_TEMPLATE_LIBRARY
  CFORUM_UTILS_LIBRARY
  CFORUM_VALIDATE_LIBRARY
)
