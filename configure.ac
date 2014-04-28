dnl
dnl Qualys WAF
dnl

AC_PREREQ(2.59)

AC_INIT([ironbee],
	m4_esyscmd([./get-version.sh VERSION]),
	[ironbee-team@ironbee.com])

AC_CONFIG_MACRO_DIR([acinclude]) AC_CONFIG_SRCDIR([README.md])
AC_CONFIG_HEADERS([ironbee_config_auto_gen.h])
AC_CONFIG_AUX_DIR([build])
AC_PREFIX_DEFAULT([/usr/local/ironbee])
AX_SPLIT_VERSION([$PACKAGE_VERSION])

AM_INIT_AUTOMAKE([foreign tar-pax])

### Determine automake version.
AC_MSG_CHECKING([for automake > 1.12])
AX_COMPARE_VERSION([$am__api_version],[gt],[1.12],
[
	automake113="yes"
	AC_MSG_RESULT([yes])
],
[
	automake113="no"
	AC_MSG_RESULT([no])
])
AM_CONDITIONAL([AUTOMAKE113], [test "$automake113" = "yes"])

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
                    AM_LDFLAGS="${AM_LDFLAGS} -Wl,-R/usr/local/lib/gcc45"
                elif test "$GCC46BIN" != "no"; then
                    CC="gcc46"
                    CPP="cpp46"
                    CXX="g++46"
                    AM_LDFLAGS="${AM_LDFLAGS} -Wl,-R/usr/local/lib/gcc46"
                elif test "$GCC47BIN" != "no"; then
                    CC="gcc47"
                    CPP="cpp47"
                    CXX="g++47"
                    AM_LDFLAGS="${AM_LDFLAGS} -Wl,-R/usr/local/lib/gcc47"
                else
                    echo
                    echo "   You must have gcc45/gcc46/gcc47 to compile on FreeBSD 7/8/9"
                    echo
                    exit 1
                fi
        ;;
esac

AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_PROG_CXX
AC_PROG_CXXCPP

if test "$GCC" = "yes"; then
	AC_MSG_CHECKING([if compiler is c clang])
    AC_TRY_COMPILE(,
                   [#ifndef __clang__
                       something invalid
                    #endif
                  return 0;],
                [ AC_MSG_RESULT([yes]); cc_is_clang=yes ], [AC_MSG_RESULT([no]); cc_is_clang=no]
                )
    if test "$cc_is_clang" == "yes"; then
        CFLAGS="${CFLAGS} -fPIC"
        CXXFLAGS="${CXXFLAGS} -fPIC"
    else
        CFLAGS="${CFLAGS} -std=c99"
    fi
fi
AM_CONDITIONAL([CC_IS_CLANG], [test "$cc_is_clang" = "yes"])

dnl if using gcc, determine version.
if test "$GCC" = "yes" && test "$cc_is_clang" != "yes"; then
	AX_GCC_VERSION
else
	AX_CLANG_VERSION
fi

dnl turn on lots of warnings with modern compilers
MODERN_COMPILER="no"
if test "$cc_is_clang" == "yes"; then
    MODERN_COMPILER="yes"
else
	AX_COMPARE_VERSION([$GCC_VERSION],[gt],[4.6],
		[MODERN_COMPILER="yes"])
fi
AM_CONDITIONAL([MODERN_COMPILER], [test "$MODERN_COMPILER" = "yes"])

dnl Check C++11 support
CPP11="no"
if test "$cc_is_clang" == "yes"; then
	AX_COMPARE_VERSION([$CLANG_VERSION],[ge],[3.2],
		[CPP11="yes"])
fi
AM_CONDITIONAL([CPP11], [test "$CPP11" = "yes"])

dnl After all other tests, we will turn on errors.

AC_PROG_INSTALL
AC_PROG_LN_S

dnl Checks for libraries.

AC_CHECK_HEADERS(arpa/inet.h netinet/in.h)

