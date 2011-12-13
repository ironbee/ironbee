LDADD = ../htp/.libs/libhtp.a -lz @LIBICONV@ 
AM_CFLAGS = -g -O2
EXTRA_DIST = run-tests.sh gtest

check_PROGRAMS = main test_bstr test_utils

noinst_LTLIBRARIES=libgtest.la

main_SOURCES = main.c test.c test.h	 test-tcpick.c

test_bstr_SOURCES = test_bstr.cc
test_bstr_LDADD = libgtest.la -lpthread $(LDADD)

test_utils_SOURCES = test_utils.cc
test_utils_LDADD = libgtest.la -lpthread $(LDADD)

libgtest_la_SOURCES=$(srcdir)/gtest/gtest-all.cc $(srcdir)/gtest/gtest_main.cc

TESTS_ENVIRONMENT= srcdir=$(srcdir)
TESTS = run-tests.sh test_bstr test_utils

