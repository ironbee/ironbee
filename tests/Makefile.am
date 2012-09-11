ACLOCAL_AMFLAGS = -I../acinclude

include $(top_srcdir)/build/tests.mk

LIBUTIL_LDADD = $(top_builddir)/util/libibutil.la $(PCRE_LDADD)
noinst_LIBRARIES = libgtest.a libgtest_main.a

libgtest_a_SOURCES = gtest/gtest-all.cc
libgtest_main_a_SOURCES = gtest/gtest_main.cc

TESTS_ENVIRONMENT = ./gtest_executor.sh

LDADD = $(LIBUTIL_LDADD) libgtest.a

#This is simply broken in FreeBSD 8, but still builds okay on FreeBSD 7.
if FREEBSD
LDFLAGS+=-fstack-protector
endif

if DARWIN
if ENABLE_LUA
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif
endif

if LINUX
LDFLAGS += -lrt
endif

LDFLAGS += $(HTP_LDFLAGS) $(PCRE_LDFLAGS) $(BOOST_LDFLAGS)

# This is a common set of files for runnign tests of modules.
MODULE_TEST_LDADD = $(LDADD) \
                    $(HTP_LDADD) \
                    $(PCRE_LDADD) \
                    $(top_builddir)/engine/libironbee.la

# Point the module base path to the modules build dir
CPPFLAGS+=-DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
          -DRULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
          $(BOOST_CPPFLAGS)

check_PROGRAMS = test_gtest \
                 test_util_types \
                 test_util_mpool \
                 test_util_array \
                 test_util_hash \
                 test_util_list \
                 test_util_flags \
                 test_util_field \
                 test_util_cfgmap \
                 test_util_clock \
                 test_util_dso \
                 test_util_unescape_string \
                 test_util_uuid \
                 test_util_logformat \
                 test_util_lock \
                 test_util_misc \
                 test_util_ahocorasick \
                 test_util_path \
                 test_util_string \
                 test_util_string_lower \
                 test_util_string_trim \
                 test_util_string_wspc \
                 test_util_hex_escape \
                 test_util_expand \
                 test_util_escape \
                 test_util_decode \
                 test_util_stream \
                 test_util_debug \
                 test_util_log \
                 test_engine \
                 test_module_ahocorasick \
                 test_module_pcre \
                 test_operator \
                 test_action \
                 test_config \
                 test_util_ipset \
                 test_util_ip
if ENABLE_LUA
check_PROGRAMS += test_module_rules_lua \
                  test_luajit
endif

check_LTLIBRARIES = libtest_util_dso_lib.la

TESTS=$(check_PROGRAMS)

TEST_EXTRAS = \
       AhoCorasickModuleTest.config \
       ahocorasick.patterns \
       DfaModuleTest.matches.config \
       gtest_executor.sh \
       PcreModuleTest.test_load_module.config \
       PcreModuleTest.test_pcre_operator.config \
       PcreModuleTest.test_match_basic.config \
       PcreModuleTest.test_match_capture.config \
       test_ironbee_lua_api.conf \
       TestIronBeeModuleRulesLua.operator_test.config \
       test_module_rules_lua.lua

EXTRA_DIST = \
	gtest \
	base_fixture.h \
	$(TEST_EXTRAS)

if OUT_OF_TREE
BUILT_SOURCES = $(addprefix $(abs_builddir)/, $(TEST_EXTRAS))
endif

check-programs: $(check_PROGRAMS)
check-libs:  $(check_LTLIBRARIES)
build: check-programs check-libs

$(abs_builddir)/%: $(srcdir)/%
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
	  cp -f $< $@; \
	fi

test_util_hex_escape_SOURCES = test_util_hex_escape.cc \
                               test_gtest.cc \
			       test_main.cc

# TODO: Get libhtp working w/C++
# test_util_bytestr
test_util_unescape_string_SOURCES = test_util_unescape_string.cc \
                                    test_gtest.cc \
                                    test_main.cc

