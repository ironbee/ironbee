dnl Check for YAJL.
dnl CHECK_YAJL([CALL-ON-SUCCESS [, CALL-ON-FAILURE]])
dnl Sets:
dnl YAJL_CFLAGS
dnl YAJL_CPPFLAGS
dnl YAJL_LDFLAGS
dnl HAVE_YAJL

AC_DEFUN([CHECK_YAJL],
[dnl

CPPFLAGS_BACKUP=${CPPFLAGS}
CFLAGS_BACKUP=${CFLAGS}
LDFLAGS_BACKUP=${LDFLAGS}

AC_ARG_WITH(yajl_includes,
            [  --with-yajl-includes=PATH   Path to yajl include directory],
            [with_yajl_includes="${withval}"],
            [with_yajl_includes="no"])
AC_ARG_WITH(yajl_libraries,
            [  --with-yajl-libraries=PATH  Path to yajl library directory],
            [with_yajl_libraries="${withval}"],
            [with_yajl_libraries="no"])

dnl Headers
if test "$with_yajl_includes" != "no"; then
   YAJL_CPPFLAGS="-I${with_yajl_includes}"
   YAJL_CFLAGS="-I${with_yajl_includes}"
   CPPFLAGS="${CPPFLAGS} -I${with_yajl_includes}"
   CFLAGS="${CFLAGS} -I${with_yajl_includes}"
fi

AC_CHECK_HEADER(yajl/yajl_common.h,HAVE_YAJL="yes",HAVE_YAJL="no")
if test "$HAVE_YAJL" = "no"; then
    if test "$with_yajl_includes" = "no"; then
        AC_MSG_ERROR([Unable to locate yajl header files.  If you have installed it in a non-standard location, please specify the headers include path using --with-yajl-includes.])
    else
        AC_MSG_ERROR([The path specified for yajl header files using --with-yajl-includes doesn't contain the yajl header files.  Please re-check the supplied path.])
    fi
fi
AS_UNSET([ac_cv_header_yajl_yajl_common_h])

dnl Library
if test "$with_yajl_libraries" != "no"; then
    YAJL_LDFLAGS="-L${with_yajl_libraries} -lyajl"
    LDFLAGS="${LDFLAGS} -L${with_yajl_libraries} -lyajl"
else
    YAJL_LDFLAGS="-lyajl"
    LDFLAGS="-lyajl"
fi

AC_CHECK_LIB(yajl,yajl_parse,[HAVE_YAJL="yes"],[HAVE_YAJL="no"])
if test "$HAVE_YAJL" = "no"; then
    if test "$with_yajl_libraries" = "no"; then
        AC_MSG_ERROR([Unable to locate yajl library.  If you have installed it in a non-standard location, please specify the library path using --with-yajl-libraries.])
    else
        AC_MSG_ERROR([The path specified for yajl libraries using --with-yajl-libraries doesn't contain the library files.  Please re-check the supplied path.])
    fi
fi

AC_SUBST(HAVE_YAJL)
AC_SUBST(YAJL_CFLAGS)
AC_SUBST(YAJL_CPPFLAGS)
AC_SUBST(YAJL_LDFLAGS)
])
