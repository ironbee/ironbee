include $(top_srcdir)/build/common.mk

SUBDIRS = tests

lib_LTLIBRARIES = libpredicate.la
libpredicate_la_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS)
libpredicate_la_LDFLAGS = $(LDFLAGS) $(BOOST_LDFLAGS) \
                     -version-info @LIBRARY_VERSION@ \
                     -release @MAJOR_VERSION@.@MINOR_VERSION@
libpredicate_la_SOURCES = \
    dag.cpp \
    call_factory.cpp \
    parse.cpp \
    standard.cpp \
    dot.cpp \
    bfs.cpp \
    merge_graph.cpp