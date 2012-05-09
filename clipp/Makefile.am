include $(top_srcdir)/build/common.mk
bin_PROGRAMS=clipp

check-local:
	$(srcdir)/run_tests.rb \
		$(abs_builddir)/clipp \
		$(abs_builddir)/tests/ironbee.conf

CPPFLAGS += -Wno-shadow
clipp_SOURCES = \
    clipp.cpp \
    configuration_parser.cpp \
    input.cpp \
    modsec_audit_log.cpp \
    modsec_audit_log_generator.cpp \
    view.cpp \
    raw_generator.cpp \
    ironbee_consumer.cpp \
    clipp.pb.cc \
    pb_consumer.cpp \
    pb_generator.cpp \
    apache_generator.cpp \
    suricata_generator.cpp \
    htp_generator.cpp \
    connection_modifiers.cpp
clipp_LDFLAGS = \
    $(AM_LDFLAGS) \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_regex \
    -lprotobuf
clipp_LDADD = \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la
