noinst_PROGRAMS = \
	modp_b2_test \
	modp_b16_test modp_b64_test modp_b64w_test \
	modp_b85_test modp_burl_test \
	modp_bjavascript_test \
	modp_numtoa_test modp_ascii_test \
	cxx_test \
	speedtest speedtest_numtoa speedtest_ascii

speedtest_SOURCES = apr_base64.h apr_base64.c speedtest.c
speedtest_LDADD = libmodpbase64.la

speedtest_numtoa_SOURCES = speedtest_numtoa.c
speedtest_numtoa_LDADD = libmodpbase64.la

speedtest_ascii.c: modp_ascii_data.h
speedtest_ascii_SOURCES = speedtest_ascii.c 
speedtest_ascii_LDADD = libmodpbase64.la

modp_b2_test_SOURCES = modp_b2_test.c
modp_b2_test_LDADD = libmodpbase64.la

modp_b16_test_SOURCES = modp_b16_test.c
modp_b16_test_LDADD = libmodpbase64.la

modp_b64_test_SOURCES = modp_b64_test.c
modp_b64_test_LDADD = libmodpbase64.la

modp_b64w_test.c: modp_b64_test.c
	perl -p -i -e 's/b64/b64w/g' < modp_b64_test.c > modp_b64w_test.c

modp_b64w_test_SOURCES = modp_b64w_test.c
modp_b64w_test_LDADD = libmodpbase64.la

modp_b85_test_SOURCES = modp_b85_test.c
modp_b85_test_LDADD = libmodpbase64.la

modp_burl_test_SOURCES = modp_burl_test.c
modp_burl_test_LDADD = libmodpbase64.la

modp_bjavascript_test_SOURCES = modp_bjavascript_test.c
modp_bjavascript_test_LDADD = libmodpbase64.la

modp_numtoa_test_SOURCES = modp_numtoa_test.c
modp_numtoa_test_LDADD = libmodpbase64.la

modp_ascii_test_SOURCES = modp_ascii_test.c
modp_ascii_test_LDADD = libmodpbase64.la

cxx_test_SOURCES = cxx_test.cc
cxx_test_LDADD = libmodpbase64.la

.PHONY: test unittest perftest

test: unittest speedtest

unittest: modp_b16_test modp_b64_test modp_b85_test modp_burl_test modp_bjavascript_test modp_numtoa_test modp_ascii_test cxx_test
	@find . -name '*da' | xargs rm -f
	@./modp_b16_test
	@./modp_b64_test
	@./modp_b64w_test
	@./modp_b85_test
	@./modp_b2_test
	@./modp_burl_test
	@./modp_bjavascript_test
	@./modp_numtoa_test
	@./modp_ascii_test
	@./cxx_test

perftest: speedtest speedtest_numtoa speedtest_ascii
	./speedtest
	./speedtest_numtoa
	./speedtest_ascii

CLEANFILES = \
	*~ \
	*.tmp
