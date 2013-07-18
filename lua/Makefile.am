ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = tests

FFI_FILES = \
	$(top_srcdir)/include/ironbee/engine.h \
	$(top_srcdir)/include/ironbee/engine_state.h \
	$(top_srcdir)/include/ironbee/core.h \
	$(top_srcdir)/include/ironbee/context.h \
	$(top_srcdir)/include/ironbee/rule_defs.h \
	$(top_srcdir)/include/ironbee/rule_engine.h \
	$(top_srcdir)/include/ironbee/logevent.h

BUILT_SOURCES = ironbee-ffi.h \
                ironbee-ffi-h.lua

LUA_CFLAGS = -fPIC
LUA_LIBADD = -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
             -lluajit-ironbee \
             -lm
LUA_CPPFLAGS = -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
	       $(AM_CPPFLAGS)

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
             $(shell find $(srcdir)/ironbee/waggle -name \*lua)

EXTRA_DIST = ironbee-ffi-h.lua \
             ironbee-ffi.lua \
             ironbee/api.lua \
             ironbee/config.lua \
             ironbee/engine.lua \
             ironbee/logevent.lua \
             ironbee/module.lua \
             ironbee/rules.lua \
             ironbee/tx.lua \
             ironbee/util.lua \
             example.lua \
             threat_level.lua \
             event_processor.lua \
             $(WAGGLE_FILES) \
             qa.lua


nobase_lua_DATA = \
	ironbee-ffi-h.lua \
    ironbee-ffi.lua \
    ironbee/api.lua \
    ironbee/module.lua \
    example.lua \
    threat_level.lua \
    event_processor.lua \
    qa.lua

lua_LTLIBRARIES =
noinst_LTLIBRARIES = libironbee-lua.la
libironbee_lua_la_SOURCES = ironbee.c ironbee.h
libironbee_lua_la_CPPFLAGS = $(LUA_CPPFLAGS)
libironbee_lua_la_CFLAGS = $(LUA_CFLAGS)
libironbee_lua_la_LIBADD = $(LUA_LIBADD)

if ENABLE_JSON
lua_LTLIBRARIES                 += ironbee/util/ibjson.la
ironbee_util_ibjson_la_SOURCES   = ironbee/util/ibjson.c
ironbee_util_ibjson_la_CPPFLAGS  = $(LUA_CPPFLAGS)
ironbee_util_ibjson_la_CFLAGS    = $(LUA_CFLAGS)
ironbee_util_ibjson_la_LIBADD    = $(LUA_LIBADD)
ironbee_util_ibjson_la_LDFLAGS   = $(LDFLAGS) \
                                   -L$(top_builddir)/util \
                                   -libutil \
                                   -module -avoid-version -shared
endif

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
	cat $< | grep -v -e '^# ' | grep -v __typeof__ | sed -e 's/__attribute__ *((nonnull ([0-9, ]*)))//g' | sed -e 's/__attribute__ *((nonnull))//g' >> $@
	echo "]]"                                      >> $@

CLEANFILES = ironbee-ffi.h ironbee-ffi-h.lua

install-data-hook:
	cd $(top_srcdir)/lua && tar cf - ironbee | tar xf - -C $(DESTDIR)$(libdir)

uninstall-hook:
	rm -fr $(DESTDIR)$(libdir)/ironbee