AC_MSG_CHECKING([OS])
case "$OS" in
        FreeBSD*)
                AC_MSG_RESULT(FreeBSD)
                OS_FREEBSD="true"
                AM_CPPFLAGS="${AM_CPPFLAGS} -I/usr/local/include"
                AM_LDFLAGS="${AM_LDFLAGS} -L/usr/local/lib"
        ;;
        Darwin*)
                AC_MSG_RESULT(Darwin)
                OS_DARWIN="true"
                AC_DEFINE(_POSIX_C_SOURCE, [1], [Enable POSIX C source feature test macro])
                AC_DEFINE(_DARWIN_C_SOURCE, [1], [Enable full C features on OSX])
                AM_CPPFLAGS="${AM_CPPFLAGS} -I/usr/local/include -I/opt/local/include"
                AM_LDFLAGS="${AM_LDFLAGS} -L/usr/local/lib -L/opt/local/lib"
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

### ThreadSanitizer
AC_ARG_ENABLE(thread-sanitizer-workaround,
              AS_HELP_STRING([--enable-thread-sanitizer-workaround],
                             [Disable or otherwise workaround thread sanitizer issues for certain tests.]),
[
  if test "$enableval" != "no"; then
    thread_sanitizer_workaround="yes"
    AC_DEFINE([IB_THREAD_SANITIZER_WORKAROUND], [1], [Workaround thread sanitizer issues.])
  else
    thread_sanitizer_workaround="no"
  fi
],
[
  thread_sanitizer_workaround="no"
])
AM_CONDITIONAL([THREAD_SANITIZER_WORKAROUND], [test "$thread_sanitizer_workaround" == "no"])


### Experimental features
AC_ARG_ENABLE(experimental,
              AS_HELP_STRING([--enable-experimental],
                             [Enable experimental features.]),
[
  experimental=$enableval
],
[
  experimental="yes"
])
AM_CONDITIONAL([BUILD_EXPERIMENTAL_MODULES], [test "$experimental" != "no"])

### Disable Ruby.
AC_ARG_ENABLE(ruby-code,
              AS_HELP_STRING([--disable-ruby-code],
                             [Disable everything that requires ruby.]),
[
  ruby_code=$enableval
],
[
  ruby_code="default"
])

### Mpool Valgrind Support
AC_ARG_ENABLE(mpool-valgrind,
              AS_HELP_STRING([--enable-mpool-valgrind],
                             [Add valgrind hints to mpool code.]),
[
  mpool_valgrind=$enableval
],
[
  mpool_valgrind="no"
])

if test "$mpool_valgrind" != "no"; then
    AC_DEFINE([IB_MPOOL_VALGRIND], [1], [Valgrind support in mpool.])
fi

### Rule Tracing
AC_ARG_ENABLE(rule-trace,
              AS_HELP_STRING([--rule-trace],
                             [Instrument rule engine.]),
[
  rule_trace=$enableval
],
[
  rule_trace="no"
])

if test "$rule_trace" != "no"; then
    AC_DEFINE([IB_RULE_TRACE], [1], [Trace rules.])
fi

### CPP
AC_ARG_ENABLE(cpp,
              AS_HELP_STRING([--disable-cpp],
                             [Disable C++ code.]),[enable_cpp="$enableval"],[enable_cpp=yes])
AS_IF([test "$enable_cpp" = "yes"], [cpp_code=1], [cpp_code=0])
AM_CONDITIONAL([CPP], [test "$enable_cpp" = "yes"])
ironbeepp_includedir='${prefix}/include/ironbeepp'
AC_SUBST(ironbeepp_includedir)
ironautomata_includedir='${prefix}/include/ironautomata'
AC_SUBST(ironautomata_includedir)
ironautomata_generator_includedir='${prefix}/include/ironautomata/generator'
AC_SUBST(ironautomata_generator_includedir)

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


### Integration Tests
integration_tests="no"
AC_ARG_WITH([integration-tests],
            [  --with-integration-tests],
            [integration_tests="yes"],[integration_tests="no"])
AM_CONDITIONAL([INTEGRATION_TESTS], test "${integration_test}" = "yes"])

