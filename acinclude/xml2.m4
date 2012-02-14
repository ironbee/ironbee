dnl Check for XML2 Libraries
dnl CHECK_XML2(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  XML2_CFLAGS
dnl  XML2_LIBS

XML2_CONFIG=""
XML2_VERSION=""
XML2_CFLAGS=""
XML2_CPPFLAGS=""
XML2_LDADD=""
XML2_LDFLAGS=""

AC_DEFUN([CHECK_XML2],
[dnl

AC_ARG_WITH(
    xml2,
    [AC_HELP_STRING([--with-xml2=PATH],[Path to xml2 prefix or config script])],
    [test_paths="${with_xml}"],
    [test_paths="/usr/local/xml2 /usr/local/xml2 /usr/local/xml /usr/local /opt/xml2 /opt/xml /opt/xml2 /opt/xml /opt /usr"])

AC_MSG_CHECKING([for xml2 config script])

for x in ${test_paths}; do
    dnl # Determine if the script was specified and use it directly
    if test ! -d "$x" -a -e "$x"; then
        XML2_CONFIG=$x
        xml2_path="no"
        break
    fi

    dnl # Try known config script names/locations
    for XML2_CONFIG in xml2-config xml-2-config xml-config; do
        if test -e "${x}/bin/${XML2_CONFIG}"; then
            xml2_path="${x}/bin"
            break
        elif test -e "${x}/${XML2_CONFIG}"; then
            xml2_path="${x}"
            break
        else
            xml2_path=""
        fi
    done
    if test -n "$xml2_path"; then
        break
    fi
done

if test -n "${xml2_path}"; then
    if test "${xml2_path}" != "no"; then
        XML2_CONFIG="${xml2_path}/${XML2_CONFIG}"
    fi
    AC_MSG_RESULT([${XML2_CONFIG}])
    XML2_VERSION="`${XML2_CONFIG} --version`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(xml VERSION: $XML2_VERSION); fi
    XML2_CFLAGS="`${XML2_CONFIG} --cflags`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(xml CFLAGS: $XML2_CFLAGS); fi
    XML2_LDADD="`${XML2_CONFIG} --libs`"
    if test "$verbose_output" -eq 1; then AC_MSG_NOTICE(xml LDADD: $XML2_LDADD); fi
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(XML2_CONFIG)
AC_SUBST(XML2_VERSION)
AC_SUBST(XML2_CFLAGS)
AC_SUBST(XML2_CPPFLAGS)
AC_SUBST(XML2_LDADD)
AC_SUBST(XML2_LDFLAGS)

if test -z "${XML2_VERSION}"; then
    AC_MSG_NOTICE([*** xml library not found.])
    ifelse([$2], , AC_MSG_ERROR([xml2 is required]), $2)
else
    AC_MSG_NOTICE([using xml2 v${XML2_VERSION}])
    ifelse([$1], , , $1) 
fi 
])
