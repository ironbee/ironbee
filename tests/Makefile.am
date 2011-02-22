ACLOCAL_AMFLAGS = -I../acinclude
SUBDIRS = gtest
EXTRA_DIST = gtest
include $(top_srcdir)/build/tests.mk

LDADD = gtest/libgtest.la

#This is simply broken in FreeBSD 8, but still builds okay on FreeBSD 7.
if FREEBSD
  LDFLAGS+=-fstack-protector
endif 

check_PROGRAMS = test_gtest \
                 test_util_array \
                 test_util_list \
                 test_engine

# TODO: Get libhtp working w/C++
#                 test_util_bytestr

test_gtest_SOURCES = test_gtest.cc
test_gtest_CXXFLAGS = $(AM_CXXFLAGS) @APR_CFLAGS@
test_gtest_CPPFLAGS = @APR_CPPFLAGS@
test_gtest_LDFLAGS = @APR_LDFLAGS@
if FREEBSD
test_gtest_LDADD =  gtest/libgtest.la \
                    @APR_LDADD@
else
test_gtest_LDADD =  gtest/libgtest.la \
                    -ldl \
                    @APR_LDADD@
endif
test_util_array_SOURCES = test_util_array.cc
test_util_array_CXXFLAGS = $(AM_CXXFLAGS) @APR_CFLAGS@
test_util_array_CPPFLAGS = @APR_CPPFLAGS@
test_util_array_LDFLAGS = @APR_LDFLAGS@
if FREEBSD
test_util_array_LDADD =  gtest/libgtest.la \
                    @APR_LDADD@
else
test_util_array_LDADD =  gtest/libgtest.la \
                    -ldl \
                    @APR_LDADD@
endif
test_util_list_SOURCES = test_util_list.cc
test_util_list_CXXFLAGS = $(AM_CXXFLAGS) @APR_CFLAGS@
test_util_list_CPPFLAGS = @APR_CPPFLAGS@
test_util_list_LDFLAGS = @APR_LDFLAGS@
if FREEBSD
test_util_list_LDADD =  gtest/libgtest.la \
                    @APR_LDADD@
else
test_util_list_LDADD =  gtest/libgtest.la \
                    -ldl \
                    @APR_LDADD@
endif

#test_util_bytestr_SOURCES = test_util_bytestr.cc
#test_util_bytestr_CXXFLAGS = $(AM_CXXFLAGS) @APR_CFLAGS@
#test_util_bytestr_CPPFLAGS = @APR_CPPFLAGS@
#test_util_bytestr_LDFLAGS = @APR_LDFLAGS@
#test_util_bytestr_LDADD =  gtest/libgtest.la \
#                          -ldl -lhtp \
#                          @APR_LDADD@

test_engine_SOURCES = test_engine.cc
test_engine_CXXFLAGS = $(AM_CXXFLAGS) @APR_CFLAGS@
test_engine_CPPFLAGS = @APR_CPPFLAGS@
test_engine_LDFLAGS = @APR_LDFLAGS@
if FREEBSD
test_engine_LDADD = gtest/libgtest.la \
                    $(top_builddir)/util/libibutil.la \
                    -lhtp \
                    -liconv
else
test_engine_LDADD = gtest/libgtest.la \
                    $(top_builddir)/util/libibutil.la \
                    -lhtp
endif

CLEANFILES = *_details.xml *_stderr.log *_valgrind_memcheck.xml

check-local: $(check_PROGRAMS)
	for cp in $(check_PROGRAMS); do \
	  ./$${cp} --gtest_output=xml:$${cp}_details.xml 2> $${cp}_stderr.log; \
	  if test -n "$(VALGRIND)"; then \
	   $(VALGRIND) --xml=yes --xml-file=$${cp}_valgrind_memcheck.xml ./$${cp}; \
	  fi; \
	done

