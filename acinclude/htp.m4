dnl Check for HTP Libraries
dnl CHECK_HTP(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  HTP_CFLAGS
dnl  HTP_LIBS

HTP_CONFIG=""
HTP_VERSION=""
HTP_CFLAGS=""
HTP_CPPFLAGS=""
HTP_LDADD=""
HTP_LDFLAGS=""
HTP_CONFIG=pkg-config
HTP_PKGNAMES="htp htp1"
HTP_SONAMES="so la sl dll dylib"

AC_DEFUN([CHECK_HTP],
[dnl

AC_ARG_WITH(
    htp,
    [AC_HELP_STRING([--with-htp=PATH],[Path to htp prefix or config script])],
    [test_paths="${with_htp}"],
    [test_paths="/usr/local/libhtp /usr/local/htp /usr/local /opt/libhtp /opt/htp /opt/local /opt /usr"; ])

AC_MSG_CHECKING([for libhtp config script])

for x in ${test_paths}; do
    dnl # Determine if the script was specified and use it directly
    if test ! -d "$x" -a -e "$x"; then
        HTP_CONFIG=$x
        break
    fi

    dnl # Try known config script names/locations
    for y in $HTP_CONFIG; do
        if test -e "${x}/bin/${y}"; then
            HTP_CONFIG="${x}/bin/${y}"
            htp_config="${HTP_CONFIG}"
            break
        elif test -e "${x}/${y}"; then
            HTP_CONFIG="${x}/${y}"
            htp_config="${HTP_CONFIG}"
            break
        fi
    done
    if test -n "${htp_config}"; then
        break
    fi
done

dnl # Try known package names
if test -n "${HTP_CONFIG}"; then
    HTP_PKGNAME=""
    for x in ${HTP_PKGNAMES}; do
        if ${HTP_CONFIG} --exists ${x}; then
            HTP_PKGNAME="$x"
            break
        fi
    done
fi

if test -n "${HTP_PKGNAME}"; then
    AC_MSG_RESULT([${HTP_CONFIG}])
    HTP_VERSION="`${HTP_CONFIG} ${HTP_PKGNAME} --modversion`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(htp VERSION: $HTP_VERSION); fi
    HTP_CFLAGS="`${HTP_CONFIG} ${HTP_PKGNAME} --cflags-only-I`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(htp CFLAGS: $HTP_CFLAGS); fi
    HTP_CPPFLAGS="`${HTP_CONFIG} ${HTP_PKGNAME} --cflags-only-other`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(htp CPPFLAGS: $HTP_CPPFLAGS); fi
    HTP_LDADD="`${HTP_CONFIG} ${HTP_PKGNAME} --libs-only-l`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(htp LDADD: $HTP_LDADD); fi
    HTP_LDFLAGS="`${HTP_CONFIG} ${HTP_PKGNAME} --libs-only-L --libs-only-other`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(htp LDFLAGS: $HTP_LDFLAGS); fi
else
    AC_MSG_RESULT([no])

    dnl Hack to just try to find the lib and include
    AC_MSG_CHECKING([for htp install])
    for x in ${test_paths}; do
        for y in ${HTP_SONAMES}; do
            if test -e "${x}/libhtp.${y}"; then
                htp_lib_path="${x}"
                htp_lib_name="htp"
                break
            elif test -e "${x}/lib/libhtp.${y}"; then
                htp_lib_path="${x}/lib"
                htp_lib_name="htp"
                break
            elif test -e "${x}/lib64/libhtp.${y}"; then
                htp_lib_path="${x}/lib64"
                htp_lib_name="htp"
                break
            elif test -e "${x}/lib32/libhtp.${y}"; then
                htp_lib_path="${x}/lib32"
                htp_lib_name="htp"
                break
            else
                htp_lib_path=""
                htp_lib_name=""
            fi
        done
        if test -n "$htp_lib_path"; then
            break
        fi
    done
    for x in ${test_paths}; do
        if test -e "${x}/include/htp.h"; then
            htp_inc_path="${x}/include"
            break
        elif test -e "${x}/htp.h"; then
            htp_inc_path="${x}"
            break
        fi

        dnl # Check some sub-paths as well
        for htp_pkg_name in ${htp_lib_name} ${HTP_PKGNAMES}; do
            if test -e "${x}/include/${htp_pkg_name}/htp.h"; then
                htp_inc_path="${x}/include/${htp_pkg_name}"
                break
            elif test -e "${x}/${htp_pkg_name}/htp.h"; then
                htp_inc_path="${x}/${htp_pkg_name}"
                break
            else
                htp_inc_path=""
            fi
        done
        if test -n "$htp_inc_path"; then
            break
        fi
    done
    if test -n "${htp_lib_path}" -a -n "${htp_inc_path}"; then
        HTP_CONFIG=""
        AC_MSG_RESULT([${htp_lib_path} ${htp_inc_path}])
        HTP_VERSION="unknown"
        HTP_CFLAGS="-I${htp_inc_path}"
        HTP_LDADD="-l${htp_lib_name}"
        HTP_LDFLAGS="-L${htp_lib_path}"
    else
        HTP_VERSION=""
        AC_MSG_RESULT([no])
    fi
fi

if test -n "${HTP_LIBS}"; then
    HTP_CPPFLAGS="-DWITH_HTP"
fi

AC_SUBST(HTP_CONFIG)
AC_SUBST(HTP_VERSION)
AC_SUBST(HTP_CFLAGS)
AC_SUBST(HTP_CPPFLAGS)
AC_SUBST(HTP_LDADD)
AC_SUBST(HTP_LDFLAGS)

if test -z "${HTP_VERSION}"; then
  ifelse([$2], , AC_MSG_ERROR([htp library is required]), $2)
else
  AC_MSG_NOTICE([using htp v${HTP_VERSION}])
  ifelse([$1], , , $1) 
fi 
])
