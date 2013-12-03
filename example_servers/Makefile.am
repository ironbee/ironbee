ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

LDADD = $(LIBADD) $(top_builddir)/engine/libironbee.la

if FREEBSD
AM_LDFLAGS += -L/usr/local/lib -module -avoid-version -lssp_nonshared
else
AM_LDFLAGS += -module -avoid-version
endif

noinst_PROGRAMS = parsed_c

parsed_c_SOURCES = parsed_c.c
