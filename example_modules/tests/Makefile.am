include $(top_srcdir)/build/tests.mk

check-local:
	(cd $(srcdir); abs_builddir=$(abs_builddir) abs_top_builddir=$(abs_top_builddir) $(RUBY) ./ts_all.rb --verbose $(test_args))
