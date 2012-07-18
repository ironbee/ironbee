dnl
dnl Qualys WAF
dnl

AC_PREREQ(2.59)

AC_INIT([ironbee], [0.6], [ironbee-team@ironbee.com])

AC_CONFIG_MACRO_DIR([acinclude]) AC_CONFIG_SRCDIR([README])
AC_CONFIG_HEADERS([ironbee_config_auto_gen.h])
AC_CONFIG_AUX_DIR([build])
AC_PREFIX_DEFAULT([/usr/local/ironbee])

AM_INIT_AUTOMAKE([foreign])

dnl Checks for programs.
dnl If CFLAGS and CXXFLAGS are blank these macro's fill in defaults we don't want that
OS=`uname -s`

#We need to set/detect CC and CPP piror to calling these macros as AC_PROG_LIBTOOL etc depend on them.
case "$OS" in
        FreeBSD*)
                AC_PATH_PROG(GCC46BIN,gcc46,no)
                AC_PATH_PROG(GCC47BIN,gcc47,no)
                AC_PATH_PROG(GCC45BIN,gcc45,no)
                if test "$GCC45BIN" != "no"; then
                    CC="gcc45"
                    CPP="cpp45"
                    CXX="g++45"
                    LDFLAGS="${LDFLAGS} -Wl,-R/usr/local/lib/gcc45"
                elif test "$GCC46BIN" != "no"; then
                    CC="gcc46"
                    CPP="cpp46"
                    CXX="g++46"
                    LDFLAGS="${LDFLAGS} -Wl,-R/usr/local/lib/gcc46"
                elif test "$GCC47BIN" != "no"; then
                    CC="gcc47"
                    CPP="cpp47"
                    CXX="g++47"
                    LDFLAGS="${LDFLAGS} -Wl,-R/usr/local/lib/gcc47"
                else
                    echo
                    echo "   You must have gcc45/gcc46/gcc47 to compile on FreeBSD 7/8/9"
                    echo
                    exit 1
                fi
        ;;
esac


dnl do gcc-characteristics
AC_ARG_ENABLE(gcc-characteristics,
              AS_HELP_STRING([--disable-gcc-characteristics],
                             [Disable gcc feature and optimization macro (enabled by default).]),[enable_gcc_characteristics="$enableval"],[enable_gcc_characteristics=yes])
save_CFLAGS="${CFLAGS}"
save_CXXFLAGS="${CXXFLAGS}"
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CXXCPP

AC_CHECK_PROG(ruby, 'ruby', 'ruby', '')
if test "$ruby" = ""; then
    AC_MSG_WARN("Ruby required for codecheck.")
fi

dnl if we have clang use the CFLAGS set by autoconf macros.
dnl if not clang assume gcc and use the CFLAGS set by our macro.
if test "$GCC" = "yes"; then
    AC_TRY_COMPILE(,
                   [#ifndef __clang__
                       something invalid
                    #endif
                  return 0;],
                [ cc_is_clang=yes ], [:]
                )
    if test "$cc_is_clang" == "yes"; then
        CFLAGS="${CFLAGS} -Wall -fPIC" 
        CXXFLAGS="${CXXFLAGS} -Wall -fPIC" 
    elif test "$cc_is_clang" != "yes" && test "$enable_gcc_characteristics" = "yes"; then
        CFLAGS="${save_CFLAGS}"
        CXXFLAGS="${save_CXXFLAGS}"
        sinclude(acinclude/gcc_characteristics.m4)
        GCC_CHARACTERISTICS()
    else
        CFLAGS="${CFLAGS} -std=c99"
    fi
fi
AM_CONDITIONAL([CC_IS_CLANG], [test "$cc_is_clang" = "yes"])
AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(stdint.h unistd.h sys/param.h sys/time.h time.h sys/mkdev.h sys/sysmacros.h string.h memory.h fcntl.h dirent.h sys/ndir.h ndir.h alloca.h locale.h arpa/inet.h netinet/in.h)

