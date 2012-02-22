ACLOCAL_AMFLAGS = -I../acinclude

AM_CPPFLAGS += -I$(srcdir)/..

LDADD = \
    $(top_builddir)/tests/gtest/libgtest.la \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libibpp.la \
    $(top_builddir)/util/libibutil.la
    
LDFLAGS = \
    -lstdc++

include $(top_srcdir)/build/tests.mk

check_PROGRAMS = \
    test_catch \
    test_data
TESTS=$(check_PROGRAMS)

test_catch_SOURCES = test_catch.cpp
test_data_SOURCES = test_data.cpp

check-local: $(check_PROGRAMS)
	for cp in $(check_PROGRAMS); do \
		./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
		if test -n "$(VALGRIND)"; then \
		 $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
		fi; \
	done
