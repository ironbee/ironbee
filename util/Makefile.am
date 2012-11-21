ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

noinst_LTLIBRARIES = libibutil-c.la
libibutil_c_la_SOURCES = ahocorasick.c \
                         array.c \
                         bytestr.c \
                         cfgmap.c \
                         clock.c \
                         debug.c \
                         decode.c \
                         dso.c \
                         escape.c \
                         expand.c \
                         field.c \
                         hash.c \
                         ip.c \
                         ipset.c \
                         kvstore.c \
                         kvstore_filesystem.c \
                         list.c \
                         lock.c \
                         logformat.c \
                         modsec_compat.c \
                         mpool.c \
                         path.c \
                         regex.c \
                         stream.c \
                         string.c \
                         strlower.c \
                         strtrim.c \
                         strwspc.c \
                         types.c \
                         util.c \
                         uuid.c

libibutil_c_la_CFLAGS = @OSSP_UUID_CFLAGS@
if FREEBSD
libibutil_c_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ -lssp_nonshared @OSSP_UUID_LIBS@ 
else
libibutil_c_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ 
libibutil_c_la_LIBADD = -ldl @OSSP_UUID_LIBS@
endif

lib_LTLIBRARIES = libibutil.la
libibutil_la_SOURCES =
libibutil_la_LIBADD = libibutil-c.la

EXTRA_DIST = ahocorasick_private.h

