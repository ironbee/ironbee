ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = lock.c util.c logformat.c \
                       debug.c mpool.c dso.c uuid.c \
                       array.c list.c stream.c hash.c bytestr.c field.c \
                       cfgmap.c radix.c ahocorasick.c string.c expand.c \
                       ironbee_util_private.h types.c
libibutil_la_CFLAGS = @HTP_CFLAGS@ @OSSP_UUID_CFLAGS@
libibutil_la_CPPFLAGS = @HTP_CPPFLAGS@ 
if FREEBSD
libibutil_la_LDFLAGS = @HTP_LDFLAGS@ @OSSP_UUID_LDFLAGS@ -lssp_nonshared @OSSP_UUID_LIBS@ 
else
libibutil_la_LDFLAGS = @HTP_LDFLAGS@ @OSSP_UUID_LDFLAGS@ 
libibutil_la_LIBADD = -ldl @OSSP_UUID_LIBS@
endif
