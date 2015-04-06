dnl Check for Lib CURL library.
dnl CHECK_LIBCURL([CALL-ON-SUCCESS [, CALL-ON-FAILURE]])
dnl Sets:
dnl LIBCURL_CFLAGS
dnl LIBCURL_LDFLAGS
dnl HAVE_CURL_H
dnl HAVE_LIBCURL

HAVE_LIBCURL=no
LIBCURL_CFLAGS=
LIBCURL_LDFLAGS=

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
        AC_DEFINE([HAVE_LIBCURL], [1], [Is libcurl included in ironbee.])
        HAVE_LIBCURL=yes
        $1
    ],
    [dnl
        dnl # Replace the original values.
        HAVE_LIBCURL=no
        $2
    ])

LDFLAGS="$save_LDFLAGS"
CFLAGS="$save_CFLAGS"

AC_SUBST(HAVE_CURL_H)
AC_SUBST(HAVE_LIBCURL)
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LDFLAGS)
])
