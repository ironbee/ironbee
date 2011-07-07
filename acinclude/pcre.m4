dnl Check for PCRE Libraries
dnl CHECK_PCRE(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  PCRE_CFLAGS
dnl  PCRE_LIBS

PCRE_CONFIG=""
PCRE_VERSION=""
PCRE_CPPFLAGS=""
PCRE_CFLAGS=""
PCRE_LDFLAGS=""
PCRE_LDADD=""

AC_DEFUN([CHECK_PCRE],
[dnl

AC_ARG_WITH(
    pcre,
    [AC_HELP_STRING([--with-pcre=PATH],[Path to pcre prefix or config script])],
    [test_paths="${with_pcre}"],
    [test_paths="/usr/local/libpcre /usr/local/pcre /usr/local /opt/libpcre /opt/pcre /opt /usr /opt/local"])

AC_MSG_CHECKING([for libpcre config script])

dnl # Determine pcre lib directory
if test -z "${with_pcre}"; then
    test_paths="/usr/local/pcre /usr/local /usr /opt/local"
else
    test_paths="${with_pcre}"
fi

for x in ${test_paths}; do
    dnl # Determine if the script was specified and use it directly
    if test ! -d "$x" -a -e "$x"; then
        PCRE_CONFIG=$x
        pcre_path="no"
        break
    fi

    dnl # Try known config script names/locations
    for PCRE_CONFIG in pcre-config; do
        if test -e "${x}/bin/${PCRE_CONFIG}"; then
            pcre_path="${x}/bin"
            break
        elif test -e "${x}/${PCRE_CONFIG}"; then
            pcre_path="${x}"
            break
        else
            pcre_path=""
        fi
    done
    if test -n "$pcre_path"; then
        break
    fi
done
    CFLAGS=$save_CFLAGS
    LDFLAGS=$save_LDFLAGS

if test -n "${pcre_path}"; then
    if test "${pcre_path}" != "no"; then
        PCRE_CONFIG="${pcre_path}/${PCRE_CONFIG}"
    fi
    AC_MSG_RESULT([${PCRE_CONFIG}])
    PCRE_VERSION="`${PCRE_CONFIG} --version`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(pcre VERSION: $PCRE_VERSION); fi
    PCRE_CFLAGS="`${PCRE_CONFIG} --cflags`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(pcre CFLAGS: $PCRE_CFLAGS); fi
    PCRE_LDADD="`${PCRE_CONFIG} --libs`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(pcre LDADD: $PCRE_LDADD); fi
else
    AC_MSG_RESULT([no])
fi

    #enable suppot for PCRE-sljit http://sljit.sourceforge.net/pcre.html
    AC_ARG_ENABLE(pcre-sljit,
           AS_HELP_STRING([--enable-pcre-sljit], [Enable experimental support for PCRE SLJIT]),,[enable_pcre_sljit=no])
    AS_IF([test "x$enable_pcre_sljit" = "xyes"], [
        AC_MSG_CHECKING(for PCRE SLJIT)
        save_CFLAGS=$CFLAGS
        save_LDFLAGS=$LDFLAGS
        CFLAGS="${PCRE_CFLAGS} ${CFLAGS}"
        LDFLAGS="${PCRE_LDFLAGS} ${CFLAGS}"
        AC_TRY_COMPILE([ #include <pcre.h> ],
            [ const char* patt = "bar";
              pcre *re;
              const char *error;
              pcre_extra *extra;
              int err_offset;
              re = pcre_compile(patt,0, &error, &err_offset,NULL);
              extra = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &error);
              if (!(extra->flags & PCRE_EXTRA_EXECUTABLE_FUNC)) {
                  printf("PCRE-SLJIT compiler does not support: %s\n", patt);
              }
              return 0;],
            [ pcre_sljit_available=yes ], [:]
            )

           if test "x$pcre_sljit_available" = "xyes"; then
               AC_MSG_RESULT(yes)
               PCRE_CFLAGS="${PCRE_CFLAGS} -DPCRE_HAVE_SLJIT"
           else
               AC_MSG_RESULT(no)
               echo
               echo "   Error! --enable-pcre-sljit set but PCRE_STUDY_JIT_COMPILE not found"
               echo "   Make sure you use pcre found here "
               echo "   http://sljit.sourceforge.net/pcre.html"
               echo
               exit 1
           fi
        CFLAGS=$save_CFLAGS
        LDFLAGS=$save_$LDFLAGS
    ])

AC_SUBST(PCRE_CONFIG)
AC_SUBST(PCRE_VERSION)
AC_SUBST(PCRE_CPPFLAGS)
AC_SUBST(PCRE_CFLAGS)
AC_SUBST(PCRE_LDFLAGS)
AC_SUBST(PCRE_LDADD)

if test -z "${PCRE_VERSION}"; then
    AC_MSG_NOTICE([*** pcre library not found.])
    ifelse([$2], , AC_MSG_ERROR([pcre library is required]), $2)
else
    AC_MSG_NOTICE([using pcre v${PCRE_VERSION}])
    ifelse([$1], , , $1) 
fi 
])
