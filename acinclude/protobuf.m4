dnl Check for protobuf
dnl CHECK_PROTOBUF(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  PROTOBUF
dnl  PROTOBUF_CFLAGS

HAVE_PROTOBUF="no"
PROTOBUF_CFLAGS=""
PROTOBUF_CPPFLAGS=""
PROTOBUF_LDFLAGS=""
PROTOBUF_LDADD=""

AC_DEFUN([CHECK_PROTOBUF],
[dnl

AC_ARG_WITH(
    protobuf,
    [AC_HELP_STRING([--with-protobuf=PATH],[Path to protobuf])],
    [test_paths="${with_protobuf}"],
    [test_paths="/usr/local/protobuf /usr/local /opt/protobuf /opt/local /opt /usr"])

SAVE_CPPFLAGS="${CPPFLAGS}"
AC_LANG_PUSH(C++)

if test "${test_paths}" != "no"; then
    protobuf_path=""
    for x in ${test_paths}; do
        CPPFLAGS="${SAVE_CPPFLAGS} -I${x}/include"
        AC_CHECK_HEADER(google/protobuf/descriptor.h,HAVE_PROTOBUF="yes",HAVE_PROTOBUF="no")
        if test "$HAVE_PROTOBUF" != "no"; then
            protobuf_path="${x}"
            PROTOBUF_CPPFLAGS="-I${x}/include"
            PROTOBUF_LDFLAGS="-L${x}/$libsubdir"
            break
        fi
        AS_UNSET([ac_cv_header_google_protobuf_descriptor_h])
    done
fi

if test "${HAVE_PROTOBUF}" != "no" ; then
  AC_MSG_NOTICE([Using protobuf from ${protobuf_path}])
fi

CPPFLAGS="${SAVE_CPPFLAGS}"
AC_LANG_POP

AC_SUBST(HAVE_PROTOBUF)
AC_SUBST(PROTOBUF_CFLAGS)
AC_SUBST(PROTOBUF_CPPFLAGS)
AC_SUBST(PROTOBUF_LDFLAGS)
AC_SUBST(PROTOBUF_LDADD)

])
