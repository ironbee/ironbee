
AM_CFLAGS = -g -O2
CPPFLAGS = -I$(top_srcdir)/src
AUTOMAKE_OPTIONS = subdir-objects

check_PROGRAMS = test

noinst_LTLIBRARIES = libgtest.la

test_SOURCES = test.cc
test_LDADD = libgtest.la -lpthread ../src/.libs/libsqltfn.a

libgtest_la_SOURCES = gtest/gtest-all.cc gtest/gtest_main.cc gtest/gtest.h

TESTS = test
