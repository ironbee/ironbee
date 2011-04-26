
check_PROGRAMS = main
main_SOURCES = main.c test.c test.h	 test-tcpick.c

LDADD = ../htp/.libs/libhtp.a -lz @LIBICONV@ 
AM_CFLAGS = -g -O2

test:
	./main $(srcdir)/files
	
#check: all
# ./main
	
	
