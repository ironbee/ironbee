AC_DEFUN([AX_BOOST_SUFFIX],
[
AC_ARG_WITH([boost-suffix],
            [AC_HELP_STRING([--with-boost-suffix=suffix suffix name for boost libraries])],
            [
				boostsuffix="$withval"
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
		candidates=`echo $boostlibpath/libboost_date_time* | sed 's|.*libboost_date_time||g' | sed 's/\.[^ ]*//g'`
		changequote([, ])dnl
		if `echo $candidates | grep -q ' '`; then
			AC_MSG_ERROR(Multiple candidates found for suffix.  Use --with-boostsuffix to choose one: --$candidates--)
		else
			BOOST_SUFFIX=$candidates
			AC_SUBST(BOOST_SUFFIX)
		fi
	fi
	AC_MSG_NOTICE(Using boost suffix: $BOOST_SUFFIX)
fi
])
