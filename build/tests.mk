# -Wno-unused-parameter is because IronBee is so callback heavy multiple
# implements ignore one or more of their parameters.
# In C++, we could ommit the name, but not in C.
CPPFLAGS += @GCC_CHARACTERISTICS_CPPFLAGS@ \
            @IB_DEBUG@ \
            -I$(top_srcdir)/tests/gtest/include \
            -I$(top_srcdir)/include \
            -I$(top_srcdir)/util \
            -I$(top_srcdir)/engine \
            -I$(top_srcdir) \
            -Wno-unused-parameter

CXXFLAGS += -g

LDFLAGS += @GCC_CHARACTERISTICS_LDFLAGS@

# Alias for "check"
test: check

.PHONY: test
