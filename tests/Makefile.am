ACLOCAL_AMFLAGS = -I../acinclude
SUBDIRS = gtest
EXTRA_DIST = gtest \
       TestIronBeeModuleRulesLua.operator_test.config \
       AhoCorasickModuleTest.config \
       ahocorasick.patterns \
       PcreModuleTest.test_load_module.config \
       test_module_rules_lua.lua \
       base_fixture.h \
       test_ironbee_lua_api.conf \
       PcreModuleTest.matches.config \
       gtest_executor.sh
 
include $(top_srcdir)/build/tests.mk

LIBUTIL_LDADD = gtest/libgtest.la $(top_builddir)/util/libibutil.la

TESTS_ENVIRONMENT = ./gtest_executor.sh

LDADD = $(LIBUTIL_LDADD)

#This is simply broken in FreeBSD 8, but still builds okay on FreeBSD 7.
if FREEBSD
LDFLAGS+=-fstack-protector
endif

if DARWIN
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
LDFLAGS += -pagezero_size 10000 -image_base 100000000 
endif

if LINUX
LDFLAGS += -lrt
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
                 test_util_mpool \
                 test_util_string \
                 test_util_string_trim \
                 test_util_expand \
                 test_engine \
                 test_module_ahocorasick \
                 test_module_trace \
                 test_module_pcre \
                 test_module_poc_sig \
                 test_module_rules_lua \
                 test_operator \
                 test_action \
                 test_luajit \
                 test_config

TESTS=$(check_PROGRAMS)

BUILT_SOURCES = \
    $(abs_builddir)/TestIronBeeModuleRulesLua.operator_test.config \
    $(abs_builddir)/AhoCorasickModuleTest.config \
    $(abs_builddir)/ahocorasick.patterns \
    $(abs_builddir)/PcreModuleTest.test_load_module.config \
    $(abs_builddir)/test_module_rules_lua.lua \
    $(abs_builddir)/test_ironbee_lua_api.conf \
    $(abs_builddir)/PcreModuleTest.matches.config \
    $(abs_builddir)/gtest_executor.sh

$(abs_builddir)/%: $(srcdir)/%
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
	cp -f $< $@; \
    fi

# TODO: Get libhtp working w/C++
# test_util_bytestr
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

test_util_string_SOURCES = test_util_string.cc test_main.cc

test_util_string_trim_SOURCES = test_util_string_trim.cc test_main.cc

test_util_expand_SOURCES = test_util_expand.cc test_main.cc

test_util_uuid_SOURCES = test_util_uuid.cc test_main.cc
test_util_uuid_CPPFLAGS = $(CPPFLAGS) $(OSSP_UUID_CFLAGS)
test_util_uuid_LDADD = $(MODULE_TEST_LDADD) $(OSSP_UUID_LDFLAGS) $(OSSP_UUID_LIBS)

test_util_ahocorasick_SOURCES = test_util_ahocorasick.cc test_main.cc

test_util_mpool_SOURCES = test_util_mpool.cc
test_util_mpool_CFLAGS = $(AM_CFLAGS) $(VALGRIND_CFLAGS)
test_util_mpool_LDADD = $(MODULE_TEST_LDADD)
if HAVE_NO_UNUSED_BUT_SET_VARIABLE
test_util_mpool_CPPFLAGS = $(CPPFLAGS) -Wno-unused-but-set-variable
endif

test_engine_SOURCES = test_engine.cc test_main.cc ibtest_util.cc
test_engine_LDADD = $(MODULE_TEST_LDADD)

test_config_SOURCES = test_config.cc test_main.cc 
test_config_LDADD = $(MODULE_TEST_LDADD)

test_module_trace_SOURCES = test_module_trace.cc test_main.cc ibtest_util.hh ibtest_util.cc
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
test_module_rules_lua_LDADD = $(LDADD) \
                              $(MODULE_TEST_LDADD) \
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

test_luajit_SOURCES = test_main.cc \
                      test_luajit.cc \
                      test_ironbee_lua_api.cc
test_luajit_CPPFLAGS = $(CPPFLAGS) \
                       -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                       -I$(top_srcdir)
test_luajit_LDFLAGS = $(LDFLAGS)
test_luajit_LDADD = $(LDADD) \
                    $(MODULE_TEST_LDADD) \
                    $(top_builddir)/lua/libironbee-lua.la \
                    -lm \
                    gtest/libgtest.la 

CLEANFILES = *_details.xml *_stderr.log *_valgrind_memcheck.xml

#check-local: $(check_PROGRAMS)
#	for cp in $(check_PROGRAMS); do \
#		./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
#		if test -n "$(VALGRIND)"; then \
#		 $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
#		fi; \
#	done
