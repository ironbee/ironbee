ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

LDADD = $(LIBADD) \
  $(top_builddir)/engine/libironbee.la \
  $(top_builddir)/util/libibutil.la

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS += -module -avoid-version
endif

noinst_PROGRAMS = parsed_c
if CPP
noinst_PROGRAMS += unparsed_cpp
endif

parsed_c_SOURCES = parsed_c.c

unparsed_cpp_SOURCES = unparsed_cpp.cpp
unparsed_cpp_LDADD = $(LDADD) \
	$(top_builddir)/modules/libparser_suite.la \
	$(top_builddir)/ironbeepp/libibpp.la
