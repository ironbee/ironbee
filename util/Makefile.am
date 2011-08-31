ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = util.c \
                       debug.c mpool.c dso.c \
                       array.c list.c hash.c bytestr.c field.c \
                       cfgmap.c radix.c ahocorasick.c \
                       ironbee_util_private.h
libibutil_la_CFLAGS = @APR_CFLAGS@ @HTP_CFLAGS@
libibutil_la_CPPFLAGS = @APR_CPPFLAGS@ @HTP_CPPFLAGS@
if FREEBSD
libibutil_la_LDFLAGS = @APR_LDFLAGS@ @HTP_LDFLAGS@ -lssp_nonshared 
libibutil_la_LIBADD = @APR_LDADD@
else
libibutil_la_LDFLAGS = @APR_LDFLAGS@ @HTP_LDFLAGS@
libibutil_la_LIBADD = -ldl \
                      @APR_LDADD@
endif
