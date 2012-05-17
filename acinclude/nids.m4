dnl Check for libnids
dnl CHECK_NIDS(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])

HAVE_NIDS="no"
NIDS_CFLAGS=""
NIDS_CPPFLAGS=""
NIDS_LDFLAGS=""
NIDS_LDADD=""

AC_DEFUN([CHECK_NIDS],
[dnl

AC_ARG_WITH(
    nids,
    [AC_HELP_STRING([--with-libnids=PATH],[Path to libnids])],
    [test_paths="${with_libnids}"],
    [test_paths="/usr/local /opt/local /opt /usr"])

SAVE_CPPFLAGS="${CPPFLAGS}"

if test "${test_paths}" != "no"; then
    libnids_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${SAVE_CPPFLAGS} -I${x}/include"
        AC_CHECK_HEADER(nids.h,HAVE_NIDS="yes",HAVE_NIDS="no")
        if test "$HAVE_NIDS" != "no"; then
            libnids_path="${x}"
            NIDS_CPPFLAGS="-I${x}/include"
            NIDS_LDFLAGS="-L${x}/$libsubdir"
            break
        fi
        AS_UNSET([ac_cv_header_nids_h])
    done
fi

if test "${HAVE_NIDS}" != "no" ; then
  AC_MSG_NOTICE([Using libnids from ${libnids_path}])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"

AC_SUBST(HAVE_NIDS)
AC_SUBST(NIDS_CFLAGS)
AC_SUBST(NIDS_CPPFLAGS)
AC_SUBST(NIDS_LDFLAGS)
AC_SUBST(NIDS_LDADD)

])
