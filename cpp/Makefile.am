include $(top_srcdir)/build/common.mk
bin_PROGRAMS=ibcli_cpp

#if DARWIN
#AM_LDFLAGS = -pagezero_size 10000 -image_base 100000000
#endif

ibcli_cpp_SOURCES = \
  ibcli_cpp.cpp \
  input.cpp \
  modsec_audit_log.cpp \
  modsec_audit_log_generator.cpp \
  raw_generator.cpp \
  ironbee.cpp
ibcli_cpp_CXXFLAGS = -std=c++11
ibcli_cpp_LDFLAGS = \
  $(AM_LDFLAGS) \
  -lboost_system \
  -lboost_filesystem \
  -lboost_program_options \
  -lboost_regex \
  -lstdc++
ibcli_cpp_LDADD = \
  $(top_builddir)/engine/libironbee.la
