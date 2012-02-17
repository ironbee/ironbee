ACLOCAL_AMFLAGS = -I../acinclude

LDADD = \
    $(top_builddir)/tests/gtest/libgtest.la \
    $(top_builddir)/tests/test_main.o \
    $(builddir)/../libibpp.la

LDFLAGS = \
    -lstdc++

include $(top_srcdir)/build/tests.mk

check_PROGRAMS = \
    test_catch
TESTS=$(check_PROGRAMS)

test_catch_SOURCES = test_catch.cpp

check-local: $(check_PROGRAMS)
	for cp in $(check_PROGRAMS); do \
		./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
		if test -n "$(VALGRIND)"; then \
		 $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
		fi; \
	done
