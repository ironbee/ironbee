include $(top_srcdir)/build/common.mk

SUBDIRS = tests

lib_LTLIBRARIES = libibpp.la
libibpp_la_CFLAGS = $(PROTOBUF_CFLAGS) @YAJL_CFLAGS@
libibpp_la_CPPFLAGS = $(AM_CPPFLAGS) $(BOOST_CPPFLAGS) $(PROTOBUF_CPPFLAGS)
libibpp_la_LDFLAGS = $(LDFLAGS) $(BOOST_LDFLAGS) $(PROTOBUF_LDFLAGS) @YAJL_LDFLAGS@ \
                     -version-info @LIBRARY_VERSION_INFO@ \
                     -release @LIBRARY_RELEASE@
libibpp_la_LIBADD = $(PROTOBUF_LDADD)
libibpp_la_SOURCES = \
	action.cpp \
    catch.cpp \
    engine.cpp \
    data.cpp \
    module.cpp \
    context.cpp \
    module_bootstrap.cpp \
    module_delegate.cpp \
    throw.cpp \
    memory_pool.cpp \
    byte_string.cpp \
    field.cpp \
    configuration_map.cpp \
    site.cpp \
    configuration_directives.cpp \
    configuration_parser.cpp \
    connection.cpp \
    transaction.cpp \
    clock.cpp \
    parsed_request_line.cpp \
    parsed_response_line.cpp \
    parsed_header.cpp \
    parser_suite_adaptors.cpp \
    hooks.cpp \
    json.cpp \
    notifier.cpp \
    ironbee.cpp \
    server.cpp \
  	operator.cpp \
  	test_fixture.cpp \
  	transformation.cpp \
  	var.cpp \
    memory_manager.cpp \
    memory_pool_lite.cpp
