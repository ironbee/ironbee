check_PROGRAMS = main
main_SOURCES = main.c test.c test.h	 test-tcpick.c
LDADD = ../htp/.libs/libhtp.a -lz @LIBICONV@ 
AM_CFLAGS = -g -O2
EXTRA_DIST = run-tests.sh
#If we put these in TEST_ENVIRONMNET 'make check' works but 'make distcheck' fails have to use wrapper script 
#TEST_ENVIRONMENT = exec $1 $(srcdir)/files
TESTS_ENVIRONMENT= srcdir=$(srcdir) check_PROGRAMS=$(check_PROGRAMS)
TESTS = run-tests.sh
