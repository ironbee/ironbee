dnl Check for Zorba Libraries if C++ is enabled.
dnl CHECK_ZORBA(ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND])
dnl Sets:
dnl  ZORBA_CXXFLAGS
dnl  ZORBA_LIBS
dnl  ZORBA_VERSION

ZORBA_CPPFLAGS=
ZORBA_LIBS=
ZORBA_VERSION=

AC_DEFUN([CHECK_ZORBA],
[dnl

AC_ARG_WITH(
    zorba,
    [AC_HELP_STRING([--with-zorba=PATH],[Path to zorba prefix or config script])],
    [test_paths="${with_zorba}"],
    [test_paths="/usr/local /usr /opt/local /opt"])

AC_MSG_CHECKING([for zorba])

for test_path in $test_paths; do
    if test -z "$ZORBA_VERSION"; then
        if test -n "${test_path}"; then

            OLD_LIBS="$LIBS"
            OLD_CPPFLAGS="$CPPFLAGS"

            LIBS="-lzorba_simplestore $LIBS"
            CPPFLAGS="-I$test_path/include $CPPFLAGS"

            AC_LANG([C++])
            AC_RUN_IFELSE(
                [
                    AC_LANG_PROGRAM(
                        [
                            #include <iostream>
                            #include <zorba/version.h>
                        ],
                        [
                            zorba::Version v;
                            std::cout << v.getVersion() << std::endl;
                        ]
                    )
                ],
                [
                    export ZORBA_VERSION=`./conftest$EXEEXT`
                ],
                [
                    LIBS="$OLD_LIBS"
                    CPPFLAGS="$OLD_CPPFLAGS"
                ]
            )
        fi
    fi
done

if test -z "${ZORBA_VERSION}"; then
    AC_MSG_NOTICE([no])
fi

])