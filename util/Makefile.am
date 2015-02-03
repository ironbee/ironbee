ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

SUBDIRS = tests

lib_LTLIBRARIES = libibutil.la
libibutil_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_NO_UNUSED_BUT_SET_VARIABLE
libibutil_la_CPPFLAGS += -Wno-unused-but-set-variable
endif

libibutil_la_SOURCES = array.c \
                       bytestr.c \
                       cfgmap.c \
                       clock.c \
                       dso.c \
                       escape.c \
                       field.c \
                       file.c \
                       flags.c \
                       hash.c \
                       ip.c \
                       ipset.c \
                       kvstore.c \
                       kvstore_filesystem.c \
                       list.c \
                       lock.c \
                       logformat.c \
                       modsec_compat.c \
                       mm.c \
                       mm_mpool.c \
                       mm_mpool_lite.c \
                       mpool.c \
                       mpool_freeable.c \
                       mm_mpool_freeable.c \
                       mpool_lite.c \
                       path.c \
                       queue.c \
                       resource_pool.c \
                       stream.c \
                       string.c \
                       stringset.c \
                       string_assembly.c \
                       string_lower.c \
                       string_trim.c \
                       strval.c \
                       string_whitespace.c \
                       types.c \
                       type_convert.c \
                       vector.c \
                       util.c \
                       uuid.c
if BUILD_RIAK
  libibutil_la_SOURCES += kvstore_riak.c
endif

EXTRA_DIST = \
        json_yajl_private.h \
        kvstore_private.h

libibutil_la_CFLAGS = @OSSP_UUID_CFLAGS@
if FREEBSD
  libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@ -lssp_nonshared @OSSP_UUID_LIBS@
else
  libibutil_la_LDFLAGS = $(LDFLAGS) @OSSP_UUID_LDFLAGS@
  libibutil_la_LIBADD = -ldl @OSSP_UUID_LIBS@
endif
if HAVE_LIBCURL
  libibutil_la_LDFLAGS += $(LIBCURL_LDFLAGS)
  libibutil_la_CFLAGS += $(LIBCURL_CFLAGS)
endif
if BUILD_YAJL
  libibutil_la_SOURCES += json_yajl_common.c \
                          json_yajl_decode.c \
                          json_yajl_encode.c
  libibutil_la_CFLAGS  += @YAJL_CFLAGS@
  libibutil_la_LDFLAGS += @YAJL_LDFLAGS@
endif

libibutil_la_LDFLAGS += -version-info @LIBRARY_VERSION_INFO@ \
                        -release @LIBRARY_RELEASE@
