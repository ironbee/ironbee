include $(top_srcdir)/build/common.mk

bin_PROGRAMS = ibcli

if DARWIN
AM_LDFLAGS = -image_base 100000000
endif

ibcli_SOURCES = ibcli.c
if DARWIN
  ibcli_LDADD_extra =
else
  ibcli_LDADD_extra = -lrt
endif

ibcli_LDADD = $(top_builddir)/engine/libironbee.la \
              $(top_builddir)/util/libibutil.la \
              -lhtp @LIBICONV@ $(ibcli_LDADD_extra) $(PCRE_LDADD)
ibcli_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS) $(PCRE_LDFLAGS)
ibcli_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS) $(PCRE_CFLAGS)
