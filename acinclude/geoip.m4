dnl Check for GeoIP
dnl CHECK_GEOIP(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  GEOIP
dnl  GEOIP_CFLAGS

HAVE_GEOIP_H="no"
HAVE_GEOIP_CITY_H="no"
HAVE_GEOIP_LIB="no"
HAVE_GEOIP_DEPS="no"
HAVE_GEOIP_LIB_VERSION="no"

GEOIP_CPPFLAGS=""
GEOIP_LDFLAGS=""
GEOIP_CFLAGS=""

AC_DEFUN([CHECK_GEOIP],
[dnl

AC_ARG_WITH(
    geoip,
    [AC_HELP_STRING([--with-geoip=PATH],[Path to geoip])],
    [test_paths="${with_geoip}"],
    [test_paths="/usr /usr/local /opt/local /opt"])

AC_MSG_CHECKING([for GeoIP])

SAVE_CPPFLAGS="${CPPFLAGS}"
SAVE_LDFLAGS="${LDFLAGS}"

if test "${test_paths}" != "no"; then
    geoip_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${CPPFLAGS} -I${x}/include"
        LDFLAGS="${LDFLAGS} -L${x}/lib"
        AC_CHECK_LIB(GeoIP,GeoIP_record_by_addr,HAVE_GEOIP_LIB="yes",HAVE_GEOIP_LIB="no")
        AC_CHECK_LIB(GeoIP,GeoIP_lib_version,HAVE_GEOIP_LIB_VERSION="yes",HAVE_GEOIP_LIB_VERSION="no")
        AC_CHECK_HEADERS(GeoIP.h,HAVE_GEOIP_H="yes",HAVE_GEOIP_H="no")
        AC_CHECK_HEADERS(GeoIPCity.h,HAVE_GEOIP_CITY_H="yes",HAVE_GEOIP_CITY_H="no")
        if test "$HAVE_GEOIP_H" != "no" && test "$HAVE_GEOIP_LIB" != "no" && test "$HAVE_GEOIP_CITY_H" != "no"; then
            geoip_path="${x}"
            HAVE_GEOIP_DEPS="yes"
            break
        fi
    done

    if test "$HAVE_GEOIP_DEPS" != "no"; then
        AC_MSG_NOTICE([Building GeoIP support])
        GEOIP_CPPFLAGS=" -I${geoip_path}/include"
        GEOIP_LDFLAGS=" -L${geoip_path}/lib"

        dnl on some platfroms without the following compilation fails with ‘GeoIPRecord’ has no member named ‘metro_code’
        GEOIP_CFLAGS=" -fms-extensions"

        dnl we need this check as confidence factor items were added in 1.4.7 
        if test "$HAVE_GEOIP_LIB_VERSION" != "no"; then
            AC_DEFINE([GEOIP_HAVE_VERSION], [1], [Have the GeoIP_lib_version function included in GeoIP >= 1.4.7])
        fi
    else
        if test -z "${with_geoip}"; then
            AC_MSG_NOTICE([Not building with Maxmind GeoIP support.])
        else 
            AC_MSG_ERROR([Maxmind GeoIP not found!  Either specify it or configure without it])
        fi
    fi
else
    AC_MSG_NOTICE([Not building with Maxmind GeoIP support])
fi

AM_CONDITIONAL([BUILD_GEOIP], [test "$HAVE_GEOIP_DEPS" != "no"])

CPPFLAGS="${SAVE_CPPFLAGS}"
LDFLAGS="${SAVE_LDFLAGS}"

AC_SUBST(GEOIP_CPPFLAGS)
AC_SUBST(GEOIP_LDFLAGS)
AC_SUBST(GEOIP_CFLAGS)
])