AC_HEADER_MAJOR
AC_FUNC_ALLOCA
AC_STRUCT_TM
AC_STRUCT_ST_BLOCKS
AC_FUNC_CLOSEDIR_VOID
AC_CHECK_FUNCS([memset strcasecmp])
AC_CHECK_FUNC(mknod)

# Check for -Wno-null-dereference
AC_MSG_CHECKING(for -Wno-null-dereference support)    
save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Werror -Wno-null-dereference"
AC_TRY_COMPILE(,,
  [have_no_warn_null_deref=yes],[have_no_warn_null_deref=no])
CPPFLAGS="${save_CPPFLAGS}"
if test "$have_no_warn_null_deref" == "yes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_NO_WARN_NULL_DEREF], [test "$have_no_warn_null_deref" = "yes"])

# Check for -Wunused-but-set-variable
AC_MSG_CHECKING(for -Wunused-but-set-variable)    
save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Werror -Wunused-but-set-variable"
AC_TRY_COMPILE(,,
  [have_no_unused_but_set_variable=yes],[have_no_unused_but_set_variable=no])
CPPFLAGS="${save_CPPFLAGS}"
if test "$have_no_unused_but_set_variable" == "yes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_NO_UNUSED_BUT_SET_VARIABLE], [test "$have_no_unused_but_set_variable" = "yes"])

AC_MSG_CHECKING([OS])
case "$OS" in
        FreeBSD*)
                AC_MSG_RESULT(FreeBSD) 
                OS_FREEBSD="true"
                CPPFLAGS="${CPPFLAGS} -I/usr/local/include -I/usr/local/include/htp"
                LDFLAGS="${LDFLAGS} -L/usr/local/lib"
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
                AC_CHECK_LIB([dl], [dlopen], [])
                AC_DEFINE(_GNU_SOURCE, [1], [Enable GNU C source feature test macro])
                AC_CHECK_LIB(rt, clock_gettime, LIBS="$LIBS -lrt")
                OS_LINUX="true"
        ;;
        *)
                AC_MSG_RESULT(no)
                AC_DEFINE(_POSIX_C_SOURCE, [1], [Enable POSIX C source feature test macro])
        ;;
esac
AM_CONDITIONAL([FREEBSD], [test "$OS_FREEBSD" = "true"])
AM_CONDITIONAL([DARWIN], [test "$OS_DARWIN" = "true"])
AM_CONDITIONAL([LINUX], [test "$OS_LINUX" = "true"])

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

### Development-only features
AC_ARG_ENABLE(devel,
              AS_HELP_STRING([--enable-devel],
                             [Enable development features (development use only).]),
[
  devel=$enableval
],
[
  devel="no"
])

AM_CONDITIONAL([BUILD_DEV_MODULES], [test "$devel" != "no"])

### CLI
AC_ARG_ENABLE(cli,
              AS_HELP_STRING([--disable-cli],
                             [Disable the cli utility (enabled by default).]),[enable_cli_utility="$enableval"],[enable_cli_utility=yes])
AS_IF([test "$enable_cli_utility" = "yes"], [util_cli=1], [util_cli=0])

### CPP
AC_ARG_ENABLE(cpp,
              AS_HELP_STRING([--enable-cpp],
                             [Enable C++ code (disabled by default).]),[enable_cpp="$enableval"],[enable_cpp=no])
AS_IF([test "$enable_cpp" = "yes"], [cpp_code=1], [cpp_code=0])
AM_CONDITIONAL([CPP], [test "$enable_cpp" = "yes"])
ironbeepp_includedir='${prefix}/include/ironbeepp'
ironbeepp_internal_includedir='${prefix}/include/ironbeepp/internal'
AC_SUBST(ironbeepp_includedir)
AC_SUBST(ironbeepp_internal_includedir)

### Development-only features
AC_ARG_ENABLE(clipp,
              AS_HELP_STRING([--enable-clipp],
                             [Enable the CPP CLI (clipp).]),
[
  enable_clipp=$enableval
],
[
  enable_clipp="${enable_cpp}"
])

