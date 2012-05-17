include $(top_srcdir)/build/common.mk
bin_PROGRAMS=clipp

check-local:
	$(srcdir)/run_tests.rb \
		$(abs_builddir)/clipp \
		$(abs_builddir)/tests/ironbee.conf

# Avoid various warnings as error in clipp builds because protobuf is not
# careful about warnings.
#
# NOTE: this must be done in CPPFLAGS vs clipp_CPPFLAGS because
#       of option ordering.
CPPFLAGS += -Wno-shadow -Wno-extra

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
    null_consumer.cpp \
    pb_generator.cpp \
    apache_generator.cpp \
    suricata_generator.cpp \
    htp_generator.cpp \
    echo_generator.cpp \
    pcap_generator.cpp \
    connection_modifiers.cpp \
    parse_modifier.cpp \
    unparse_modifier.cpp \
    aggregate_modifier.cpp
clipp_CFLAGS = $(PROTOBUF_CFLAGS) $(NIDS_CFLAGS) $(NET_CFLAGS)
clipp_CPPFLAGS = $(AM_CPPFLAGS) \
    ${PROTOBUF_CPPFLAGS} \
    $(NIDS_CPPFLAGS) \
    $(NET_CPPFLAGS)
clipp_LDFLAGS = \
    $(AM_LDFLAGS) \
    $(PROTOBUF_LDFLAGS) \
    $(NIDS_LDFLAGS) \
    $(NET_LDFLAGS) \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_regex \
    -lprotobuf \
    -lnids -lnet -lpcap 
clipp_LDADD = \
    $(top_builddir)/ironbeepp/libibpp.la \
    $(top_builddir)/engine/libironbee.la
