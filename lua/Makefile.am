ACLOCAL_AMFLAGS = -I ../acinclude

AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/build/common.mk

SUBDIRS = tests

FFI_FILES = \
    $(top_srcdir)/include/ironbee/engine.h \
    $(top_srcdir)/include/ironbee/engine_types.h \
    $(top_srcdir)/include/ironbee/engine_state.h \
    $(top_srcdir)/include/ironbee/core.h \
    $(top_srcdir)/include/ironbee/config.h \
    $(top_srcdir)/include/ironbee/operator.h \
    $(top_srcdir)/include/ironbee/transformation.h \
    $(top_srcdir)/include/ironbee/action.h \
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

LUA_FILES = \
    ironbee-ffi-h.lua \
    predicate-primitives.lua \
    ironbee/api.lua \
    ironbee/config.lua \
    ironbee/engine.lua \
    ironbee/logevent.lua \
    ironbee/module.lua \
    ironbee/predicate.lua \
    ironbee/rules.lua \
    ironbee/tx.lua \
    ironbee/util.lua \
    $(WAGGLE_FILES)

EXTRA_DIST = $(LUA_FILES)

nobase_lua_DATA = $(LUA_FILES)

ironbee_luadir=${luadir}/ironbee/util
ironbee_lua_LTLIBRARIES =
noinst_LTLIBRARIES = libironbee-lua.la
libironbee_lua_la_SOURCES = ironbee.c ironbee.h
libironbee_lua_la_CPPFLAGS = $(LUA_CPPFLAGS)
libironbee_lua_la_CFLAGS = $(LUA_CFLAGS)
libironbee_lua_la_LIBADD = $(LUA_LIBADD)

if ENABLE_JSON
ironbee_lua_LTLIBRARIES         += ironbee/util/ibjson.la
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
	echo "#undef __BLOCKS__" > tmp.h
	for i in $+; do \
		echo "#include \"$$i\"" >> tmp.h; \
	done
	$(CPP) -DLUA_FFI -I$(top_srcdir)/include -I$(top_builddir)/include $(CPPFLAGS) tmp.h > "$@"
	rm tmp.h

ironbee-ffi-h.lua: ironbee-ffi.h $(srcdir)/Makefile.am
	echo "#!/usr/bin/lua"             > $@
	echo "local ffi = require('ffi')" >> $@
	echo "ffi.cdef [["                >> $@
	cat $<                            |\
        grep -v -e '^# '              |\
        grep -v __typeof__            >> $@
	echo "]]"                         >> $@

CLEANFILES = ironbee-ffi.h ironbee-ffi-h.lua

