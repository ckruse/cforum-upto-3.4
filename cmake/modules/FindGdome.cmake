# - Try to find GDOME
# Once done this will define
#
#  GDOME_FOUND - system has gdome
#  GDOME_INCLUDE_DIR
#  GDOME_LIBRARIES
#  GDOME_CFLAGS

pkgconfig(gdome2 _GDOME_INCLUDE_DIR _GDOME_LINK_DIR _GDOME_LIBRARIES _GDOME_CFLAGS)

if (_GDOME_LIBRARIES)
  set (GDOME_FOUND TRUE)
  set (GDOME_INCLUDE_DIR ${_GDOME_INCLUDE_DIR} CACHE STRING "The include path for GDOME")
  set (GDOME_LIBRARIES ${_GDOME_LIBRARIES} CACHE STRING "The libraries needed for GDOME")
  set (GDOME_CFLAGS ${_GDOME_CFLAGS} CACHE STRING "The compiler flags needed for GDOME")
endif (_GDOME_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args (Gdome "Could not find Gdome" GDOME_INCLUDE_DIR GDOME_LIBRARIES)
MARK_AS_ADVANCED(GDOME_INCLUDE_DIR GDOME_LIBRARIES)

