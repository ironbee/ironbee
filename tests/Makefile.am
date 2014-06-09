include $(top_srcdir)/build/tests.mk
AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS += -Wno-missing-field-initializers

noinst_LIBRARIES = libibtest.a

# Turn off Werror for gtest.
if HAVE_NO_UNUSED_PRIVATE_FIELD
libibtest_a_CPPFLAGS = $(AM_CPPFLAGS) -Wno-unused-private-field -Wno-unused-const-variable -Wno-unknown-warning-option
else
libibtest_a_CPPFLAGS = $(AM_CPPFLAGS)
endif
libibtest_a_SOURCES = \
	ibtest_util.cpp \
    ibtest_main.cpp \
	gtest/gtest-all.cc

TEST_EXTRAS = \
	gtest_executor.sh \
	BasicIronBee.config

EXTRA_DIST = \
	gtest/gtest.h \
	ibtest_util.hpp \
	ibtest_log_fixture.hpp \
	simple_fixture.hpp \
	base_fixture.h \
	ibtest_lua.hpp \
	$(TEST_EXTRAS)

if OUT_OF_TREE
BUILT_SOURCES = $(addprefix $(abs_builddir)/, $(TEST_EXTRAS))
CLEANFILES += $(TEST_EXTRAS)
endif