### Ragel
CHECK_RAGEL(6.8)

### Doxygen
AC_ARG_WITH([doxygen],
            [  --with-doxygen=PROG doxygen executable],
            [doxygen="$withval"],[doxygen=yes])

if test "$doxygen" = "yes"; then
    AC_PATH_PROGS([doxygen],[doxygen],[])
elif test "$doxygen" != "no"; then
    AC_MSG_NOTICE([Using doxygen: $doxygen])
else
    doxygen=
    AC_MSG_NOTICE([Not using doxygen: not found])
fi

DOXYGEN=$doxygen

DOXYGEN_TOP=`cd $srcdir;pwd`
DOXYGEN_DIRS="include util engine modules automata fast lua predicate example_modules example_servers"
MARKDOWN_DOXYGEN_FILES=`cd $srcdir;find . lua -type f -maxdepth 1 -name '*.md'`
EXTERNAL_DOXYGEN_FILES=`cd $srcdir;find $DOXYGEN_DIRS -type f -name '*.h' -o -name '*.hpp'`
INTERNAL_DOXYGEN_FILES=`cd $srcdir;find $DOXYGEN_DIRS -type f -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cpp' -o -name '*.cc'`
DOXYGEN_INPUT=""
for dir in $DOXYGEN_DIRS; do
    DOXYGEN_INPUT="$DOXYGEN_INPUT $DOXYGEN_TOP/$dir"
done
for dir in $MARKDOWN_DOXYGEN_FILES; do
    DOXYGEN_INPUT="$DOXYGEN_INPUT $DOXYGEN_TOP/$dir"
done
DOXYGEN_EXTERNAL_DEPS=""
for f in $EXTERNAL_DOXYGEN_FILES; do
    DOXYGEN_EXTERNAL_DEPS="$DOXYGEN_EXTERNAL_DEPS \$(abs_top_srcdir)/$f"
done
DOXYGEN_INTERNAL_DEPS=""
for f in $INTERNAL_DOXYGEN_FILES; do
    DOXYGEN_INTERNAL_DEPS="$DOXYGEN_INTERNAL_DEPS \$(abs_top_srcdir)/$f"
done

### Asciidoc executable and options
AC_ARG_WITH([asciidoc],
            [  --with-asciidoc=PROG asciidoc engine executable],
            [asciidoc="$withval"],[asciidoc=yes])

if test "$asciidoc" = "yes"; then
    AC_PATH_PROGS([asciidoc],[asciidoctor asciidoc],[])
elif test "$asciidoc" != "no"; then
    AC_MSG_NOTICE([Using asciidoc: $asciidoc])
else
    asciidoc=
    AC_MSG_NOTICE([Not using asciidoc: no engine found])
fi

ASCIIDOC=$asciidoc

AC_ARG_WITH([asciidoc_options],
            [  --with-asciidoc-options=OPTIONS asciidoc engine options],
            [asciidoc_options="$withval"],[asciidoc_options=yes])

if test "$asciidoc_options" = "yes"; then
    asciidoc_options="-b html5"
    AC_MSG_NOTICE([Using default asciidoc options: $asciidoc_options])
elif test "$asciidoc_options" != "no"; then
    AC_MSG_NOTICE([Using asciidoc options: $asciidoc_options])
else
    asciidoc_options=
fi

ASCIIDOC_OPTIONS=$asciidoc_options

AM_CONDITIONAL([ENABLE_REFMANUAL], test -n "$ASCIIDOC"])

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
sinclude(acinclude/dso_tool.m4)
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

AC_CHECK_LIB([m], [ceill], [have_libm=yes], [have_libm=no])
if test "$have_libm" = "no"; then
    AC_CHECK_FUNC([ceill], [], [
	  # No ceill anywhere.
	  AC_MSG_ERROR([Unable to find ceill().])
    ])
	LIBM_LDFLAGS=""
else
    LIBM_LDFLAGS="-lm"
fi
AC_SUBST(LIBM_LDFLAGS)


