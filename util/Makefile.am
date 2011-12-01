ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = util.c logformat.c \
                       debug.c mpool.c dso.c uuid.c \
                       array.c list.c hash.c bytestr.c field.c \
                       cfgmap.c radix.c ahocorasick.c \
                       ironbee_util_private.h
libibutil_la_CFLAGS = @HTP_CFLAGS@
libibutil_la_CPPFLAGS = @HTP_CPPFLAGS@
if FREEBSD
libibutil_la_LDFLAGS = @HTP_LDFLAGS@ -lssp_nonshared 
else
libibutil_la_LDFLAGS = @HTP_LDFLAGS@
libibutil_la_LIBADD = -ldl
endif
