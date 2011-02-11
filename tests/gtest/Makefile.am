CLOCAL_AMFLAGS = -I../../acinclude

include $(top_srcdir)/build/tests.mk

check_LTLIBRARIES = libgtest.la
libgtest_la_SOURCES = src/gtest-all.cc
libgtest_la_CXXFLAGS = -DGTEST_HAS_PTHREAD=0
