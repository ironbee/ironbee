dnl Check for Apache HTTPD
dnl CHECK_HTTPD(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  BUILD_HTTPD_PLUGIN
dnl  HAVE_HTTPD

HAVE_HTTPD="no"

AC_DEFUN([CHECK_HTTPD],
[dnl

AC_ARG_WITH(
    httpd,
    [AC_HELP_STRING([--with-httpd],[enable support for Apache HTTPD plug-in])],
    [with_httpd_plugin="${with_httpd}"],
    [with_httpd_plugin="yes"])
    
    AC_MSG_CHECKING([for httpd])
if test "${with_httpd_plugin}" != "no"; then
    if test -z "${APXS}"; then
        if test -z "${with_httpd}"; then
            AC_MSG_NOTICE([Not building HTTPD plugin.])
        else 
            AC_MSG_ERROR([No APXS found!  Either specify it or configure --without-httpd])
        fi
    elif test -z "${APU_VERSION}"; then
        if test -z "${with_httpd}"; then
            AC_MSG_NOTICE([Not building HTTPD plugin.])
        else 
            AC_MSG_ERROR([No APU found!  Either specify it or configure --without-httpd])
        fi
    elif test -z "${APR_VERSION}"; then
        if test -z "${with_httpd}"; then
            AC_MSG_NOTICE([Not building HTTPD plugin.])
        else 
            AC_MSG_ERROR([No APU found!  Either specify it or configure --without-httpd])
        fi
    else
        AC_MSG_NOTICE([Building HTTPD plugin])
        HAVE_HTTPD="yes"
    fi 
else
    AC_MSG_NOTICE([Not building HTTPD plugin])
fi

AM_CONDITIONAL([BUILD_HTTPD_PLUGIN], [test "$HAVE_HTTPD" != "no"])
AC_SUBST(HAVE_HTTPD)
])
