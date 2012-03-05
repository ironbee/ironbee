dnl Check for OSSP_UUID utility
dnl CHECK_OSSP_UUID(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  OSSP_UUID
dnl  OSSP_UUID_CFLAGS
dnl  OSSP_UUID_LDFLAGS
dnl  OSSP_UUID_LIBS
dnl  OSSP_UUID_MODULES

OSSP_UUID=""
OSSP_UUID_CFLAGS=""
OSSP_UUID_CPPFLAGS=""
OSSP_UUID_LDFLAGS=""
OSSP_UUID_LIBS=""

AC_DEFUN([CHECK_OSSP_UUID],
[dnl

AC_ARG_WITH(
    ossp-uuid,
    [AC_HELP_STRING([--with-ossp-uuid=PATH],[Path to ossp-uuid prefix or script])],
    [test_paths="${with_ossp_uuid}"],
    [test_paths="/usr/local /usr"])

AC_MSG_CHECKING([for ossp-uuid])

for x in ${test_paths}; do
    dnl # Determine if the script was specified and use it directly
    if test ! -d "$x" -a -e "$x"; then
        OSSP_UUID=$x
        ossp_uuid_path=no
        break
    fi

    dnl # Try known config script names/locations
    for OSSP_UUID in uuid-config; do
        if test -e "${x}/sbin/${OSSP_UUID}"; then
            ossp_uuid_path="${x}/sbin"
            break
        elif test -e "${x}/bin/${OSSP_UUID}"; then
            ossp_uuid_path="${x}/bin"
            break
        elif test -e "${x}/${OSSP_UUID}"; then
            ossp_uuid_path="${x}"
            break
        else
            ossp_uuid_path=""
        fi
    done
    if test -n "$ossp_uuid_path"; then
        break
    fi
done

if test -n "${ossp_uuid_path}"; then
    if test "${ossp_uuid_path}" != "no"; then
        OSSP_UUID="${ossp_uuid_path}/${OSSP_UUID}"
    fi
    AC_MSG_RESULT([${OSSP_UUID}])
    OSSP_UUID_CFLAGS=`${OSSP_UUID} --cflags`
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(ossp-uuid CFLAGS: $OSSP_UUID_CFLAGS); fi
    OSSP_UUID_LDFLAGS=`${OSSP_UUID} --ldflags`
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(ossp-uuid LDFLAGS: $OSSP_UUID_LDFLAGS); fi
    OSSP_UUID_LIBS=`${OSSP_UUID} --libs`
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(ossp-uuid LIBS: $OSSP_UUID_LIBS); fi
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(OSSP_UUID)
AC_SUBST(OSSP_UUID_CFLAGS)
AC_SUBST(OSSP_UUID_LDFLAGS)
AC_SUBST(OSSP_UUID_LIBS)

if test -z "${OSSP_UUID}"; then
    AC_MSG_NOTICE([*** ossp-uuid utility not found.])
    ifelse([$2], , AC_MSG_ERR([ossp-uuid library is required]), $2)
else
    AC_MSG_NOTICE([using ossp-uuid ${OSSP_UUID}])
    ifelse([$1], , , $1) 
fi 
])
