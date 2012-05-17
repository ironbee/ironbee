dnl Check for libnet
dnl CHECK_NET(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])

HAVE_NET="no"
NET_CFLAGS=""
NET_CPPFLAGS=""
NET_LDFLAGS=""
NET_LDADD=""

AC_DEFUN([CHECK_NET],
[dnl

AC_ARG_WITH(
    net,
    [AC_HELP_STRING([--with-libnet=PATH],[Path to libnet])],
    [test_paths="${with_libnet}"],
    [test_paths="/usr/local /opt/local /opt /usr"])

SAVE_CPPFLAGS="${CPPFLAGS}"

if test "${test_paths}" != "no"; then
    libnet_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${SAVE_CPPFLAGS} -I${x}/include"
        AC_CHECK_HEADER(libnet.h,HAVE_NET="yes",HAVE_NET="no")
        if test "$HAVE_NET" != "no"; then
            libnet_path="${x}"
            NET_CPPFLAGS="-I${x}/include"
            NET_LDFLAGS="-L${x}/$libsubdir"
            break
        fi
        AS_UNSET([ac_cv_header_libnet_h])
    done
fi

if test "${HAVE_NET}" != "no" ; then
  AC_MSG_NOTICE([Using libnet from ${libnet_path}])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"

AC_SUBST(HAVE_NET)
AC_SUBST(NET_CFLAGS)
AC_SUBST(NET_CPPFLAGS)
AC_SUBST(NET_LDFLAGS)
AC_SUBST(NET_LDADD)

])