### dladdr() support
AC_CHECK_FUNC(
  [dladdr],
  [AC_DEFINE([HAVE_DLADDR], [1], [dladdr exists])],
  [])

CHECK_PCRE()
AX_BOOST_BASE(1.40,
              [have_boost_low=yes],
              [have_boost_low=no])
# Save flags in case we don't find higher version.
saved_BOOST_LDFLAGS=$BOOST_LDFLAGS
saved_BOOST_CPPFLAGS=$BOOST_CPPFLAGS
AX_BOOST_BASE(1.49,
              [have_boost=yes],
              [have_boost=no])
if test "$have_boost_low" = "no"; then
    AC_MSG_WARN("Boost required for testing.")
fi
if test "$have_boost" = "no"; then
  BOOST_LDFLAGS=$saved_BOOST_LDFLAGS
  BOOST_CPPFLAGS=$saved_BOOST_CPPFLAGS
fi
AX_BOOST_SUFFIX()
AX_BOOST_THREAD_SUFFIX()

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

if test "${APR_REQUIRES_APU}" = "yes" ; then
    CHECK_APU()
fi
CHECK_HTTPD()
CHECK_NGX_DSO_TOOL()
CHECK_TS()
CHECK_GEOIP()
CHECK_OSSP_UUID()
CHECK_PROTOBUF()
CHECK_NIDS()
CHECK_NET()
CHECK_MODP()
CHECK_LIBCURL()
CHECK_YAJL()

AM_CONDITIONAL(HAVE_LIBCURL, [test "${HAVE_LIBCURL}" != no])
if test "${HAVE_LIBCURL}" != "no" && test "${HAVE_YAJL}" != "no" ; then
    dnl # We use "BUILD" and not "HAVE" because we are not using any riak libs.
    dnl # If we import a riak client library, it should define "HAVE."
    AC_DEFINE([BUILD_RIAK], [1], [Build riak libraries.])
    BUILD_RIAK="yes"
else
    BUILD_RIAK="no"
fi
AM_CONDITIONAL([BUILD_RIAK], [test "${BUILD_RIAK}" != "no"])

if test "${HAVE_NIDS}" != "no" ; then
    AC_DEFINE([HAVE_NIDS], [1], [Have libnids library.])
fi
AM_CONDITIONAL(HAVE_NIDS, test "${HAVE_NIDS}" != "no")

if test "${HAVE_MODP}" != "no" ; then
    AC_DEFINE([HAVE_MODP], [1], [Have modp library.])
fi
AM_CONDITIONAL(HAVE_MODP, test "${HAVE_MODP}" != "no")

if test "${cpp_code}" = 1 ; then
    if test "${have_boost}" != "yes"; then
        AC_MSG_ERROR([Boost is required for C++ code])
    fi
    if test "${HAVE_PROTOBUF}" != "yes" ; then
        AC_MSG_ERROR([protobuf is required for C++ code])
    fi
	have_ruby=yes
	if test "$ruby_code" != "no"; then
	    CHECK_RUBY(1.9,[],[have_ruby=no])
	    CHECK_GEM(1.3,[],[have_ruby=no])
	    CHECK_RUBY_GEM(ruby_protobuf, 0.4.11, ruby_protobuf,[],[have_ruby=no])
	fi

	if test "$have_ruby" != "yes"; then
		if test "$ruby_code" = "yes"; then
			AC_MSG_ERROR([ruby requirements not met but --enable-ruby-code present.])
		else
	        echo "************************************************************"
	        echo "* Warning: ruby requirements not met; disabling ruby code. *"
	        echo "************************************************************"
			ruby_code=no
		fi
	fi

    if test "${HAVE_NIDS}" != "yes" ; then
        echo "*******************************************************"
        echo "* Warning: libnids missing; no pcap support in clipp. *"
        echo "*******************************************************"
    else
        if test "${HAVE_NET}" != "yes" ; then
            AC_MSG_ERROR([libnet is required for clipp])
        fi
        sinclude(acinclude/glib-2.0.m4)
        AM_PATH_GLIB_2_0(, , , gthread)
    fi
    if test "${HAVE_MODP}" != "yes" ; then
        echo "********************************************************"
        echo "* Warning: modp missing; no decoding support in clipp. *"
        echo "********************************************************"
    fi
