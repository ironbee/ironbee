dnl Check for APXS utility
dnl CHECK_AXS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  APXS
dnl  APXS_CFLAGS
dnl  APXS_LDFLAGS
dnl  APXS_LIBS
dnl  APXS_MODULES

APXS=""
APXS_CFLAGS=""
APXS_CPPFLAGS=""
APXS_LDFLAGS=""
APXS_LIBS=""

AC_DEFUN([CHECK_APXS],
[dnl

AC_ARG_WITH(
    apxs,
    [AC_HELP_STRING([--with-apxs=PATH],[Path to apxs prefix or script])],
    [test_paths="${with_apxs}"],
    [test_paths="/usr/local/apache22 /usr/local/apache2 /usr/local/apache /usr/local/httpd /usr/local /opt/apache22 /opt/apache2 /opt/apache /opt/httpd /opt/local /opt /usr"])

AC_MSG_CHECKING([for apxs])

for x in ${test_paths}; do
    dnl # Determine if the script was specified and use it directly
    if test ! -d "$x" -a -e "$x"; then
        APXS=$x
        apxs_path=no
        break
    fi

    dnl # Try known config script names/locations
    for APXS in apxs2 apxs; do
        if test -e "${x}/sbin/${APXS}"; then
            apxs_path="${x}/sbin"
            break
        elif test -e "${x}/bin/${APXS}"; then
            apxs_path="${x}/bin"
            break
        elif test -e "${x}/${APXS}"; then
            apxs_path="${x}"
            break
        else
            apxs_path=""
        fi
    done
    if test -n "$apxs_path"; then
        break
    fi
done

if test -n "${apxs_path}"; then
    if test "${apxs_path}" != "no"; then
        APXS="${apxs_path}/${APXS}"
    fi
    AC_MSG_RESULT([${APXS}])
    APXS_CFLAGS="-I`${APXS} -q INCLUDEDIR`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(apxs CFLAGS: $APXS_CFLAGS); fi
    APXS_CPPFLAGS=""
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(apxs CPPFLAGS: $APXS_CPPFLAGS); fi
    APXS_LIBDIR=`${APXS} -q LIBDIR`
    if test -d "${APXS_LIBDIR}"; then
        APXS_LDFLAGS="-L${APXS_LIBDIR}"
    else
        APXS_LDFLAGS=""
    fi
    APXS_LDFLAGS="`$APXS -q EXTRA_LDFLAGS` ${APXS_LDFLAGS}"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(apxs LDFLAGS: $APXS_LDFLAGS); fi
    APXS_LIBS="`$APXS -q LIBS` `$APXS -q EXTRA_LIBS`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(apxs LIBS: $APXS_LIBS); fi
    APXS_MODULES="`$APXS -q LIBEXECDIR`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(apxs MODULES: $APXS_MODULES); fi
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(APXS)
AC_SUBST(APXS_CFLAGS)
AC_SUBST(APXS_CPPFLAGS)
AC_SUBST(APXS_LDFLAGS)
AC_SUBST(APXS_LIBS)
AC_SUBST(APXS_MODULES)

if test -z "${APXS}"; then
    AC_MSG_NOTICE([*** apxs utility not found.])
    ifelse([$2], , AC_MSG_NOTICE([apxs library is required to build HTTPD plug-in]), $2)
else
    AC_MSG_NOTICE([using apxs ${APXS}])
    ifelse([$1], , , $1) 
fi 
])
