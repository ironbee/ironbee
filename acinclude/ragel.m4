# ==================== Ragel ====================
AC_DEFUN([CHECK_RAGEL],
[
  AC_ARG_WITH([ragel],
              [  --with-ragel=PROG ragel executable],
              [ragel="$withval"],[ragel=no])
  if test "$ragel" != "no"; then
    AC_MSG_NOTICE([Using ragel: $ragel])
    AC_SUBST(RAGEL)
  else
    AC_PATH_PROG([RAGEL], ragel)
  fi

  if test "x$RAGEL" != "x" ; then
	  exactRagelVersion=[$1]
	  AC_MSG_CHECKING([for exact version $ragelVersion])

	  ragelVersion=`$RAGEL --version | awk '{print $ 6}'`

	  AX_COMPARE_VERSION([$ragelVersion],[eq],[$exactRagelVersion],
	       [AC_MSG_RESULT($ragelVersion is ok)],
	       [
		   		AC_MSG_WARN([ragel version must be == $exactRagelVersion - found $ragelVersion])
				RAGEL=""
		   ]
	  )
  fi
  AC_SUBST(RAGEL)
])

