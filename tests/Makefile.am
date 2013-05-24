include $(top_srcdir)/build/tests.mk

noinst_LIBRARIES = libibtest.a

# Turn off Werror for gtest.
if HAVE_NO_UNUSED_PRIVATE_FIELD
libibtest_a_CPPFLAGS = $(AM_CPPFLAGS) -Wno-unused-private-field
endif
libibtest_a_SOURCES = \
	ibtest_util.cpp \
	gtest/gtest_main.cc \
	gtest/gtest-all.cc

TEST_EXTRAS = \
	gtest_executor.sh \
	BasicIronBee.config

EXTRA_DIST = \
	gtest \
	base_fixture.h \
	$(TEST_EXTRAS)

if OUT_OF_TREE
BUILT_SOURCES = $(addprefix $(abs_builddir)/, $(TEST_EXTRAS))
CLEANFILES += $(TEST_EXTRAS)
endif

SUBDIRS =
DIST_SUBDIRS  =
if INTEGRATION_TESTS
SUBDIRS += integration
DIST_SUBDIRS += integration
endif

