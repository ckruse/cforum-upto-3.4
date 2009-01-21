#- check if plain "char" type is signed
#
#  CHECK_SIGNED_CHAR("FLAGS" VARIABLE)
#
# (TODO: replace this module with one that may check the signedness of any
# integer type.)
#
# Check if the plain "char" type of the C compiler is signed, and return the
# result in cache VARIABLE. The result is true if char is signed, or false
# if the char is unsigned or we could not run the test program.
# You may pass "FLAGS" such as -fsigned-char to the compiler, or just
# use "" for no flags.
#
# You should not need to perform this check unless you are building old
# or poor code that makes assumptions about the signedness of "char".

# Object Necromancer: Modules/CheckSignedChar.cmake --Kernigh, June 2007
# This module is in the public domain and has no copyright.

MACRO( CHECK_SIGNED_CHAR flags var )
  IF( DEFINED ${var} )
    # The cache already contains the result.
  ELSE( DEFINED ${var} )
    FILE( WRITE ${CMAKE_BINARY_DIR}/CMakeFiles/CheckSignedChar.c "
      int main() {
        int i = (char)255;
        if( i < 0 ) return 0; else return 1;
      }
    " )
    TRY_RUN( ${var} _dev_null ${CMAKE_BINARY_DIR}
             ${CMAKE_BINARY_DIR}/CMakeFiles/CheckSignedChar.c
             COMPILE_DEFINITIONS "${flags}" )
    IF( ${var} STREQUAL 0 )
      SET( "${var}" TRUE
           CACHE INTERNAL "True if char is known to be signed. ${flags}" )
    ELSE( ${var} STREQUAL 0 )
      SET( "${var}" FALSE
           CACHE INTERNAL "True if char is known to be signed. ${flags}" )
    ENDIF( ${var} STREQUAL 0 )
  ENDIF( DEFINED ${var} )
ENDMACRO( CHECK_SIGNED_CHAR )
