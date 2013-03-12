dnl Check for tengine's dso_tool
dnl CHECK_HTTPD(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  NGX_DSO_TOOL

HAVE_TENGINE="no"

AC_DEFUN([CHECK_NGX_DSO_TOOL],
[dnl

AC_ARG_WITH(
    ngx_dso_tool,
    [AC_HELP_STRING([--with-ngx_dso_tool=PATH],
                    [configure tengine's dso_tool to build nginx module as a shared lib])],
    [test_paths="${with_ngx_dso_tool}"],
    [test_paths="/usr /usr/local /usr/local/nginx /usr/local/tengine /opt /opt/nginx /opt/tengine"]
  )
    
    AC_MSG_CHECKING([for ngx dso_tool])


if test "${with_ngx_dso_tool}" != "no"; then
    for x in ${test_paths}; do
        if test -x "$x" -a -f "$x" ; then
            NGX_DSO_TOOL=$x
            HAVE_TENGINE="yes"
            break
        elif test -x "$x/sbin/dso_tool" ; then
            NGX_DSO_TOOL="$x/sbin/dso_tool"
            HAVE_TENGINE="yes"
            break
        fi
    done
    if test "${HAVE_TENGINE}" = "yes"; then
        AC_MSG_NOTICE([Configuring dynamic ngx/tengine module with ${NGX_DSO_TOOL}])
    else
        AC_ERROR([No dso_tool found: cannot configure for ngx/tengine module])
    fi
else
    AC_MSG_NOTICE([Not configuring dynamic ngx/tengine module])
fi

AC_SUBST(NGX_DSO_TOOL)
])
