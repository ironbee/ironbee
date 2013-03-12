ACLOCAL_AMFLAGS = -I../acinclude

include $(top_srcdir)/build/tests.mk

noinst_LIBRARIES = libgtest.a libgtest_main.a

LIBUTIL_LDADD = $(top_builddir)/util/libibutil.la
LIBENGINE_LDADD = $(LIBUTIL_LDADD) $(top_builddir)/engine/libironbee.la

# Turn off Werror for gtest-all.
if HAVE_NO_UNUSED_PRIVATE_FIELD
libgtest_a_CPPFLAGS = $(AM_CPPFLAGS) -Wno-unused-private-field
endif
libgtest_a_SOURCES = gtest/gtest-all.cc
libgtest_main_a_SOURCES = gtest/gtest_main.cc

LDADD = $(LIBENGINE_LDADD) libgtest.a

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
if CC_IS_CLANG
LDFLAGS += -lpthread
endif
endif

SUBDIRS =
DIST_SUBDIRS  =
if INTEGRATION_TESTS
SUBDIRS += integration
DIST_SUBDIRS += integration
endif

LDFLAGS += $(PCRE_LDFLAGS) $(BOOST_LDFLAGS)

# This is a common set of files for runnign tests of modules.
MODULE_TEST_LDADD = $(LDADD) \
                    $(PCRE_LDADD) \
                    -lboost_filesystem$(BOOST_SUFFIX) \
                    -lboost_system$(BOOST_SUFFIX) \
                    $(top_builddir)/libs/libhtp/htp/libhtp.la

# Point the module base path to the modules build dir
AM_CPPFLAGS+=-DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
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
                 test_util_log \
                 test_engine \
                 test_module_ahocorasick \
                 test_module_pcre \
                 test_module_ee_oper \
                 test_operator \
                 test_action \
                 test_config \
                 test_rule_inject \
                 test_util_ipset \
                 test_util_ip \
		 test_kvstore
if ENABLE_LUA
check_PROGRAMS += test_module_rules_lua \
                  test_luajit
endif

if ENABLE_JSON
  check_PROGRAMS += test_util_json
endif

check_LTLIBRARIES = libtest_util_dso_lib.la

TESTS=$(check_PROGRAMS)

TEST_EXTRAS = \
       AhoCorasickModuleTest.config \
       ahocorasick.patterns \
       DfaModuleTest.matches.config \
       EeOperModuleTest.config \
       eudoxus_pattern1.e \
       gtest_executor.sh \
       BasicIronBee.config \
       PcreModuleTest.test_load_module.config \
       PcreModuleTest.test_pcre_operator.config \
       PcreModuleTest.test_match_basic.config \
       PcreModuleTest.test_match_capture.config \
       TestIronBeeModuleRulesLua.operator_test.config \
       CoreActionTest.setVarMult.config \
       CoreActionTest.setVarAdd.config \
       CoreActionTest.setVarSub.config \
       CoreActionTest.integration.config \
       RuleInjectTest.test_inject.config \
       test_ironbee_lua_modules.lua \
       test_ironbee_lua_configs.lua \
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

test_util_hex_escape_SOURCES = test_util_hex_escape.cpp \
                               test_gtest.cpp \
			                   test_main.cpp

# TODO: Get libhtp working w/C++
# test_util_bytestr
test_util_unescape_string_SOURCES = test_util_unescape_string.cpp \
                                    test_gtest.cpp \
                                    test_main.cpp

test_gtest_SOURCES = test_gtest.cpp test_main.cpp

test_util_types_SOURCES = test_util_types.cpp test_main.cpp

test_util_flags_SOURCES = test_util_flags.cpp test_main.cpp

test_util_array_SOURCES = test_util_array.cpp test_main.cpp

test_util_logformat_SOURCES = test_util_logformat.cpp test_main.cpp

test_util_hash_SOURCES = test_util_hash.cpp test_main.cpp

test_util_cfgmap_SOURCES = test_util_cfgmap.cpp test_main.cpp

test_util_clock_SOURCES = test_util_clock.cpp test_main.cpp

test_util_lock_SOURCES = test_util_lock.cpp test_main.cpp

test_util_misc_SOURCES = test_util_misc.cpp test_main.cpp

test_util_dso_SOURCES = test_util_dso.cpp test_main.cpp
test_util_dso_CFLAGS = -rpath $(PWD)

test_util_dso_lib_CFLAGS = \
	$(CFLAGS) -prefer-pic -shared

libtest_util_dso_lib_la_SOURCES = test_util_dso_lib.c
libtest_util_dso_lib_la_LDFLAGS = \
	$(LDFLAGS) \
	-shared -version-info 0:0:0 -rpath $(PWD)

test_util_list_SOURCES = test_util_list.cpp test_main.cpp

test_util_ipset_SOURCES = test_util_ipset.cpp test_main.cpp

test_util_ip_SOURCES = test_util_ip.cpp test_main.cpp

test_util_field_SOURCES = test_util_field.cpp test_main.cpp

test_util_path_SOURCES = test_util_path.cpp test_main.cpp

test_util_json_SOURCES = test_util_json.cpp test_main.cpp

