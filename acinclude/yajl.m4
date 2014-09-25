dnl Check for YAJL.
dnl CHECK_YAJL([CALL-ON-SUCCESS [, CALL-ON-FAILURE]])
dnl Sets:
dnl YAJL_CFLAGS
dnl YAJL_LDFLAGS
dnl HAVE_YAJL

AC_DEFUN([CHECK_YAJL],
[dnl

test_paths="/usr/local /opt/local /opt /usr"
AC_ARG_WITH(
    yajl,
    [AC_HELP_STRING([--with-yajl=PATH], [Path to yajl])],
    [if test "${with_yajl}" == "yes" ; then
       require_yajl="yes"
     else
       test_paths="${with_yajl}"
     fi],
    [require_yajl="no"])

AC_MSG_CHECKING([for yajl])

HAVE_YAJL=no
YAJL_CFlAGS=
YAJL_LDFLAGS=

save_LDFLAGS="$LDFLAGS"
save_CFLAGS="$CFLAGS"

if test "${test_paths}" != "no"; then
    yajl_path=""
    for x in ${test_paths}; do
	TMP_CFLAGS="-I${x}/include"
	TMP_LDFLAGS="-L${x}/$libsubdir -lyajl"
        CFLAGS="${save_CFLAGS} ${TMP_CFLAGS}"
        LDFLAGS="${save_LDFLAGS} ${TMP_LDFLAGS}"

        AC_LANG([C])
        AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM(
                [[
                    #include <yajl/yajl_parse.h>
                    #include <yajl/yajl_gen.h>
                    #include <yajl/yajl_tree.h>
                ]],
                [[
                    yajl_gen g = yajl_gen_alloc(NULL);
                    yajl_handle h = yajl_alloc(NULL, NULL, NULL);
                    yajl_parse(h, (const unsigned char*)"{\"k\":\"v\"}", 9);
                    yajl_free(h);
                    yajl_gen_free(g);
                ]]
            )],
            [dnl
                AC_MSG_RESULT([yes])
                HAVE_YAJL=yes
		YAJL_CFLAGS="${TMP_CFLAGS}"
		YAJL_LDFLAGS="${TMP_LDFLAGS}"
                $1
                break
            ],
            [dnl
                AC_MSG_RESULT([no])
		LDFLAGS="$save_LDFLAGS"
		CFLAGS="$save_CFLAGS"
                $2
            ])
    done

    dnl # Fail if the user asked for YAJL explicitly.
    if test "${require_yajl}" == yes && test "${HAVE_YAJL}" != "yes"; then
        AC_MSG_ERROR([not found])
    fi
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(HAVE_YAJL)
AC_SUBST(YAJL_CFLAGS)
AC_SUBST(YAJL_LDFLAGS)
])
