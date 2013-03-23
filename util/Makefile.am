ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

lib_LTLIBRARIES = libibutil.la
libibutil_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_NO_UNUSED_BUT_SET_VARIABLE
libibutil_la_CPPFLAGS += -Wno-unused-but-set-variable
endif

libibutil_la_SOURCES = ahocorasick.c \
                       array.c \
                       bytestr.c \
                       cfgmap.c \
                       clock.c \
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
                       strval.c \
                       strwspc.c \
                       types.c \
                       util.c \
                       uuid.c
if BUILD_RIAK
  libibutil_la_SOURCES += kvstore_riak.c
endif

if BUILD_YAJL
  libibutil_la_SOURCES += json_yajl_common.c \
                          json_yajl_decode.c \
                          json_yajl_encode.c
endif

EXTRA_DIST = \
        ahocorasick_private.h \
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
libibutil_la_LDFLAGS += -version-info @LIBRARY_VERSION@ \
                        -release @MAJOR_VERSION@.@MINOR_VERSION@
