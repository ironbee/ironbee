ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

if DARWIN
if ENABLE_LUA
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif
endif

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -no-undefined -static-libtool-libs
else
AM_LDFLAGS += -no-undefined -static-libtool-libs
endif

check_PROGRAMS    = test_lua
TESTS             = $(check_PROGRAMS)
test_lua_SOURCES  = test_lua.cpp \
		    $(top_srcdir)/tests/gtest/gtest_main.cc \
		    $(top_srcdir)/tests/gtest/gtest-all.cc
test_lua_CPPFLAGS = -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
		    -I$(top_srcdir)/tests \
		    -DTOP_SRCDIR=$(top_srcdir) \
		    -DFFI_FILE=$(top_srcdir)/lua/ironbee-ffi.lua
test_lua_LDFLAGS  = $(LDFLAGS) \
		    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src
test_lua_LDADD    = -lluajit-ironbee

