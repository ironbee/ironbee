ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES = util.c debug.c mpool.c dso.c \
                       array.c list.c hash.c bytestr.c field.c \
                       cfgmap.c
libibutil_la_CFLAGS = @APR_CFLAGS@
libibutil_la_CPPFLAGS = @APR_CPPFLAGS@
libibutil_la_LDFLAGS = @APR_LDFLAGS@
if FREEBSD
libibutil_la_LIBADD = -lhtp \
                      @APR_LDADD@
libibutil_la_LDFLAGS += -fstack-protector
else
libibutil_la_LIBADD = -ldl -lhtp \
                      @APR_LDADD@
endif
