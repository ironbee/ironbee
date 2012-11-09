#
# AC_RPM_CHECK
# Looks for the rpm program, and checks to see if we can redefine "_topdir".
#
AC_DEFUN([AC_RPM_CHECK],
[
	AC_CHECK_PROG(ac_have_rpm, rpm, "yes", "no")
	if test "x$ac_have_rpm" = "xyes"; then
		rpm --define '_topdir /tmp' > /dev/null 2>&1
		AC_MSG_CHECKING(to see if we can redefine _topdir)
		if test $? -eq 0 ; then
			AC_MSG_RESULT(yes)
			HAVE_RPM=yes
		else
			AC_MSG_RESULT(no.  You'll have to build packages manually.)
			HAVE_RPM=no
		fi
	fi
])
