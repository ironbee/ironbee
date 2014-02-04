# -Wno-unused-parameter is because IronBee is so callback heavy multiple
# implements ignore one or more of their parameters.
# In C++, we could ommit the name, but not in C.
AM_CPPFLAGS += @IB_DEBUG@ \
               -I$(top_srcdir)/tests \
               -I$(top_srcdir)/include \
               -I$(top_srcdir)/util \
               -I$(top_srcdir)/engine \
               -I$(top_srcdir) \
               -I$(top_builddir) \
               -I$(top_builddir)/include \
               -I$(top_srcdir)/libs/libhtp/htp \
               -Wno-unused-parameter \
			   $(BOOST_CPPFLAGS)

AM_LDFLAGS += \
	$(PCRE_LDFLAGS) \
	$(BOOST_LDFLAGS) \
	-L$(abs_top_builddir)/tests -libtest

if DARWIN
if ENABLE_LUA
# On Darwin (OSX) this is necessary for LuaJIT to run properly.
AM_LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif
endif

if LINUX
AM_LDFLAGS += -lrt
if CC_IS_CLANG
AM_LDFLAGS += -lpthread
endif
endif

# Alias for "check"
test: check

if AUTOMAKE113
LOG_COMPILER = $(top_srcdir)/tests/gtest_executor.sh
else
TESTS_ENVIRONMENT = $(top_srcdir)/tests/gtest_executor.sh
endif

$(abs_builddir)/%: $(srcdir)/%
	if [ "$(builddir)" != "" -a "$(builddir)" != "$(srcdir)" ]; then \
	  cp -f $< $@; \
	fi

CLEANFILES = \
	*_details.xml \
	*_stderr.log \
	*_valgrind_memcheck.xml \
	ironbee_gtest.conf_* \
	clipp_test_*.config \
	clipp_test_*.pb \
	clipp_test_*.clipp

.PHONY: check-ruby
check-ruby:
if CPP
if RUBY_CODE
	if [ -z "$$GTEST_FILTER" ]; then \
	   (cd $(srcdir); abs_builddir=$(abs_builddir) abs_top_builddir=$(abs_top_builddir) abs_srcdir=$(abs_srcdir) abs_top_srcdir=$(abs_top_srcdir) $(RUBY) -I . ./ts_all.rb --verbose $(test_args)) \
	fi
endif
if ENABLE_LUA
	if [ -z "$$GTEST_FILTER" ] && [ -e $(srcdir)/ts_lua.rb ]; then \
		(cd $(srcdir); abs_builddir=$(abs_builddir) abs_top_builddir=$(abs_top_builddir) abs_srcdir=$(abs_srcdir) abs_top_srcdir=$(abs_top_srcdir) $(RUBY) -I . ./ts_lua.rb --verbose $(test_args)); \
	fi
endif
endif

