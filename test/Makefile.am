LDADD = $(top_builddir)/htp/.libs/libhtp.a -lz @LIBICONV@ 
AM_CFLAGS = -g -O2 -std=gnu99
AM_CPPFLAGS = -I$(top_srcdir) -Wno-write-strings
EXTRA_DIST = files
check_PROGRAMS = test_bstr test_main test_utils test_hybrid test_gunzip test_multipart
check_LIBRARIES = libgtest.a

test_bstr_SOURCES = test_bstr.cpp
test_bstr_LDADD = libgtest.a -lpthread $(LDADD)

test_gunzip_SOURCES = test_gunzip.cpp
test_gunzip_LDADD = libgtest.a -lpthread $(LDADD)

test_hybrid_SOURCES = test_hybrid.cpp
test_hybrid_LDADD = libgtest.a -lpthread $(LDADD)

test_main_SOURCES = test_main.cpp test.c
test_main_LDADD = libgtest.a -lpthread $(LDADD)

test_multipart_SOURCES = test_multipart.cpp test.c
test_multipart_LDADD = libgtest.a -lpthread $(LDADD)

test_utils_SOURCES = test_utils.cpp
test_utils_LDADD = libgtest.a -lpthread $(LDADD)

libgtest_a_SOURCES = $(srcdir)/gtest/gtest-all.cc $(srcdir)/gtest/gtest_main.cc $(srcdir)/gtest/gtest.h

TESTS_ENVIRONMENT = srcdir=$(srcdir)/files
TESTS = test_bstr test_main test_utils test_hybrid test_gunzip test_multipart

test: check
	@echo

check-compile-only: all-am
	$(MAKE) $(AM_MAKEFLAGS) $(check_LIBRARIES)
	$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)

