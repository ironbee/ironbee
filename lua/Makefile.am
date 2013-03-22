ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

luadir = $(libdir)

FFI_FILES = $(top_srcdir)/include/ironbee/engine.h \
	    $(top_srcdir)/include/ironbee/core.h \
	    $(top_srcdir)/include/ironbee/rule_defs.h \
	    $(top_srcdir)/include/ironbee/rule_engine.h \
	    $(top_srcdir)/include/ironbee/logevent.h

BUILT_SOURCES = ironbee-ffi.h \
                ironbee-ffi-h.lua

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

WAGGLE_FILES=ironbee/waggle.lua \
             $(shell find ironbee/waggle -name \*lua)

EXTRA_DIST = ironbee-ffi-h.lua \
             ironbee-ffi.lua \
             ironbee-api.lua \
             ironbee-modlua.lua \
             example.lua \
             threat_level.lua \
             event_processor.lua \
             $(WAGGLE_FILES) \
             qa.lua

lua_DATA   = ironbee-ffi-h.lua \
             ironbee-ffi.lua \
             ironbee-api.lua \
             ironbee-modlua.lua \
             example.lua \
             threat_level.lua \
             event_processor.lua \
             qa.lua

noinst_LTLIBRARIES = libironbee-lua.la
libironbee_lua_la_SOURCES = ironbee.c ironbee.h
libironbee_lua_la_CPPFLAGS = $(AM_CPPFLAGS) \
                             -I$(top_srcdir)/libs/luajit-2.0-ironbee/src
libironbee_lua_la_CFLAGS = -fPIC
libironbee_lua_la_LIBADD = -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                           -lluajit-ironbee \
                           -lm

ironbee-ffi.h: $(FFI_FILES)
	> tmp.h
	for i in $+; do \
		echo "#include \"$$i\"" >> tmp.h; \
	done
	$(CPP) -I$(top_srcdir)/include -I$(top_builddir)/include $(CPPFLAGS) tmp.h > "$@"
	rm tmp.h

ironbee-ffi-h.lua: ironbee-ffi.h
	echo "#!/usr/bin/lua"                           > $@
	echo "local ffi = require('ffi')"              >> $@
	echo "ffi.cdef [["                             >> $@
	cat $< | grep -v -e '^# ' | grep -v __typeof__ >> $@
	echo "]]"                                      >> $@

check_PROGRAMS    = test_lua
TESTS             = $(check_PROGRAMS)
test_lua_SOURCES  = test_lua.cpp \
		    $(top_srcdir)/tests/gtest/gtest_main.cc \
		    $(top_srcdir)/tests/gtest/gtest-all.cc
test_lua_CPPFLAGS = -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
		    -I$(top_srcdir)/tests \
		    -DFFI_FILE=$(top_srcdir)/lua/ironbee-ffi.lua
test_lua_LDFLAGS  = $(LDFLAGS) \
		    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src
test_lua_LDADD    = -lluajit-ironbee

CLEANFILES = ironbee-ffi.h ironbee-ffi-h.lua

install-data-hook:
	cd $(top_srcdir)/lua && tar cf - ironbee | tar xf - -C $(DESTDIR)$(libdir)

uninstall-hook:
	rm -fr $(DESTDIR)$(libdir)/ironbee

