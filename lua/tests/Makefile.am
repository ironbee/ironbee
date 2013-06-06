include $(top_srcdir)/build/tests.mk

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

CPPFLAGS += -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
            -I$(top_srcdir)/tests \
            -DTOP_SRCDIR=$(top_srcdir) \
            -DFFI_FILE=$(top_srcdir)/lua/ironbee-ffi.lua
LDFLAGS  += -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src
LDADD     = -lluajit-ironbee \
            $(top_builddir)/tests/ibtest_util.o \
            $(top_builddir)/util/libibutil.la

TESTS             = $(check_PROGRAMS)

check_PROGRAMS    = test_lua
test_lua_SOURCES  = test_lua.cpp

if ENABLE_JSON
check_PROGRAMS += test_lua_json
test_lua_json_SOURCES  = test_lua_json.cpp
endif
