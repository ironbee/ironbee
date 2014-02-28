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
PROTOBUF_LIBRARY_PATH=""
PROTOC="protoc"

AC_DEFUN([CHECK_PROTOBUF],
[dnl

CPPFLAGS_BACKUP=${CPPFLAGS}
CFLAGS_BACKUP=${CFLAGS}
LDFLAGS_BACKUP=${LDFLAGS}

AC_LANG_PUSH(C++)

AC_ARG_WITH(protoc,
            [  --with-protoc=PATH  Path to protoc directory],
            [with_protoc="$withval"],
            [with_protoc="no"])
AC_ARG_WITH(protobuf_includes,
            [  --with-protobuf-includes=PATH   Path to protobuf include directory],
            [with_protobuf_includes="${withval}"],
            [with_protobuf_includes="no"])
AC_ARG_WITH(protobuf_libraries,
            [  --with-protobuf-libraries=PATH  Path to protobuf library directory],
            [with_protobuf_libraries="${withval}"],
            [with_protobuf_libraries="no"])

dnl protc
if test "$with_protoc" = "no"; then
   with_protoc=$PATH
   protoc_specified="no"
else
   protoc_specified="yes"
fi

AC_PATH_PROG(PROTOC, protoc, no, $with_protoc)
if test "$PROTOC" = "no"; then
    if test "$protoc_specified" = "yes"; then
        AC_MSG_ERROR([The path specified for protoc doesn't seem to contain the protoc executable.  Please re-check the supplied path."])
    else
        AC_MSG_ERROR([Please install google protobuf.  If you have protobuf installed in a non-standard location, please specify a path to protoc using --with-protoc=PATH."])
    fi
fi

dnl Headers
if test "$with_protobuf_includes" != "no"; then
   PROTOBUF_CPPFLAGS="-I${with_protobuf_includes}"
   CPPFLAGS="${CPPFLAGS} -I${with_protobuf_includes}"
   CFLAGS="${CFLAGS} -I${with_protobuf_includes}"
fi

AC_CHECK_HEADER(google/protobuf/descriptor.h,HAVE_PROTOBUF="yes",HAVE_PROTOBUF="no")
if test "$HAVE_PROTOBUF" = "no"; then
    if test "$with_protobuf_includes" = "no"; then
        AC_MSG_ERROR([Unable to locate protobuf header files.  If you have installed it in a non-standard location, please specify the headers include path using --with-protobuf-includes.])
    else
        AC_MSG_ERROR([The path specified for protobuf header files using --with-protobuf-includes doesn't contain the protobuf header files.  Please re-check the supplied path.])
    fi
fi
AS_UNSET([ac_cv_header_google_protobuf_descriptor_h])

dnl Library
if test "$with_protobuf_libraries" != "no"; then
    PROTOBUF_LDFLAGS="-L${with_protobuf_libraries}"
    LDFLAGS="${LDFLAGS} -L${with_protobuf_libraries}"
    PROTOBUF_LIBRARY_PATH="${with_protobuf_libraries}"
fi

HAVE_PROTOBUF="no"
LDFLAGS="$LDFLAGS -lprotobuf"
AC_LINK_IFELSE([AC_LANG_PROGRAM([],[])],HAVE_PROTOBUF="yes",HAVE_PROTOBUF="no")
if test "$HAVE_PROTOBUF" = "no"; then
    if test "$with_protobuf_libraries" = "no"; then
        AC_MSG_ERROR([Unable to locate protobuf library.  If you have installed it in a non-standard location, please specify the library path using --with-protobuf-libraries.])
    else
        AC_MSG_ERROR([The path specified for protobuf libraries using --with-protobuf-libraries doesn't contain the library files.  Please re-check the supplied path.])
    fi
fi

CPPFLAGS=${CPPFLAGS_BACKUP}
CFLAGS=${CFLAGS_BACKUP}
LDFLAGS=${LDFLAGS_BACKUP}

AC_LANG_POP

AC_SUBST(HAVE_PROTOBUF)
AC_SUBST(PROTOBUF_CFLAGS)
AC_SUBST(PROTOBUF_CPPFLAGS)
AC_SUBST(PROTOBUF_LDFLAGS)
AC_SUBST(PROTOBUF_LDADD)
AC_SUBST(PROTOC)
AC_SUBST(PROTOBUF_LIBRARY_PATH)

])
