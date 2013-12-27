AC_DEFUN([AX_BOOST_THRAD_SUFFIX],
[
AC_ARG_WITH([boost-thread-suffix],
            [AC_HELP_STRING([--with-boost-thread-suffix=suffix suffix name for boost thread libraries])],
            [
                BOOST_THREAD_SUFFIX="$withval"
                findsuffix="no"
            ],[
                findsuffix="yes"
            ])
if test "x$findsuffix" = "xyes"; then
    if test "x$BOOST_LDFLAGS" = "x"; then
        AC_MSG_ERROR(BOOST_LDFLAGS not defined.)
    else
        boostlibpath=`echo $BOOST_LDFLAGS | sed 's/^-L//'`
        changequote(<<, >>)dnl
        candidates=`echo $boostlibpath/libboost_thread* | sed 's|.*libboost_thread||g' | sed 's/\.[^ ]*//g'`
        changequote([, ])dnl

    if test "x$candidates" = "x*"; then
      AC_MSG_ERROR(Unable to find boost thread suffix.  Use --with-boost-thread-suffix to specify.)
        elif `echo $candidates | grep -q ' '`; then
            AC_MSG_ERROR(Multiple candidates found for suffix.  Use --with-boost-thread-suffix to choose one: $candidates)
        else
            BOOST_THREAD_SUFFIX=$candidates
        fi
    fi
fi
AC_SUBST(BOOST_THREAD_SUFFIX)
AC_MSG_NOTICE(Using boost thread suffix: $BOOST_THREAD_SUFFIX)
])
