dnl
dnl Qualys WAF
dnl

AC_PREREQ(2.59)

AC_INIT([ironbee], [0.2], [brectanus@qualys.com])

AC_CONFIG_MACRO_DIR([acinclude])
AC_CONFIG_SRCDIR([README])
AC_CONFIG_HEADERS([ironbee_config_auto_gen.h])
AC_CONFIG_AUX_DIR([build])
AC_PREFIX_DEFAULT([/usr/local/ironbee])

AM_INIT_AUTOMAKE([foreign])

dnl Checks for programs.
dnl If CFLAGS and CXXFLAGS are blank these macro's fill in defaults we don't want that
save_CFLAGS="${CFLAGS}"
save_CXXFLAGS="${CXXFLAGS}"
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CXXCPP
CFLAGS="${save_CFLAGS}"
CXXFLAGS="${save_CXXFLAGS}"

AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stdint.h unistd.h sys/param.h sys/time.h time.h sys/mkdev.h sys/sysmacros.h string.h memory.h fcntl.h dirent.h sys/ndir.h ndir.h alloca.h locale.h )

AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM
AC_STRUCT_ST_BLOCKS
AC_FUNC_CLOSEDIR_VOID
AC_CHECK_FUNCS([memset strcasecmp])
AC_CHECK_FUNC(mknod)
    
dnl Checks for typedefs, structures, and compiler characteristics.

dnl check for avaliable gcc optimizations AC_PROG_CC should set GCC to yes
if test "$GCC" = "yes"; then
    sinclude(acinclude/gcc_characteristics.m4)
    GCC_CHARACTERISTICS()
fi

# Determine the OS 
AC_MSG_CHECKING([OS])
OS=`uname -s`
case "$OS" in
        FreeBSD*)
                AC_MSG_RESULT(FreeBSD) 
                OS_FREEBSD="true"
                CPPFLAGS="${CPPFLAGS} -I/usr/local/include -I/usr/local/include/htp"
                LDFLAGS="${LDFLAGS} -L/usr/local/lib"
                AC_PATH_PROG([GCC45BIN], [gcc45],[yes],[no])
                if test "$GCC45BIN" = "yes"; then
                    CC="gcc45"
                    CPP="cpp45"
                else
                    echo
                    echo "   You must have gcc45 to compile on FreeBSD 7/8"
                    echo
                    exit 1
                fi
        ;;
        Darwin*)
                AC_MSG_RESULT(Darwin) 
                OS_DARWIN="true"
                AC_DEFINE(_POSIX_C_SOURCE, [1], [Enable POSIX C source feature test macro])
                AC_DEFINE(_DARWIN_C_SOURCE, [1], [Enable full C features on OSX])
                CPPFLAGS="${CPPFLAGS} -I/usr/local/include -I/opt/local/include"
                LDFLAGS="${LDFLAGS} -L/usr/local/lib -L/opt/local/lib"
                AC_CHECK_LIB(iconv, libiconv_open, LIBS="$LIBS -liconv")
        ;;
        Linux*)
                AC_MSG_RESULT(Linux)
                AC_DEFINE(_GNU_SOURCE, [1], [Enable GNU C source feature test macro])
                OS_LINUX="true"
        ;;
        *)
                AC_MSG_RESULT(no)
                AC_DEFINE(_POSIX_C_SOURCE, [1], [Enable POSIX C source feature test macro])
        ;;
esac
AM_CONDITIONAL([FREEBSD], [test "$OS_FREEBSD" = "true"])
AM_CONDITIONAL([DARWIN], [test "$OS_DARWIN" = "true"])

#We need to call the iconv macro after OS detection for FreeBSD to work properly
sinclude(acinclude/iconv.m4)
sinclude(acinclude/lib-ld.m4)
sinclude(acinclude/lib-link.m4)
sinclude(acinclude/lib-prefix.m4)
AM_ICONV

dnl Checks for library functions.


dnl Configure options
### Dmalloc (dmalloc.com)
AM_WITH_DMALLOC

### Verbose output
AC_ARG_ENABLE(verbose-output,
              AS_HELP_STRING([--enable-verbose-output],
                             [Enable more verbose configure output.]),
[
  if test "$enableval" != "no"; then
    verbose_output=1
  else
    verbose_output=0
  fi
],
[
  verbose_output=0
])

### Debug
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug],
                             [Enable debugging (development use only).]),
[
  debug=$enableval
],
[
  debug="no"
])
if test "$debug" != "no"; then
    IB_DEBUG="-DIB_DEBUG"
else
    IB_DEBUG=
fi

### CLI
AC_ARG_ENABLE(cli,
              AS_HELP_STRING([--enable-cli],
                             [Enable the cli utility.]),,[enable_cli_utility=yes])
AS_IF([test "$enable_cli_utility" = "yes"], [util_cli=1], [util_cli=0])

### Ragel
AC_ARG_WITH([ragel],
            [  --with-ragel=PROG ragel executable],
            [ragel="$withval"],[ragel=no])

if test "$ragel" != "no"; then
    AC_MSG_NOTICE([Using ragel: $ragel])
else
    AC_PATH_PROGS([ragel],[ragel],[])
fi

RAGEL=$ragel

### Doxygen
AC_ARG_WITH([doxygen],
            [  --with-doxygen=PROG doxygen executable],
            [doxygen="$withval"],[doxygen=no])

if test "$doxygen" != "no"; then
    AC_MSG_NOTICE([Using doxygen: $doxygen])
else
    AC_PATH_PROGS([doxygen],[doxygen],[])
