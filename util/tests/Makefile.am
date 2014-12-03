ACLOCAL_AMFLAGS = -I../acinclude

include $(top_srcdir)/build/tests.mk

if INTEGRATION_TESTS
SUBDIRS = integration
DIST_SUBDIRS = integration
endif

if CPP
CPP_LDADD = $(top_builddir)/ironbeepp/libibpp.la \
     	$(top_builddir)/engine/libironbee.la
LDADD = $(CPP_LDADD) \
     	$(top_builddir)/util/libibutil.la
else
CPP_LDADD =
LDADD = $(top_builddir)/util/libibutil.la
endif

check_PROGRAMS = \
	test_util_array \
	test_util_bytestr \
	test_util_cfgmap \
	test_util_clock \
	test_util_decode \
	test_util_dso \
	test_util_escape \
	test_util_field \
	test_util_flags \
	test_util_hash \
	test_util_ip \
	test_util_ipset \
  	test_util_json \
	test_util_list \
	test_util_lock \
	test_util_log \
	test_util_logformat \
	test_util_misc \
	test_util_mm \
	test_util_mpool \
	test_util_mpool_lite \
	test_util_mpool_freeable \
	test_util_path \
	test_util_queue \
	test_util_resource_pool \
	test_util_stream \
	test_util_string \
	test_util_stringset \
	test_util_string_lower \
	test_util_string_trim \
	test_util_string_whitespace \
	test_util_strval \
	test_util_uuid \
	test_util_vector \
	test_util_types

EXTRA_DIST = \
	test_util_dso.h \
	integration

if CPP
check_PROGRAMS += \
	test_util_string_assembly
endif


check_LTLIBRARIES = libtest_util_dso_lib.la

TESTS = $(check_PROGRAMS)

check-programs: $(check_PROGRAMS)
check-libs:  $(check_LTLIBRARIES)
build: check-programs check-libs

test_util_types_SOURCES = test_util_types.cpp

test_util_flags_SOURCES = test_util_flags.cpp

test_util_array_SOURCES = test_util_array.cpp

test_util_bytestr_SOURCES = test_util_bytestr.cpp

test_util_logformat_SOURCES = test_util_logformat.cpp

test_util_hash_SOURCES = test_util_hash.cpp

test_util_cfgmap_SOURCES = test_util_cfgmap.cpp

test_util_clock_SOURCES = test_util_clock.cpp

test_util_lock_SOURCES = test_util_lock.cpp

test_util_misc_SOURCES = test_util_misc.cpp

test_util_queue_SOURCES = test_util_queue.cpp

test_util_resource_pool_SOURCES = test_util_resource_pool.cpp

test_util_dso_SOURCES = test_util_dso.cpp
test_util_dso_CFLAGS = -rpath $(PWD)

test_util_dso_lib_CFLAGS = $(AM_CFLAGS) -prefer-pic -shared

libtest_util_dso_lib_la_SOURCES = test_util_dso_lib.c
libtest_util_dso_lib_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-shared -version-info 0:0:0 -rpath $(PWD)

test_util_list_SOURCES = test_util_list.cpp

test_util_ipset_SOURCES = test_util_ipset.cpp

test_util_ip_SOURCES = test_util_ip.cpp

test_util_field_SOURCES = test_util_field.cpp

test_util_path_SOURCES = test_util_path.cpp

test_util_json_SOURCES = test_util_json.cpp

test_util_string_SOURCES = test_util_string.cpp

test_util_stringset_SOURCES = test_util_stringset.cpp

test_util_string_assembly_SOURCES = test_util_string_assembly.cpp

test_util_string_lower_SOURCES = test_util_string_lower.cpp

test_util_string_trim_SOURCES = test_util_string_trim.cpp

test_util_string_whitespace_SOURCES = test_util_string_whitespace.cpp

test_util_strval_SOURCES = test_util_strval.cpp

test_util_escape_SOURCES = test_util_escape.cpp

test_util_decode_SOURCES = test_util_decode.cpp

test_util_stream_SOURCES = test_util_stream.cpp

test_util_vector_SOURCES = test_util_vector.cpp

test_util_log_SOURCES = test_util_log.cpp
test_util_log_LDADD = $(LDADD) -lboost_regex$(BOOST_SUFFIX)

test_util_uuid_SOURCES = test_util_uuid.cpp
test_util_uuid_CPPFLAGS = $(AM_CPPFLAGS) $(OSSP_UUID_CFLAGS)
test_util_uuid_LDADD = $(LDADD) $(OSSP_UUID_LDFLAGS) $(OSSP_UUID_LIBS)

test_util_mpool_SOURCES = test_util_mpool.cpp
test_util_mpool_LDADD = $(LDADD) -lboost_thread$(BOOST_THREAD_SUFFIX) -lboost_system$(BOOST_SUFFIX)

test_util_mm_SOURCES = test_util_mm.cpp

test_util_mpool_lite_SOURCES = test_util_mpool_lite.cpp

test_util_mpool_freeable_SOURCES = test_util_mpool_freeable.cpp
