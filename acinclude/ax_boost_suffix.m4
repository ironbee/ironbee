AC_DEFUN([AX_BOOST_HAS_SUFFIX], [
    want_suffix=$1
    lib=$4
    if test "x$lib" = "x"; then
        lib="boost_date_time"
    fi
    AC_LANG(C++)
    SAVED_LDFLAGS=$LDFLAGS
    LDFLAGS="$LDFLAGS $BOOST_LDFLAGS -l$lib$1"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])], [$2], [$3])
    LDFLAGS=$SAVED_LDFLAGS
])

AC_DEFUN([AX_BOOST_SUFFIX],
[
AC_ARG_WITH([boost-suffix],
            [AC_HELP_STRING([--with-boost-suffix=suffix suffix name for boost libraries])],
            [
                BOOST_SUFFIX="$withval"
                findsuffix="no"
            ],[
                findsuffix="yes"
            ])
AC_MSG_CHECKING([boost library suffix])
if test "x$findsuffix" = "xyes"; then
    AX_BOOST_HAS_SUFFIX("-mt", [BOOST_SUFFIX="-mt"], [
        AX_BOOST_HAS_SUFFIX("", [BOOST_SUFFIX=""], [
            AC_MSG_ERROR("Could not determine boost suffix.  Use --with-boost-suffix")
        ])
    ])
fi
AC_SUBST(BOOST_SUFFIX)
AC_MSG_RESULT([$BOOST_SUFFIX])
])

AC_DEFUN([AX_BOOST_THREAD_SUFFIX],
[
AC_ARG_WITH([boost-thread-suffix],
            [AC_HELP_STRING([--with-boost-thread-suffix=suffix suffix name for boost thread library])],
            [
                BOOST_THREAD_SUFFIX="$withval"
                findsuffix="no"
            ],[
                findsuffix="yes"
            ])
AC_MSG_CHECKING([boost thread library suffix])
if test "x$findsuffix" = "xyes"; then
    AX_BOOST_HAS_SUFFIX("-mt", [BOOST_THREAD_SUFFIX="-mt"], [
        AX_BOOST_HAS_SUFFIX("", [BOOST_THREAD_SUFFIX=""], [
            AC_MSG_ERROR("Could not determine boost thread suffix.  Use --with-boost-thread_suffix")
        ], [boost_thread])
    ], [boost_thread])
fi
AC_SUBST(BOOST_THREAD_SUFFIX)
AC_MSG_RESULT([$BOOST_THREAD_SUFFIX])
])