### Disable Lua
AC_ARG_ENABLE(lua,
    AC_HELP_STRING([--disable-lua],
                   [Disable Lua support.]),
    [enable_lua="$enableval"], [enable_lua=yes]
)
AM_CONDITIONAL([ENABLE_LUA], [test "$enable_lua" = "yes"])
if test "$enable_lua" != "no" ; then
    AC_DEFINE([ENABLE_LUA], [1], [Lua rules enabled.])
fi

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
DOXYGEN_INPUT="include util engine modules servers tests"
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
elif test "$withval" = "no"; then
    AC_MSG_NOTICE([Not using valgrind])
    valgrind=no
elif test "$valgrind" != "no"; then
    AC_MSG_NOTICE([Using valgrind: $valgrind])
else
    valgrind=
fi

VALGRIND=$valgrind
if test "$valgrind" != "no"; then
    PKG_CHECK_MODULES([VALGRIND], [valgrind], [AC_DEFINE([HAVE_VALGRIND],[1],[valgrind exists])],[AC_DEFINE([NO_VALGRIND], [1], [valgrind does not exist])])
fi

dnl Checks for various external dependencies
sinclude(acinclude/htp.m4)
sinclude(acinclude/pcre.m4)
sinclude(acinclude/apxs.m4)
sinclude(acinclude/apr.m4)
sinclude(acinclude/apu.m4)
sinclude(acinclude/httpd.m4)
sinclude(acinclude/trafficserver.m4)
sinclude(acinclude/geoip.m4)
sinclude(acinclude/ax_pthread.m4)
sinclude(acinclude/ossp-uuid.m4)
sinclude(acinclude/ax_boost_base.m4)
sinclude(acinclude/protobuf.m4)

# Required libs
CHECK_HTP()
CHECK_PCRE()
AX_BOOST_BASE(1.40,
              [have_boost_low=yes], 
              [have_boost_low=no])
AX_BOOST_BASE(1.46,
              [have_boost=yes], 
              [have_boost=no])
if test "$have_boost_low" = "no"; then
    AC_MSG_WARN("Boost required for testing.")
fi

# Clang does not support -pthread or variants.
if test "$cc_is_clang" = "yes"; then
    AX_PTHREAD([LIBS="$LIBS $PTHREAD_LIBS"
                AC_MSG_RESULT([yes])],
               [AC_MSG_ERROR([Pthreads is required])])
else
    AX_PTHREAD([LIBS="$LIBS $PTHREAD_LIBS"
                CFLAGS="${CFLAGS} ${PTHREAD_CFLAGS}"
                CXXFLAGS="${CXXFLAGS} ${PTHREAD_CFLAGS}"
                AC_MSG_RESULT([yes])],
               [AC_MSG_ERROR([Pthreads is required])])
fi
dnl TODO: These need to be optional (only if building apache httpd server module)
CHECK_APXS()
CHECK_APR()
CHECK_APU()
CHECK_HTTPD()
CHECK_TS()
CHECK_GEOIP()
CHECK_OSSP_UUID()
CHECK_PROTOBUF()
CHECK_NIDS()
CHECK_NET()
CHECK_MODP()

if test "${HAVE_NIDS}" != "no" ; then
    AC_DEFINE([HAVE_NIDS], [1], [Have libnids library.])
fi
AM_CONDITIONAL(HAVE_NIDS, test "${HAVE_NIDS}" != "no")

if test "${HAVE_MODP}" != "no" ; then
    AC_DEFINE([HAVE_MODP], [1], [Have modp library.])
fi
AM_CONDITIONAL(HAVE_MODP, test "${HAVE_MODP}" != "no")

