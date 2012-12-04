dnl Check for trafficserver
dnl CHECK_TS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  HAVE_TS
dnl  BUILD_TS_SERVER
dnl  TS_CFLAGS

HAVE_TS="no"
TS_CFLAGS=""
TS_CPPFLAGS=""
TS_LDFLAGS=""
TS_LDADD=""

AC_DEFUN([CHECK_TS],
[dnl

AC_ARG_WITH(
    trafficserver,
    [AC_HELP_STRING([--with-trafficserver=PATH],[Path to trafficserver])],
    [test_paths="${with_trafficserver}"],
    [test_paths="/usr/local/trafficserver /usr/local /opt/trafficserver /opt/local /opt /usr"])

AC_MSG_CHECKING([for trafficserver])

if test "${test_paths}" != "no"; then
    ts_path=""
    for x in ${test_paths}; do
        SAVE_CFLAGS="${CFLAGS}"
        CFLAGS="${CPPFLAGS} -I${x}/include"
        AC_CHECK_HEADER(ts/ts.h,HAVE_TS="yes",HAVE_TS="no")
        CFLAGS="${SAVE_CCFLAGS}"

        if test "$HAVE_TS" != "no"; then
            ts_path="${x}"
            break
        fi
    done

    if test -n "${ts_path}"; then
        AC_MSG_NOTICE([Building trafficserver server plugin for ${ts_path}])
        TS_CFLAGS="-I${ts_path}/include"
    else
        if test -z "${with_trafficserver}"; then
            AC_MSG_NOTICE([Not building trafficserver server plugin.])
        else 
            AC_MSG_ERROR([No trafficserver found!  Either specify it or configure without it])
        fi
    fi
else
    AC_MSG_NOTICE([Not building trafficserver server plugin.])
fi

AM_CONDITIONAL([BUILD_TS_SERVER], [test "$HAVE_TS" != "no"])

AC_SUBST(HAVE_TS)
AC_SUBST(TS_CFLAGS)
AC_SUBST(TS_CPPFLAGS)
AC_SUBST(TS_LDFLAGS)
AC_SUBST(TS_LDADD)

])