test_gtest_SOURCES = test_gtest.cc test_main.cc

test_util_types_SOURCES = test_util_types.cc test_main.cc

test_util_flags_SOURCES = test_util_flags.cc test_main.cc

test_util_array_SOURCES = test_util_array.cc test_main.cc

test_util_logformat_SOURCES = test_util_logformat.cc test_main.cc

test_util_hash_SOURCES = test_util_hash.cc test_main.cc

test_util_cfgmap_SOURCES = test_util_cfgmap.cc test_main.cc

test_util_clock_SOURCES = test_util_clock.cc test_main.cc

test_util_lock_SOURCES = test_util_lock.cc test_main.cc

test_util_misc_SOURCES = test_util_misc.cc test_main.cc

test_util_dso_SOURCES = test_util_dso.cc test_main.cc
test_util_dso_CFLAGS = -rpath $(PWD)

test_util_dso_lib_CFLAGS = \
	$(CFLAGS) @GCC_CHARACTERISTICS_CFLAGS@ -prefer-pic -shared

libtest_util_dso_lib_la_SOURCES = test_util_dso_lib.c
libtest_util_dso_lib_la_LDFLAGS = \
	$(LDFLAGS) @GCC_CHARACTERISTICS_LDFLAGS@ \
	-shared -version-info 0:0:0 -rpath $(PWD)

test_util_list_SOURCES = test_util_list.cc test_main.cc

test_util_ipset_SOURCES = test_util_ipset.cc test_main.cc

test_util_ip_SOURCES = test_util_ip.cc test_main.cc

test_util_field_SOURCES = test_util_field.cc test_main.cc

test_util_path_SOURCES = test_util_path.cc test_main.cc

test_util_string_SOURCES = test_util_string.cc test_main.cc

test_util_string_lower_SOURCES = test_util_string_lower.cc test_main.cc

test_util_string_trim_SOURCES = test_util_string_trim.cc test_main.cc

test_util_string_wspc_SOURCES = test_util_string_wspc.cc test_main.cc

test_util_expand_SOURCES = test_util_expand.cc test_main.cc

test_util_escape_SOURCES = test_util_escape.cc test_main.cc

test_util_decode_SOURCES = test_util_decode.cc test_main.cc

test_util_stream_SOURCES = test_util_stream.cc test_main.cc

test_util_debug_SOURCES = test_util_debug.cc test_main.cc
test_util_debug_LDADD = $(LDADD) -lboost_regex

test_util_log_SOURCES = test_util_log.cc test_main.cc
test_util_log_LDADD = $(LDADD) -lboost_regex

test_util_uuid_SOURCES = test_util_uuid.cc test_main.cc
test_util_uuid_CPPFLAGS = $(CPPFLAGS) $(OSSP_UUID_CFLAGS)
test_util_uuid_LDADD = $(MODULE_TEST_LDADD) $(OSSP_UUID_LDFLAGS) $(OSSP_UUID_LIBS)

test_util_ahocorasick_SOURCES = test_util_ahocorasick.cc test_main.cc

test_util_mpool_SOURCES = test_util_mpool.cc test_main.cc
test_util_mpool_LDADD = $(LDADD) -lboost_thread-mt -lboost_system

test_engine_SOURCES = test_engine.cc test_main.cc \
                      test_parsed_content.cc \
                      ibtest_util.cc
test_engine_LDADD = $(MODULE_TEST_LDADD)

test_config_SOURCES = test_config.cc test_main.cc
test_config_LDADD = $(MODULE_TEST_LDADD)


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

test_operator_SOURCES = test_operator.cc test_main.cc
test_operator_LDADD = $(MODULE_TEST_LDADD)

test_action_SOURCES = test_action.cc test_main.cc
test_action_LDADD = $(MODULE_TEST_LDADD)

test_module_pcre_SOURCES = test_module_pcre.cc \
                           test_module_dfa.cc \
			   test_main.cc
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
                    -lm

CLEANFILES = *_details.xml *_stderr.log *_valgrind_memcheck.xml
