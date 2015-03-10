dnl Check for zlib
dnl Sets:
dnl  HAVE_LIBZ
dnl  LIBZ_CPPFLAGS
dnl  LIBZ_LDFLAGS

AC_DEFUN([CHECK_ZLIB],
[dnl

HAVE_LIBZ="yes"
SAVE_LDFLAGS=$LDFLAGS
SAVE_CPPFLAGS=$CPPFLAGS

AC_ARG_WITH(
    zlib,
    [AC_HELP_STRING([--with-zlib=PATH],[Path to zlib])],
    [test_paths="${with_zlib}"],
    [test_paths="/usr/local /usr /opt/local /opt"])

if test "${test_paths}" != "no"; then
    for x in ${test_paths}; do
        CPPFLAGS="${SAVE_CPPFLAGS} -I${x}/include"
        LDFLAGS="${SAVE_LDFLAGS} -L${x}/lib"
        AC_CHECK_LIB([z], [inflateEnd], [zlib_lib=yes], [zlib_lib=no])
        AC_CHECK_HEADER([zlib.h], [zlib_h="yes"], [zlib_h="no"])
	if test "${zlib_h}" = "yes" && test "${zlib_lib}" = "yes"; then
	    HAVE_LIBZ="yes"
	    LIBZ_CPPFLAGS="-I${x}/include"
            LIBZ_LDFLAGS="-L${x}/$libsubdir -lz"
            break
  	fi
    done
fi

if test "${HAVE_LIBZ}" != "no" ; then
  AC_MSG_NOTICE([Using zlib from ${zlib_path}])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"
LDFLAGS="${SAVE_LDFLAGS}"

AC_SUBST(LIBZ_CPPFLAGS)
AC_SUBST(LIBZ_LDFLAGS)
AC_SUBST(HAVE_LIBZ)
])
