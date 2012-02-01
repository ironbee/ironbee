CPPFLAGS += @GCC_CHARACTERISTICS_CPPFLAGS@ \
            @IB_DEBUG@ \
            -I$(top_srcdir)/tests/gtest/include \
            -I$(top_srcdir)/include \
            -I$(top_srcdir)/util \
            -I$(top_srcdir)/engine \
            -I$(top_srcdir)

CXXFLAGS += -g -O2

LDFLAGS += @GCC_CHARACTERISTICS_LDFLAGS@

# Alias for "check"
test: check

.PHONY: test
