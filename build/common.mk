CPPFLAGS += @GCC_CHARACTERISTICS_CPPFLAGS@ \
            @IB_DEBUG@ \
            -I$(top_srcdir)/include \
            -I$(top_srcdir)/util \
            -I$(top_srcdir)/engine \
            -DMODULE_BASE_PATH=$(libdir) \
            -DRULE_BASE_PATH=$(libdir)

CFLAGS += @GCC_CHARACTERISTICS_CFLAGS@

LDFLAGS += @GCC_CHARACTERISTICS_LDFLAGS@

# Alias for "check"
test: check

.PHONY: test
