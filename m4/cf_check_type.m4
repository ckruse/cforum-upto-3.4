dnl
dnl AC_CHECK_TYPE sucks
dnl
AC_DEFUN(CF_CHECK_TYPE,
  [
    AC_MSG_CHECKING(for $1)
    AC_CACHE_VAL(ac_cv_cf_have_$1,
      AC_TRY_COMPILE(
        [
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
        ],
        [$1 vr],
        ac_cv_cf_have_$1=yes,
        ac_cv_cf_have_$1=no
      )
    )
    AC_MSG_RESULT($ac_cv_cf_have_$1)
    if test "$ac_cv_cf_have_$1" = "no"; then
      AC_DEFINE(DONT_HAS_$1,$2,[Defined if we haven't got $1])
    fi
  ]
)

