dnl Check for LibXML2
dnl CHECK_LIBXML2(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  LIBXML2
dnl  LIBXML2_CFLAGS

HAVE_LIBXML2="no"
HAVE_LIBXML2_H="no"
HAVE_LIBXML2_LIB="no"

LIBXML2_CPPFLAGS=""
LIBXML2_LDFLAGS=""
LIBXML2_CFLAGS=""

AC_DEFUN([CHECK_LIBXML2],
[dnl

AC_ARG_WITH(
    libxml2,
    [AC_HELP_STRING([--with-libxml2=PATH],[Path to libxml2])],
    [test_paths="${with_libxml2}"],
    [test_paths="/usr /usr/local /opt/local /opt"])

AC_MSG_CHECKING([for LibXML2])

SAVE_CPPFLAGS="${CPPFLAGS}"
SAVE_LDFLAGS="${LDFLAGS}"

if test "${test_paths}" != "no"; then
    libxml2_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${CPPFLAGS} -I${x}/include/libxml2"
        LDFLAGS="${LDFLAGS} -L${x}/lib"
        AC_CHECK_LIB(xml2,xmlReadMemory,HAVE_LIBXML2_LIB="yes",HAVE_LIBXML2_LIB="no")
        AC_CHECK_HEADERS(libxml/xmlversion.h,HAVE_LIBXML2_H="yes",HAVE_LIBXML2_H="no")
        if test "$HAVE_LIBXML2_H" != "no" && test "$HAVE_LIBXML2_LIB" != "no"; then
            libxml2_path="${x}"
            HAVE_LIBXML2="yes"
            LIBXML2_CPPFLAGS="-I${x}/include/libxml2"
            LIBXML2_LDFLAGS="-L${x}/lib"
            LIBXML2_CFLAGS="-I${x}/include/libxml2"
            break
        fi
    done

    AC_MSG_NOTICE([Not building with LibXML2 support.])
else
    AC_MSG_NOTICE([Not building with LibXML2 support])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"
LDFLAGS="${SAVE_LDFLAGS}"

AC_SUBST(HAVE_LIBXML2)
AC_SUBST(LIBXML2_CPPFLAGS)
AC_SUBST(LIBXML2_LDFLAGS)
AC_SUBST(LIBXML2_CFLAGS)
])
