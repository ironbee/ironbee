include $(top_srcdir)/build/common.mk
bin_PROGRAMS=ibcli_cpp

ibcli_cpp_SOURCES = \
  ibcli_cpp.cpp \
  input.cpp \
  audit_log.cpp \
  audit_log_generator.cpp \
  raw_generator.cpp
ibcli_cpp_CXXFLAGS = -std=c++11
ibcli_cpp_LDFLAGS = \
  -lboost_system \
  -lboost_filesystem \
  -lboost_program_options \
  -lboost_regex \
  -lstdc++
