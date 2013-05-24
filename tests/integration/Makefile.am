ACLOCAL_AMFLAGS = -I../../acinclude

include $(top_srcdir)/build/tests.mk

noinst_LIBRARIES = libgtest.a libgtest_main.a

LIBUTIL_LDADD = $(top_builddir)/util/libibutil.la
LIBENGINE_LDADD = $(LIBUTIL_LDADD) $(top_builddir)/engine/libironbee.la

libgtest_a_SOURCES = ../gtest/gtest-all.cc
libgtest_main_a_SOURCES = ../gtest/gtest_main.cc

LDADD = $(LIBENGINE_LDADD) libgtest.a

#This is simply broken in FreeBSD 8, but still builds okay on FreeBSD 7.
if FREEBSD
LDFLAGS+=-fstack-protector
endif

if DARWIN
if ENABLE_LUA
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif
endif

if LINUX
LDFLAGS += -lrt
if CC_IS_CLANG
LDFLAGS += -lpthread
endif
endif

LDFLAGS += $(BOOST_LDFLAGS)

# Point the module base path to the modules build dir
CPPFLAGS+=-DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
          -DRULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
          -I$(top_srcdir)/libs/libhtp/htp \
          $(BOOST_CPPFLAGS) -I$(top_builddir)/include

check_PROGRAMS =
if BUILD_RIAK
check_PROGRAMS += test_kvstore_riak
endif

TESTS = $(check_PROGRAMS)

EXTRA_DIST = ../gtest

check-programs: $(check_PROGRAMS)
check-libs:  $(check_LTLIBRARIES)
build: check-programs check-libs

$(abs_builddir)/%: $(srcdir)/%
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
	  cp -f $< $@; \
	fi

if BUILD_RIAK
test_kvstore_riak_SOURCES = ../test_main.cpp test_kvstore_riak.cpp
test_kvstore_riak_LDADD = $(LDADD)
endif
