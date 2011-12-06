AC_DEFUN([CHECK_GEOIP],
[
AC_MSG_NOTICE([Checking for GeoIP.])
AC_MSG_CHECKING([GeopIP headers])
AC_CHECK_HEADERS([GeoIP.h],
                 [],
                 [AC_MSG_ERROR([missing])])
AC_CHECK_HEADERS([GeoIPCity.h],
                 [],
                 [AC_MSG_ERROR([missing])])
AC_MSG_CHECKING([GeoIP libraries])
AC_CHECK_LIB([GeoIP],
             [GeoIP_record_by_addr],
             [],
             [AC_MSG_ERROR([missing])])
])


