ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

luadir = $(prefix)/lib

if FREEBSD
AM_LDFLAGS = -L/usr/local/lib -no-undefined -static-libtool-libs
else
AM_LDFLAGS = -no-undefined -static-libtool-libs
endif

lua_DATA = example.lua \
           example-ffi.lua

noinst_LTLIBRARIES = libironbee-lua.la
libironbee_lua_la_SOURCES = ironbee.c
libironbee_lua_la_CPPFLAGS = $(CPPFLAGS) \
                             -I$(top_srcdir)/libs/luajit-2.0-ironbee/src
libironbee_lua_la_CFLAGS = -fPIC
libironbee_lua_la_LIBADD = -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                           -lluajit-ironbee \
                           -lm