fi

DOXYGEN=$doxygen

dnl TODO: Use these to gen doxygen configs
DOXYGEN_INPUT="include util engine modules plugins tests"
DOXYGEN_FILES=`find $DOXYGEN_INPUT -type f -regex '.*\.\(h\|c\|cc\)$' | grep -v 'tests/gtest/\|libs/'`
DOXYGEN_DEPS=""
dnl Make deps into a single line with relative path from docs dir
for f in $DOXYGEN_FILES; do
    DOXYGEN_DEPS="$DOXYGEN_DEPS ../$f"
done

### Java
AC_ARG_WITH([java],
            [  --with-java=PROG java executable],
            [java="$withval"],[java=no])

if test "$java" != "no"; then
    AC_MSG_NOTICE([Using java: $java])
else
    AC_PATH_PROGS([java],[java],[])
fi

JAVA=$java

### Valgrind
AC_ARG_WITH([valgrind],
            [  --with-valgrind=PROG valgrind executable],
            [valgrind="$withval"],[valgrind=no])

if test "$valgrind" = "yes"; then
    AC_PATH_PROGS([valgrind],[valgrind],[])
elif test "$valgrind" != "no"; then
    AC_MSG_NOTICE([Using valgrind: $valgrind])
else
    valgrind=
fi

VALGRIND=$valgrind

dnl Checks for various external dependencies
sinclude(acinclude/htp.m4)
sinclude(acinclude/pcre.m4)
sinclude(acinclude/xml2.m4)
sinclude(acinclude/apxs.m4)
sinclude(acinclude/apr.m4)
sinclude(acinclude/apu.m4)
sinclude(acinclude/httpd.m4)
sinclude(acinclude/trafficserver.m4)
sinclude(acinclude/geoip.m4)

# Required libs
CHECK_HTP()
CHECK_PCRE()
CHECK_XML2()
dnl TODO: These need to be optional (only if building apache httpd plugin)
CHECK_APXS()
CHECK_APR()
CHECK_APU()
CHECK_HTTPD()
CHECK_TS()
CHECK_GEOIP()

#If we are not building plugins bail
if test "$HAVE_HTTPD" = "no" && test "$HAVE_TS" = "no"; then
    echo "**********************************************"
    echo "* Warning! No plugins enabled!               *"
    echo "* We will only be building the cli test tool.*"
    echo "**********************************************"
fi
  
dnl Optional build modules
dnl
dnl TODO: Make these configure opts
dnl
TOPLEVEL_SUBDIRS="libs util engine plugins lua modules etc docs tests"
LIBS_SUBDIRS=""
PLUGINS_SUBDIRS="apache_httpd2 trafficserver"

dnl Enable cli subdir if enabled
if test "$util_cli" != 0; then
    TOPLEVEL_SUBDIRS="${TOPLEVEL_SUBDIRS} cli"
fi

dnl Make sure abs_top_builddir is available
AC_MSG_CHECKING([abs_top_builddir is available])
if test -z "$abs_top_builddir"; then
    abs_top_builddir=`(cd "$top_builddir" && pwd)`
    AC_MSG_RESULT([no, using $abs_top_builddir]) 
    AC_SUBST(abs_top_builddir)
else
    AC_MSG_RESULT([yes, $abs_top_builddir]) 
fi

dnl Make sure abs_top_srcdir is available
AC_MSG_CHECKING([abs_top_srcdir is available])
if test -z "$abs_top_srcdir"; then
    abs_top_srcdir=`(cd "$top_srcdir" && pwd)`
    AC_MSG_RESULT([no, using $abs_top_srcdir]) 
    AC_SUBST(abs_top_srcdir)
else
    AC_MSG_RESULT([yes, $abs_top_srcdir]) 
fi

dnl Substitutions
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(CONFIG_MACRO_DIR)
AC_SUBST(TOPLEVEL_SUBDIRS)
AC_SUBST(LIBS_SUBDIRS)
AC_SUBST(PLUGINS_SUBDIRS)
AC_SUBST(RAGEL)
AC_SUBST(DOXYGEN)
AC_SUBST(DOXYGEN_INPUT)
AC_SUBST(DOXYGEN_DEPS)
AC_SUBST(JAVA)
AC_SUBST(VALGRIND)
AC_SUBST(LDFLAGS)
AC_SUBST(IB_DEBUG)
dnl Generate files
AC_CONFIG_FILES([Makefile])

AC_CONFIG_FILES([libs/Makefile])
AC_CONFIG_FILES([util/Makefile])
AC_CONFIG_FILES([engine/Makefile])

AC_CONFIG_FILES([plugins/Makefile])
AC_CONFIG_FILES([plugins/apache_httpd2/Makefile])
AC_CONFIG_FILES([plugins/trafficserver/Makefile])

if test "$util_cli" != 0; then
AC_CONFIG_FILES([cli/Makefile])
fi

AC_CONFIG_FILES([lua/Makefile])
AC_CONFIG_FILES([modules/Makefile])

AC_CONFIG_FILES([etc/Makefile])

# TODO: Conditional
AC_CONFIG_FILES([docs/Makefile])
AC_CONFIG_FILES([docs/docbook/Makefile])
AC_CONFIG_FILES([docs/docbook/manual/Makefile])

# TODO: Conditional on unit testing
AC_CONFIG_FILES([tests/Makefile])
AC_CONFIG_FILES([tests/gtest/Makefile])

# pkg-config
AC_CONFIG_FILES([ironbee.pc])

AC_OUTPUT
