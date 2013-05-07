ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = tests

pkglibdir = $(libdir)

AM_LIBADD = $(top_builddir)/engine/libironbee.la

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS += -module -avoid-version
endif

pkglib_LTLIBRARIES = ibmod_set_c.la
if CPP
pkglib_LTLIBRARIES += ibmod_set_cpp.la
endif

ibmod_set_c_la_SOURCES = ibmod_set_c.c
ibmod_set_cpp_la_SOURCES = ibmod_set_cpp.cpp
ibmod_set_cpp_la_LIBADD = $(AM_LIBADD) \
	$(top_builddir)/ironbeepp/libibpp.la
