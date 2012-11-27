dnl Check for YAJL.
dnl CHECK_YAJL()
dnl Sets:
dnl YAJL_CFLAGS
dnl YAJL_LDFLAGS
dnl HAVE_YAJL

HAVE_YAJL=no
YAJL_CFlAGS=
YAJL_LDFLAGS=

AC_DEFUN([CHECK_YAJL],
[dnl

dnl # Find the yajl libraries using pkg-config.
YAJL_LDADD=`pkg-config --libs yajl`
YAJL_CFLAGS=`pkg-config --cflags yajl`

save_LDFLAGS="$LDFLAGS"
save_CFLAGS="$CFLAGS"
CFLAGS="$YAJL_CFLAGS"
LDFLAGS="$YAJL_LDFLAGS"

AC_CHECK_HEADER([yajl/yajl_version.h])

AC_LANG([C])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[#include <yajl/yajl_common.h>
          #include <yajl/yajl_parse.h>
          #include <yajl/yajl_gen.h>
          #include <yajl/yajl_tree.h>
        ]],
        [[
            yajl_parser_config cfg = { 1, 1 };
            yajl_gen g = yajl_gen_alloc(&cfg, NULL);
        ]]
    )],
    [dnl
        HAVE_YAJL=yes
        LDFLAGS="$save_LDFLAGS $YAJL_LDFLAGS"
        CFLAGS="$save_CFLAGS $YAJL_CFLAGS"
    ],
    [dnl
        LDFLAGS="$save_LDFLAGS"
        CFLAGS="$save_CFLAGS"
    ])


AC_SUBST(HAVE_YAJL)
AC_SUBST(YAJL_CFLAGS)
AC_SUBST(YAJL_LDFLAGS)
])
