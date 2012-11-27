dnl Check for Lib CURL library.
dnl CHECK_LIBCURL(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl LIBCURL_CFLAGS
dnl LIBCURL_LDADD

HAVE_LIBCURL=no
LIBCURL_CFLAGS=
LIBCURL_LDADD=

AC_DEFUN([CHECK_LIBCURL],
[dnl
dnl # Find the libraries.
LIBCURL_LDFLAGS=`curl-config --libs`
LIBCURL_CFLAGS=`curl-config --cflags`

dnl # Save flags for test compilation.
save_LDFLAGS="$LDFLAGS"
save_CFLAGS="$CFLAGS"
CFLAGS="$LIBCURL_CFLAGS"
LDFLAGS="$LIBCURL_LDFLAGS"

dnl # Quick check for a header.
AC_CHECK_HEADER([curl/curl.h])

AC_LANG([C])

AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[#include <curl/curl.h>
          #include <curl/curlver.h>]],
        [[curl_global_init(0);]]
    )],
    [dnl
        HAVE_LIBCURL=yes
        LDFLAGS="$save_LDFLAGS $LIBCURL_LDFLAGS"
        CFLAGS="$save_CFLAGS $LIBCURL_CFLAGS"
    ],
    [dnl
        dnl # Replace the original values.
        LDFLAGS="$save_LDFLAGS"
        CFLAGS="$save_CFLAGS"
    ])

AC_SUBST(HAVE_CURL_H)
AC_SUBST(HAVE_LIBCURL)
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LDFLAGS)
])
