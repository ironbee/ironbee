include $(top_srcdir)/build/tests.mk

LDADD = \
	$(top_builddir)/util/libibutil.la \
	$(top_builddir)/engine/libironbee.la \
	$(top_builddir)/libs/libhtp/htp/libhtp.la

if CPP
LDADD += $(top_builddir)/ironbeepp/libibpp.la
endif

check_PROGRAMS = \
	test_action \
	test_config \
	test_engine \
	test_engine_manager \
	test_kvstore \
	test_operator \
	test_rule_inject \
  test_rule_hooks

if CPP
check_PROGRAMS += \
	test_var
endif

AM_LDFLAGS += \
	-lboost_system$(BOOST_SUFFIX) \
	-lboost_filesystem$(BOOST_SUFFIX) \
  -lboost_thread$(BOOST_THREAD_SUFFIX)

AM_CPPFLAGS += \
    -DMODULE_BASE_PATH=$(abs_top_builddir)/modules/.libs \
    -DRULE_BASE_PATH=$(abs_top_builddir)/modules/.libs

TESTS = $(check_PROGRAMS)

TEST_EXTRAS = \
       CoreActionTest.setFlag.config \
       CoreActionTest.setVarMult.config \
       CoreActionTest.setVarAdd.config \
       CoreActionTest.setVarSub.config \
       CoreActionTest.integration.config \
       ParseTreeTest.config \
       Huge.config \
       RuleInjectTest.test_inject.config \
       RuleHooksTest.test_basic.config \
       test_ironbee_lua_modules.lua \
       test_ironbee_lua_configs.lua

EXTRA_DIST = $(TEST_EXTRAS)

CLEANFILES += *.log *.Po lua_test_*.lua
if OUT_OF_TREE
BUILT_SOURCES = $(addprefix $(abs_builddir)/, $(TEST_EXTRAS))
CLEANFILES += $(TEST_EXTRAS)
endif

check-programs: $(check_PROGRAMS)
build: check-programs check-libs

test_engine_SOURCES = test_engine.cpp \
                      test_engine_capture.cpp \
                      test_parsed_content.cpp
test_engine_LDADD = $(LDADD) $(top_builddir)/tests/ibtest_util.o

test_engine_manager_SOURCES = test_engine_manager.cpp \
                              test_engine_manager_control_channel.cpp
test_engine_manager_LDADD = $(LDADD) $(top_builddir)/tests/ibtest_util.o

test_rule_inject_SOURCES = test_rule_inject.cpp
test_rule_inject_LDADD = $(LDADD) $(top_builddir)/tests/ibtest_util.o

test_rule_hooks_SOURCES = test_rule_hooks.cpp
#test_rule_hooks_LDADD = $(LDADD) $(top_builddir)/tests/ibtest_util.o

test_config_SOURCES = test_config.cpp \
                      mock_module.c

test_operator_SOURCES = test_operator.cpp

test_action_SOURCES = test_action.cpp test_core_actions.cpp

test_var_SOURCES = test_var.cpp

if ENABLE_LUA
check_PROGRAMS += test_luajit
test_luajit_SOURCES = \
                      test_luajit.cpp \
                      test_ironbee_lua_api.cpp \
                      test_ironbee_lua_configs.cpp \
                      test_ironbee_lua_modules.cpp
test_luajit_CPPFLAGS = -I$(top_srcdir)/libs/luajit-2.0-ironbee/src \
                       $(AM_CPPFLAGS) \
                       -I$(top_srcdir)
test_luajit_LDADD = $(LDADD) \
                    $(top_builddir)/lua/libironbee-lua.la \
                    $(top_builddir)/modules/ibmod_lua_la-lua.lo \
                    $(top_builddir)/modules/ibmod_lua_la-lua_common.lo \
                    $(top_builddir)/modules/ibmod_lua_la-lua_runtime.lo \
                    $(top_builddir)/modules/ibmod_lua_la-lua_modules.lo \
                    $(top_builddir)/modules/ibmod_lua_la-lua_rules.lo \
                    -L$(abs_top_builddir)/libs/luajit-2.0-ironbee/src \
                    -lluajit-ironbee \
                    $(LIBM_LDFLAGS)
endif

test_kvstore_SOURCES = test_kvstore.cpp

clean-local:
	rm -rf TestKVStore.d

check-local: check-ruby