test_util_string_SOURCES = test_util_string.cpp test_main.cpp

test_util_string_lower_SOURCES = test_util_string_lower.cpp test_main.cpp

test_util_string_trim_SOURCES = test_util_string_trim.cpp test_main.cpp

test_util_string_wspc_SOURCES = test_util_string_wspc.cpp test_main.cpp

test_util_expand_SOURCES = test_util_expand.cpp test_main.cpp

test_util_escape_SOURCES = test_util_escape.cpp test_main.cpp

test_util_decode_SOURCES = test_util_decode.cpp test_main.cpp

test_util_stream_SOURCES = test_util_stream.cpp test_main.cpp

test_util_log_SOURCES = test_util_log.cpp test_main.cpp
test_util_log_LDADD = $(LDADD) -lboost_regex$(BOOST_SUFFIX)

test_util_uuid_SOURCES = test_util_uuid.cpp test_main.cpp
test_util_uuid_CPPFLAGS = $(AM_CPPFLAGS) $(OSSP_UUID_CFLAGS)
test_util_uuid_LDADD = $(MODULE_TEST_LDADD) $(OSSP_UUID_LDFLAGS) $(OSSP_UUID_LIBS)

test_util_ahocorasick_SOURCES = test_util_ahocorasick.cpp test_main.cpp

test_util_mpool_SOURCES = test_util_mpool.cpp test_main.cpp
test_util_mpool_LDADD = $(LDADD) -lboost_thread-mt -lboost_system$(BOOST_SUFFIX)

test_engine_SOURCES = test_engine.cpp test_main.cpp \
                      test_parsed_content.cpp \
                      ibtest_util.cpp
test_engine_LDADD = $(MODULE_TEST_LDADD)

test_rule_inject_SOURCES = test_rule_inject.cpp test_main.cpp ibtest_util.cpp
test_rule_inject_LDADD = $(MODULE_TEST_LDADD)

test_config_SOURCES = test_config.cpp test_main.cpp
test_config_LDADD = $(MODULE_TEST_LDADD)


test_module_rules_lua_SOURCES = test_module_rules_lua.cpp \
                                test_main.cpp ibtest_util.cpp
test_module_rules_lua_CPPFLAGS = $(AM_CPPFLAGS) \
                                 -I$(top_srcdir)/modules \
                                 -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                                 -I$(top_srcdir) \
                                 -D_XOPEN_SOURCE=1 \
                                 -DTEST_LUA_FILE=\"$(abs_top_srcdir)/tests/test_module_rules_lua.lua\" \
                                 -DTEST_FFI_FILE=\"$(abs_top_srcdir)/lua/ironbee-ffi.lua\"
test_module_rules_lua_LDADD = $(LDADD) \
                              $(MODULE_TEST_LDADD) \
                              $(top_builddir)/lua/libironbee-lua.la \
                              $(top_builddir)/modules/ibmod_rules_la-lua_common.o \
                              -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                              -lluajit-ironbee \
                              -lm

test_module_ahocorasick_SOURCES = test_module_ahocorasick.cpp test_main.cpp
test_module_ahocorasick_LDADD = $(MODULE_TEST_LDADD)
test_module_ahocorasick_CPPFLAGS = $(AM_CPPFLAGS) \
                                   -I$(top_srcdir)/modules

test_operator_SOURCES = test_operator.cpp test_main.cpp
test_operator_LDADD = $(MODULE_TEST_LDADD)

test_action_SOURCES = test_action.cpp test_core_actions.cpp test_main.cpp
test_action_LDADD = $(MODULE_TEST_LDADD)

test_module_pcre_SOURCES = test_module_pcre.cpp \
                           test_module_dfa.cpp \
			   test_main.cpp
test_module_pcre_LDADD = $(MODULE_TEST_LDADD)

test_luajit_SOURCES = test_main.cpp \
                      test_luajit.cpp \
                      test_ironbee_lua_api.cpp \
                      test_ironbee_lua_configs.cpp \
                      test_ironbee_lua_modules.cpp

test_module_ee_oper_SOURCES = test_module_ee_oper.cpp \
                              test_main.cpp
test_module_ee_oper_LDADD = $(MODULE_TEST_LDADD) \
    $(top_builddir)/automata/libiaeudoxus.la

test_luajit_CPPFLAGS = $(AM_CPPFLAGS) \
                       -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                       -I$(top_srcdir)
test_luajit_LDFLAGS = $(LDFLAGS)
test_luajit_LDADD = $(LDADD) \
                    $(MODULE_TEST_LDADD) \
                    $(top_builddir)/lua/libironbee-lua.la \
                    $(top_builddir)/modules/ibmod_lua_la-lua.o \
                    $(top_builddir)/modules/ibmod_lua_la-lua_common.o \
		    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                    -lluajit-ironbee \
                    -lm

test_kvstore_SOURCES = test_main.cpp \
		       test_kvstore.cpp
test_kvstore_CPPFLAGS = $(AM_CPPFLAGS)
test_kvstore_LDADD = $(LDADD) \
		     $(MODULE_TEST_LDADD) \
		     -lm

CLEANFILES = *_details.xml *_stderr.log *_valgrind_memcheck.xml
