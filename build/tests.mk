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
               -Wno-unused-parameter

# Alias for "check"
test: check

if AUTOMAKE113
LOG_COMPILER = $(top_srcdir)/tests/gtest_executor.sh
else
TESTS_ENVIRONMENT = $(top_srcdir)/tests/gtest_executor.sh
endif
