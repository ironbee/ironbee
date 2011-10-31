dnl Check for trafficserver
dnl CHECK_TS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  TS
dnl  TS_CFLAGS

TS_CFLAGS=""
TS="no"

AC_DEFUN([CHECK_TS],
[dnl

AC_ARG_WITH(
    trafficserver,
    [AC_HELP_STRING([--with-trafficserver=PATH],[Path to trafficserver])],
    [test_paths="${with_trafficserver}"],
    [test_paths="/usr/local/trafficserver /usr/local /usr /opt"])

AC_MSG_CHECKING([for trafficserver])

ts_path=""
for x in ${test_paths}; do
    if test "${x}" == "no"; then
        ts_path="${x}"
        break
    fi
    if test -e "${x}/include/ts/ts.h"; then
        ts_path="${x}"
        break
    fi
done

if test -n "${ts_path}"; then
    if test "${x}" != "no"; then
        AC_MSG_NOTICE([Building trafficserver plugin for ${ts_path}])
        TS_CFLAGS=" -I${ts_path}/include "
        TS="yes"
    fi
else
    AC_MSG_ERROR([No trafficserver found!  Either specify it or configure without it])
fi
AC_SUBST(TS_CFLAGS)
AC_SUBST(TS)

])