fi

### YAJL / JSON support
AM_CONDITIONAL([BUILD_YAJL], [test "${HAVE_YAJL}" != "no"])
AM_CONDITIONAL([ENABLE_JSON], [test "${HAVE_YAJL}" != "no"])
if test "${HAVE_YAJL}" = "no" ; then
  AC_DEFINE([ENABLE_JSON], [0], [JSON encode/decode disabled.])
else
  AC_DEFINE([ENABLE_JSON], [1], [JSON encode/decode enabled.])
fi

# Workaround for issue with gcc 4.6 and APR on Darwin
if test "$OS_DARWIN" = true; then
    APR_CPPFLAGS=`echo "$APR_CPPFLAGS" | sed -e 's/-no-cpp-precomp//'`
fi

dnl Optional build modules
dnl
dnl TODO: Make these configure opts
dnl
TOPLEVEL_SUBDIRS="servers automata fast tools etc docs packaging"
LIBS_SUBDIRS="libhtp sqltfn"
SERVERS_SUBDIRS="apache_httpd2 trafficserver nginx"

dnl Enable lua subdir if enabled
if test "$enable_lua" != "no"; then
    TOPLEVEL_SUBDIRS="lua ${TOPLEVEL_SUBDIRS}"
fi

dnl Enable C++ subdirs if enabled
if test "$cpp_code" != 0; then
    TOPLEVEL_SUBDIRS="ironbeepp ${TOPLEVEL_SUBDIRS} predicate clipp ibctl"
fi

dnl Modules should go last.
TOPLEVEL_SUBDIRS="${TOPLEVEL_SUBDIRS} modules example_servers example_modules"

dnl Some subdirectories should go very early.
TOPLEVEL_SUBDIRS="tests util engine ${TOPLEVEL_SUBDIRS}"

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

# Prepend libs
TOPLEVEL_SUBDIRS="libs ${TOPLEVEL_SUBDIRS}"

AC_RPM_CHECK
AC_SUBST(HAVE_RPM)

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

# Check for -Wunused-private-field
AC_MSG_CHECKING(for -Wno-unused-private-field)
save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} -Werror -Wno-unused-private-field"
AC_TRY_COMPILE(,,
  [have_no_unused_private_field=yes],[have_no_unused_private_field=no])
CPPFLAGS="${save_CPPFLAGS}"
if test "$have_no_unused_private_field" == "yes"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AM_CONDITIONAL([HAVE_NO_UNUSED_PRIVATE_FIELD], [test "$have_no_unused_private_field" = "yes"])

dnl Done testing, turn on errors.
dnl Make sure this is after any autoconf tests, which tend not to be
dnl warningless.
if test "$MODERN_COMPILER" == "yes"; then
	AC_MSG_NOTICE([Modern compiler detected; turning on warnings and errors.])
	AM_CPPFLAGS="${AM_CPPFLAGS} -Wall -Wextra -Werror"
else
	AC_MSG_NOTICE([Old or unknown compiler detected; limited support.])
fi

AM_CONDITIONAL([RUBY_CODE], [test "$ruby_code" != "no"])

dnl Install directories
pkglibdir='${libdir}'
moduledir='${libexecdir}'
plugindir='${libexecdir}'
luadir='${libdir}/ironbee/lua'
AC_SUBST(moduledir)
AC_SUBST(plugindir)
AC_SUBST(luadir)

