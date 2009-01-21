# - Try to find the IDN library
# Once done this will define
#
#  IDN_FOUND - system has the IDN library
#  IDN_INCLUDE_DIR - the IDN include directory
#  IDN_LIBRARIES - The libraries needed to use IDN
#
# Based on FindPCRE.cmake
# Distributed under the BSD license.

if (IDN_INCLUDE_DIR AND IDN_LIBRARIES)
  # Already in cache, be silent
  set(IDN_FIND_QUIETLY TRUE)
endif (IDN_INCLUDE_DIR AND IDN_LIBRARIES)

FIND_PATH(IDN_INCLUDE_DIR idna.h )

FIND_LIBRARY(IDN_LIBRARIES NAMES idn)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IDN DEFAULT_MSG IDN_INCLUDE_DIR IDN_LIBRARIES )

MARK_AS_ADVANCED(IDN_INCLUDE_DIR IDN_LIBRARIES)
