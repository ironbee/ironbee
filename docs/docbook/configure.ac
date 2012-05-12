dnl
dnl Qualys WAF
dnl

AC_PREREQ(2.59)

AC_INIT([ironbee-documentation], [0.2], [brectanus@qualys.com])

AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([README])
AC_PREFIX_DEFAULT([/usr/local/ironbee])

AM_INIT_AUTOMAKE([foreign])

### Java
AC_ARG_WITH([java],
            [  --with-java=PROG java executable],
            [java="$withval"],[java=no])

if test "$java" != "no"; then
    AC_MSG_NOTICE([Using java: $java])
else
    AC_PATH_PROGS([java],[java],[])
fi

JAVA=$java

dnl Substitutions
AC_SUBST(JAVA)
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([manual/Makefile])

AC_OUTPUT