dnl Substitutions
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(CONFIG_MACRO_DIR)
AC_SUBST(TOPLEVEL_SUBDIRS)
AC_SUBST(LIBS_SUBDIRS)
AC_SUBST(SERVERS_SUBDIRS)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(IB_DEBUG)
AC_SUBST(DOXYGEN)
AC_SUBST(DOXYGEN_EXTERNAL_DEPS)
AC_SUBST(DOXYGEN_INTERNAL_DEPS)
AC_SUBST(DOXYGEN_INPUT)
AC_SUBST(DOXYGEN_TOP)
AC_SUBST(ASCIIDOC)
AC_SUBST(ASCIIDOC_OPTIONS)
AC_SUBST(VALGRIND)
AC_SUBST(BOOST_SUFFIX)
AC_SUBST(MAJOR_VERSION, [$AX_MAJOR_VERSION])
AC_SUBST(MINOR_VERSION, [$AX_MINOR_VERSION])
AC_SUBST(MICRO_VERSION, [$AX_POINT_VERSION])
AC_SUBST(LIBRARY_VERSION, [$MAJOR_VERSION:$MINOR_VERSION:0])
AC_SUBST(RUBY)
AC_SUBST(GEM)

dnl Generate files
AC_CONFIG_FILES([Makefile])

AC_CONFIG_FILES([libs/Makefile])
AC_CONFIG_FILES([util/Makefile])
AC_CONFIG_FILES([util/tests/Makefile])
AC_CONFIG_FILES([engine/Makefile])
AC_CONFIG_FILES([engine/tests/Makefile])
AC_CONFIG_FILES([automata/Makefile])
AC_CONFIG_FILES([automata/bin/Makefile])
AC_CONFIG_FILES([automata/tests/Makefile])
AC_CONFIG_FILES([fast/Makefile])

AC_CONFIG_FILES([servers/Makefile])
AC_CONFIG_FILES([servers/apache_httpd2/Makefile])
AC_CONFIG_FILES([servers/trafficserver/Makefile])
AC_CONFIG_FILES([servers/nginx/Makefile])
AC_CONFIG_FILES([servers/nginx/config.tengine])

if test "$cpp_code" != 0; then
AC_CONFIG_FILES([clipp/Makefile])
AC_CONFIG_FILES([clipp/tests/Makefile])
AC_CONFIG_FILES([ironbeepp/Makefile])
AC_CONFIG_FILES([ironbeepp/tests/Makefile])
AC_CONFIG_FILES([predicate/Makefile])
AC_CONFIG_FILES([predicate/tests/Makefile])
AC_CONFIG_FILES([ibctl/Makefile])
AC_CONFIG_FILES([ibctl/tests/Makefile])
fi

AC_CONFIG_FILES([lua/Makefile])
AC_CONFIG_FILES([lua/tests/Makefile])
AC_CONFIG_FILES([tools/Makefile])
AC_CONFIG_FILES([modules/Makefile])
AC_CONFIG_FILES([modules/tests/Makefile])
AC_CONFIG_FILES([example_modules/Makefile])
AC_CONFIG_FILES([example_modules/tests/Makefile])
AC_CONFIG_FILES([example_servers/Makefile])

AC_CONFIG_FILES([etc/Makefile])

# TODO: Conditional
AC_CONFIG_FILES([docs/Makefile])
AC_CONFIG_FILES([docs/reference-manual/Makefile])

# TODO: Conditional on unit testing
AC_CONFIG_FILES([tests/Makefile])

if test "$integration_tests" != "no"; then
AC_CONFIG_FILES([util/tests/integration/Makefile])
fi

# pkg-config
AC_CONFIG_FILES([ironbee.pc])

# doxygen
AC_CONFIG_FILES([docs/internal.doxy])
AC_CONFIG_FILES([docs/external.doxy])

# version file
AC_CONFIG_FILES([include/ironbee/release.h])

# packaging
AC_CONFIG_FILES([packaging/Makefile])
AC_CONFIG_FILES([packaging/rpm/Makefile])
AC_CONFIG_FILES([packaging/rpm/ironbee.spec])

# HTP
AC_CONFIG_SUBDIRS([libs/libhtp])

# SQLTFN
AC_CONFIG_SUBDIRS([libs/sqltfn])

AC_OUTPUT

echo "IronBee configure complete."
if test "$ruby_code" != "no"; then
	echo "Unchecked optional dependencies:"
	echo "- Some ruby scripts also require the ruby-lua gem and graphviz."
fi
