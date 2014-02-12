include $(top_srcdir)/build/tests.mk

LDADD = \
    $(top_builddir)/util/libibutil.la \
    $(top_builddir)/engine/libironbee.la \
    $(top_builddir)/libs/libhtp/htp/libhtp.la

AM_LDFLAGS += \
    -lboost_system$(BOOST_SUFFIX) \
    -lboost_filesystem$(BOOST_SUFFIX) \
    -lboost_chrono$(BOOST_SUFFIX) \
    -lstdc++

AM_CPPFLAGS += \
    -I$(top_srcdir)/modules \
    -DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
    -DRULE_BASE_PATH=$(abs_top_builddir)/modules/.libs

check_PROGRAMS = \
    test_module_ee_oper \
    test_module_pcre

if ENABLE_LUA
check_PROGRAMS += test_module_rules_lua
endif

if CPP
check_PROGRAMS += test_ibmod_engine_shutdown
test_ibmod_engine_shutdown_SOURCES = test_ibmod_engine_shutdown.cpp
test_ibmod_engine_shutdown_LDADD = \
    $(LDADD) \
    $(top_builddir)/ironbeepp/libibpp.la

check_PROGRAMS       += test_xrules
test_xrules_SOURCES   = test_xrules.cpp
test_xrules_CPPFLAGS  = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
test_xrules_LDFLAGS   = $(AM_LDFLAGS) $(BOOST_LDFLAGS)
test_xrules_LDADD     = \
    $(LDADD) \
    $(top_builddir)/ironbeepp/libibpp.la

check_PROGRAMS           += test_error_page
test_error_page_SOURCES   = test_error_page.cpp
test_error_page_CPPFLAGS  = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
test_error_page_LDFLAGS   = $(AM_LDFLAGS) $(BOOST_LDFLAGS)
test_error_page_LDADD     = \
    $(LDADD) \
    $(top_builddir)/ironbeepp/libibpp.la

check_PROGRAMS += test_txlog
test_txlog_SOURCES = test_txlog.cpp
test_txlog_LDADD = \
    $(LDADD) \
    $(top_builddir)/ironbeepp/libibpp.la

noinst_PROGRAMS = psparse
psparse_LDADD = ../libparser_suite.la
psparse_SOURCES = psparse.cpp
endif

TESTS=$(check_PROGRAMS)

TEST_EXTRAS = \
      DfaModuleTest.matches.config \
      EeOperModuleTest.config \
      error_page.html \
      eudoxus_pattern1.e \
      ee_non_streaming.e \
      init_collection_1.json \
      init_collection_2.json \
      PcreModuleTest.test_load_module.config \
      PcreModuleTest.test_pcre_operator.config \
      PcreModuleTest.test_match_basic.config \
      PcreModuleTest.test_match_capture.config \
      PcreModuleTest.test_match_capture_named.config \
      test_module_rules_lua.lua \
      test_load_relative_to_config_file.lua \
      test_lua_modules.lua \
      test_lua_set.lua \
      TestIronBeeModuleRulesLua.operator_test.config

if ENABLE_LUA
test_module_rules_lua_SOURCES = \
	test_module_rules_lua.cpp \
	test_lua_modules.cpp
test_module_rules_lua_CPPFLAGS = \
    -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
    $(AM_CPPFLAGS) \
    -D_XOPEN_SOURCE=1 \
    -DTEST_LUA_FILE=\"$(abs_srcdir)/test_module_rules_lua.lua\"
test_module_rules_lua_LDADD = \
    $(LDADD) \
    $(top_builddir)/lua/libironbee-lua.la \
    $(top_builddir)/modules/ibmod_rules_la-lua_common.o \
    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
    -lluajit-ironbee
endif

EXTRA_DIST = $(TEST_EXTRAS)

if OUT_OF_TREE
BUILT_SOURCES = $(addprefix $(abs_builddir)/, $(TEST_EXTRAS))
CLEANFILES += $(TEST_EXTRAS) \
    ironbee_gtest.conf_* \
    *.log
endif

check-programs: $(check_PROGRAMS)
build: check-programs check-libs

test_module_pcre_SOURCES = test_module_pcre.cpp

test_module_ee_oper_SOURCES = test_module_ee_oper.cpp
test_module_ee_oper_LDADD = $(LDADD) $(top_builddir)/automata/libiaeudoxus.la

if ENABLE_JSON
check_PROGRAMS += test_persistence
test_persistence_SOURCES = test_persistence.cpp
test_persistence_LDADD = $(LDADD)
endif

check-local: check-ruby