if test "${enable_clipp}" = "yes" ; then
    if test "${have_boost}" != "yes"; then
        AC_MSG_ERROR([Boost is required for clipp])
    fi
    if test "${enable_cpp}" != "yes" ; then
        AC_MSG_ERROR([C++ is required for clipp])
    fi
    if test "${HAVE_PROTOBUF}" != "yes" ; then
        AC_MSG_ERROR([protobuf is required for clipp])
    fi
    if test "${HAVE_NIDS}" != "yes" ; then
        echo "*******************************************************"
        echo "* Warning: libnids missing; no pcap support in clipp. *"
        echo "*******************************************************"
    elif test "${HAVE_NET}" != "yes" ; then
        AC_MSG_ERROR([libnet is required for clipp])
    fi
    if test "${HAVE_MODP}" != "yes" ; then
        echo "********************************************************"
        echo "* Warning: modp missing; no decoding support in clipp. *"
        echo "********************************************************"
    fi
fi

# Workaround for issue with gcc 4.6 and APR on Darwin
if test "$OS_DARWIN" = true; then
    APR_CPPFLAGS=`echo "$APR_CPPFLAGS" | sed -e 's/-no-cpp-precomp//'`
fi

#If we are not building servers bail
if test "$HAVE_HTTPD" = "no" && test "$HAVE_TS" = "no"; then
    echo "**********************************************"
    echo "* Warning! No servers enabled!               *"
    echo "* We will only be building the cli test tool.*"
    echo "**********************************************"
fi
  
dnl Optional build modules
dnl
dnl TODO: Make these configure opts
dnl
TOPLEVEL_SUBDIRS="libs util engine servers lua modules etc docs tests"
LIBS_SUBDIRS=""
SERVERS_SUBDIRS="apache_httpd2 trafficserver"

dnl Enable cli subdir if enabled
if test "$util_cli" != 0; then
    TOPLEVEL_SUBDIRS="${TOPLEVEL_SUBDIRS} cli"
fi

dnl Enable C++ subdirs if enabled
if test "$cpp_code" != 0; then
    TOPLEVEL_SUBDIRS="${TOPLEVEL_SUBDIRS} ironbeepp"
fi

dnl Enable clipp subdir if enabled
if test "${enable_clipp}" = "yes"; then
    TOPLEVEL_SUBDIRS="${TOPLEVEL_SUBDIRS} clipp"
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

dnl Check for out of tree builds
AC_MSG_CHECKING([for out of tree build])
if test "x$srcdir" != "x." ; then
   AC_MSG_RESULT([yes]);
   out_of_tree=yes
else
   AC_MSG_RESULT([no]);
   out_of_tree=no
fi
AM_CONDITIONAL([OUT_OF_TREE],[test "$out_of_tree" = "yes"])

dnl Substitutions
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(CONFIG_MACRO_DIR)
AC_SUBST(TOPLEVEL_SUBDIRS)
AC_SUBST(LIBS_SUBDIRS)
AC_SUBST(SERVERS_SUBDIRS)
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

AC_CONFIG_FILES([servers/Makefile])
AC_CONFIG_FILES([servers/apache_httpd2/Makefile])
AC_CONFIG_FILES([servers/trafficserver/Makefile])

if test "$util_cli" != 0; then
AC_CONFIG_FILES([cli/Makefile])
fi

if test "$cpp_code" != 0; then
AC_CONFIG_FILES([clipp/Makefile])
AC_CONFIG_FILES([ironbeepp/Makefile])
AC_CONFIG_FILES([ironbeepp/tests/Makefile])
fi

AC_CONFIG_FILES([lua/Makefile])
AC_CONFIG_FILES([modules/Makefile])

AC_CONFIG_FILES([etc/Makefile])

# TODO: Conditional
AC_CONFIG_FILES([docs/Makefile])
AC_CONFIG_FILES([docs/docbook/Makefile])
AC_CONFIG_FILES([docs/docbook/manual/Makefile])
AC_CONFIG_FILES([docs/docbook/manual/fop.xconf])

# TODO: Conditional on unit testing
AC_CONFIG_FILES([tests/Makefile])

# pkg-config
AC_CONFIG_FILES([ironbee.pc])

# doxygen
AC_CONFIG_FILES([docs/internal.doxy])
AC_CONFIG_FILES([docs/external.doxy])
AC_CONFIG_FILES([docs/fulldocs.doxy])

AC_OUTPUT
