dnl Check for libmodpbase64
dnl CHECK_MODP(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])

HAVE_MODP="no"
MODP_CFLAGS=""
MODP_CPPFLAGS=""
MODP_LDFLAGS=""
MODP_LDADD=""

AC_DEFUN([CHECK_MODP],
[dnl

AC_ARG_WITH(
    modp,
    [AC_HELP_STRING([--with-modp=PATH],[Path to modp])],
    [test_paths="${with_modp}"],
    [test_paths="/usr/local /opt/local /opt /usr"])

SAVE_CPPFLAGS="${CPPFLAGS}"

if test "${test_paths}" != "no"; then
    modp_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${SAVE_CPPFLAGS} -I${x}/include"
        AC_CHECK_HEADER(modp_burl.h,HAVE_MODP="yes",HAVE_MODP="no")
        if test "$HAVE_MODP" != "no"; then
            modp_path="${x}"
            MODP_CPPFLAGS="-I${x}/include"
            MODP_LDFLAGS="-L${x}/$libsubdir"
            break
        fi
        AS_UNSET([ac_cv_header_modp_h])
    done
fi

if test "${HAVE_MODP}" != "no" ; then
  AC_MSG_NOTICE([Using modp from ${modp_path}])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"

AC_SUBST(HAVE_MODP)
AC_SUBST(MODP_CFLAGS)
AC_SUBST(MODP_CPPFLAGS)
AC_SUBST(MODP_LDFLAGS)
AC_SUBST(MODP_LDADD)

])
