include $(top_srcdir)/build/common.mk
bin_PROGRAMS=ibcli_cpp

override CPPFLAGS = -Wall -Werror

ibcli_cpp_SOURCES = \
  ibcli_cpp.cpp \
  input.cpp
ibcli_cpp_CXXFLAGS = -std=c++11
ibcli_cpp_LDFLAGS = \
  -lboost_system \
  -lboost_filesystem \
  -lboost_program_options \
  -lstdc++
