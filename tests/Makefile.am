ACLOCAL_AMFLAGS = -I../acinclude
SUBDIRS = gtest
EXTRA_DIST = gtest
include $(top_srcdir)/build/tests.mk

LIBUTIL_LDADD = gtest/libgtest.la $(top_builddir)/util/libibutil.la
if FREEBSD
LIBUTIL_LDADD += -ldl
endif

HTP_LDADD = -lhtp
if FREEBSD
  HTP_LDADD += -liconv
endif

LDADD = $(LIBUTIL_LDADD) $(HTP_LDADD)

#This is simply broken in FreeBSD 8, but still builds okay on FreeBSD 7.
if FREEBSD
	LDFLAGS+=-fstack-protector
endif

# This is a common set of files for runnign tests of modules.
MODULE_TEST_LDADD = $(LDADD) \
                    $(top_builddir)/engine/libironbee.la

# Point the module base path to the modules build dir
CPPFLAGS+=-DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
          -DRULE_BASE_PATH=$(abs_top_builddir)/modules/.libs

check_PROGRAMS = test_gtest \
                 test_util_array \
                 test_util_hash \
                 test_util_list \
                 test_util_radix \
                 test_util_field \
                 test_util_unescape_string \
                 test_util_uuid \
                 test_util_logformat \
                 test_util_ahocorasick \
                 test_engine \
                 test_module_ahocorasick \
                 test_module_trace \
                 test_module_pcre \
                 test_module_poc_sig \
                 test_module_rules_lua \
                 test_operator \
                 test_action

# TODO: Get libhtp working w/C++
#									test_util_bytestr
test_util_unescape_string_SOURCES = test_util_unescape_string.cc \
              						test_gtest.cc \
							        test_main.cc

test_gtest_SOURCES = test_gtest.cc test_main.cc

test_util_array_SOURCES = test_util_array.cc test_main.cc

test_util_logformat_SOURCES = test_util_logformat.cc test_main.cc

test_util_hash_SOURCES = test_util_hash.cc test_main.cc

test_util_list_SOURCES = test_util_list.cc test_main.cc

test_util_radix_SOURCES = test_util_radix.cc test_main.cc

test_util_field_SOURCES = test_util_field.cc test_main.cc
test_util_field_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS)
test_util_field_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS)

test_util_uuid_SOURCES = test_util_uuid.cc test_main.cc

test_util_ahocorasick_SOURCES = test_util_ahocorasick.cc test_main.cc

test_engine_SOURCES = test_engine.cc test_main.cc ibtest_util.cc
test_engine_LDADD = $(MODULE_TEST_LDADD)

test_module_trace_SOURCES = test_module_trace.cc test_main.cc ibtest_util.cc
test_module_trace_LDADD = $(MODULE_TEST_LDADD) 

test_module_rules_lua_SOURCES = test_module_rules_lua.cc \
                                test_main.cc ibtest_util.cc
test_module_rules_lua_CPPFLAGS = $(CPPFLAGS) \
                                 -I$(top_srcdir)/modules \
                                 -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                                 -I$(top_srcdir) \
                                 -D_XOPEN_SOURCE=1 \
                                 -DTEST_LUA_FILE=\"$(abs_top_srcdir)/tests/test_module_rules_lua.lua\" \
                                 -DTEST_FFI_FILE=\"$(abs_top_srcdir)/lua/ironbee-ffi.lua\"
test_module_rules_lua_LDADD = $(MODULE_TEST_LDADD) \
                              $(top_builddir)/lua/libironbee-lua.la \
                              $(top_builddir)/modules/ibmod_rules_la-rules_lua.o \
                              -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                              -lluajit-ironbee \
                              -lm

test_module_ahocorasick_SOURCES = test_module_ahocorasick.cc test_main.cc
test_module_ahocorasick_LDADD = $(MODULE_TEST_LDADD)
test_module_ahocorasick_CPPFLAGS = $(CPPFLAGS) \
                                   -I$(top_srcdir)/modules

test_module_poc_sig_SOURCES = test_module_poc_sig.cc test_main.cc \
                              ibtest_util.cc
test_module_poc_sig_LDADD = $(MODULE_TEST_LDADD)

test_operator_SOURCES = test_operator.cc test_main.cc
test_operator_LDADD = $(MODULE_TEST_LDADD)

test_action_SOURCES = test_action.cc test_main.cc
test_action_LDADD = $(MODULE_TEST_LDADD)

test_module_pcre_SOURCES = test_module_pcre.cc test_main.cc
test_module_pcre_LDADD = $(MODULE_TEST_LDADD)

CLEANFILES = *_details.xml *_stderr.log *_valgrind_memcheck.xml

check-local: $(check_PROGRAMS)
	cp $(abs_top_srcdir)/lua/ironbee-ffi.lua $(abs_top_builddir)/modules/.libs/
	for cp in $(check_PROGRAMS); do \
		./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
		if test -n "$(VALGRIND)"; then \
		 $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
		fi; \
	done

