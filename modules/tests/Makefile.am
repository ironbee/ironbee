include $(top_srcdir)/build/tests.mk

LDADD = \
	$(top_builddir)/util/libibutil.la \
	$(top_builddir)/engine/libironbee.la \
    $(top_builddir)/libs/libhtp/htp/libhtp.la

AM_LDFLAGS += \
	-lboost_system$(BOOST_SUFFIX) \
	-lboost_filesystem$(BOOST_SUFFIX)

AM_CPPFLAGS += \
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
endif

TESTS=$(check_PROGRAMS)

TEST_EXTRAS = \
       DfaModuleTest.matches.config \
       EeOperModuleTest.config \
       eudoxus_pattern1.e \
       PcreModuleTest.test_load_module.config \
       PcreModuleTest.test_pcre_operator.config \
       PcreModuleTest.test_match_basic.config \
       PcreModuleTest.test_match_capture.config \
       PcreModuleTest.test_match_capture_named.config \
       test_module_rules_lua.lua \
       TestIronBeeModuleRulesLua.operator_test.config

if ENABLE_LUA
test_module_rules_lua_SOURCES = test_module_rules_lua.cpp
test_module_rules_lua_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
	-D_XOPEN_SOURCE=1 \
	-DTEST_LUA_FILE=\"$(abs_srcdir)/test_module_rules_lua.lua\" \
	-DTEST_FFI_FILE=\"$(abs_top_srcdir)/lua/ironbee-ffi.lua\"
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
CLEANFILES += $(TEST_EXTRAS)
endif

check-programs: $(check_PROGRAMS)
build: check-programs check-libs

test_module_pcre_SOURCES = test_module_pcre.cpp test_module_dfa.cpp

test_module_ee_oper_SOURCES = test_module_ee_oper.cpp
test_module_ee_oper_LDADD = $(LDADD) $(top_builddir)/automata/libiaeudoxus.la

check-local: check-ruby
