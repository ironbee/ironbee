dnl Check for ATS Libraries
dnl CHECK_ATS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  ATS_CFLAGS
dnl  ATS_LDFLAGS
dnl  ATS_LIBS
dnl  ATS_LINK_LD

ATS_CFLAGS=""
ATS_CPPFLAGS=""
ATS_LDFLAGS=""
ATS_LDADD=""

AC_DEFUN([CHECK_ATS],
[dnl

    AC_ARG_WITH(ats_includes,
            [  --with-ats-includes=DIR  apache traffic server include directory],
            [with_ats_includes="$withval"],[with_ats_includes=no])
    SAVE_CPPFLAGS="${CPPFLAGS}"
    SAVE_CFLAGS="${CFLAGS}"

    if test "$with_ats_includes" != "no"; then
    CPPFLAGS="${CPPFLAGS} -I${with_ats_includes}"
    fi

    AC_CHECK_HEADER(ts/ts.h,,HAVE_ATS="no")
    AM_CONDITIONAL([BUILD_ATS_PLUGIN], [test "$HAVE_ATS" != "no"])

    if test "$HAVE_ATS" != "no"; then
    ATS_CFLAGS="${CFLAGS} -DHAVE_ATS"
    ATS_CPPFLAGS="${CPPFLAGS}"
    fi

    if test "$HAVE_ATS" = "no"; then
    echo
    echo "   Apache Traffic Server not found"
    echo "   IronBee will not build the Apache Traffic Server Plugin"
    echo
    fi

    CPPFLAGS="${SAVE_CPPCFLAGS}"
    CFLAGS="${SAVE_CFLAGS}"

AC_SUBST(ATS_CFLAGS)
AC_SUBST(ATS_CPPFLAGS)
AC_SUBST(ATS_LDFLAGS)
AC_SUBST(ATS_LDADD)

])
