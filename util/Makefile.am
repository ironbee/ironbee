ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = lock.c util.c logformat.c \
                       debug.c mpool.c dso.c uuid.c \
                       array.c list.c stream.c hash.c bytestr.c field.c \
                       cfgmap.c radix.c ahocorasick.c expand.c \
                       string.c strlower.c strtrim.c strwspc.c \
                       path.c \
                       decode.c \
                       modsec_compat.c \
                       clock.c types.c \
                       ironbee_util_private.h
libibutil_la_CFLAGS = @OSSP_UUID_CFLAGS@
if FREEBSD
libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ -lssp_nonshared @OSSP_UUID_LIBS@ 
else
libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ 
libibutil_la_LIBADD = -ldl @OSSP_UUID_LIBS@
endif
