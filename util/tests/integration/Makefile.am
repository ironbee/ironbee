include $(top_srcdir)/build/tests.mk

LDADD = \
	$(top_builddir)/util/libibutil.la

if BUILD_RIAK
check_PROGRAMS = test_kvstore_riak
test_kvstore_riak_SOURCES = test_kvstore_riak.cpp
endif

TESTS = $(check_PROGRAMS)

check-programs: $(check_PROGRAMS)
check-libs:  $(check_LTLIBRARIES)
build: check-programs check-libs
