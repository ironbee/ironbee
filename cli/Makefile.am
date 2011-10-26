include $(top_srcdir)/build/common.mk

noinst_PROGRAMS = ibcli

ibcli_SOURCES = ibcli.c
ibcli_LDADD = $(top_builddir)/engine/libironbee.la -lhtp
ibcli_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS)
ibcli_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS)
