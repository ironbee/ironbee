ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = lock.c util.c logformat.c \
                       debug.c mpool.c dso.c uuid.c \
                       array.c list.c stream.c hash.c bytestr.c field.c \
                       cfgmap.c ahocorasick.c expand.c \
                       string.c strlower.c strtrim.c strwspc.c \
                       ip_addr.c \
                       path.c \
                       decode.c \
                       escape.c \
                       modsec_compat.c \
                       clock.c types.c ipset.c ip.c

EXTRA_DIST = ahocorasick_private.h

libibutil_la_CFLAGS = @OSSP_UUID_CFLAGS@
if FREEBSD
libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ -lssp_nonshared @OSSP_UUID_LIBS@ 
else
libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ 
libibutil_la_LIBADD = -ldl @OSSP_UUID_LIBS@
endif
