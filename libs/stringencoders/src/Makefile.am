AM_CPPFLAGS = -I$(srcdir) -I$(builddir)

noinst_LTLIBRARIES = libmodpbase64.la

GENSRC_LIBMODPBASE64 = \
	modp_b64w.c \
	modp_ascii_data.h \
	modp_b16_data.h \
	modp_b2_data.h \
	modp_b64_data.h \
	modp_b64w_data.h \
	modp_b85_data.h \
	modp_bjavascript_data.h \
	modp_burl_data.h

libmodpbase64_la_SOURCES = \
	modp_ascii.c \
	modp_ascii.h \
	modp_ascii_data.h \
	modp_b16.c \
	modp_b16.h \
	modp_b16_data.h \
	modp_b2.c \
	modp_b2.h \
	modp_b64.c \
	modp_b64.h \
	modp_b64_data.h \
	modp_b64w.h \
	modp_b64w_data.h \
	modp_b85.c \
	modp_b85.h \
	modp_b85_data.h \
	modp_bjavascript.c \
	modp_bjavascript.h \
	modp_burl.c \
	modp_burl.h \
	modp_burl_data.h \
	modp_numtoa.h \
	modp_numtoa.c \
	$(GENSRC_LIBMODPBASE64)

BUILT_SOURCES = $(GENSRC_LIBMODPBASE64)

modp_b64w.c: modp_b64.c modp_b64w.h modp_b64w_data.h
	perl -p -i -e 's/b64/b64w/g' < $(srcdir)/modp_b64.c > modp_b64w.c

modp_b2_data.h: modp_b2_gen
	./modp_b2_gen > modp_b2_data.h

modp_b16_data.h: modp_b16_gen
	./modp_b16_gen > modp_b16_data.h

modp_b64_data.h: modp_b64_gen
	./modp_b64_gen > modp_b64_data.h

modp_ascii_data.h: modp_ascii_gen
	./modp_ascii_gen > modp_ascii_data.h

#
# Recall B64WCHARS by default is "-_."
#
modp_b64w_data.h: modp_b64_gen
	./modp_b64_gen $(B64WCHARS) > modp_b64w_data.h

modp_b85_data.h: modp_b85_gen
	./modp_b85_gen > modp_b85_data.h

modp_burl_data.h: modp_burl_gen
	./modp_burl_gen > modp_burl_data.h 

modp_bjavascript_data.h: modp_bjavascript_gen
	./modp_bjavascript_gen > modp_bjavascript_data.h 

include_HEADERS = \
	modp_ascii.h \
	modp_b16.h \
	modp_b2.h \
	modp_b64.h \
	modp_b64w.h \
	modp_b85.h \
	modp_bjavascript.h \
	modp_burl.h \
	modp_numtoa.h

PROG_DATAGEN = \
	modp_ascii_gen \
	modp_b16_gen \
	modp_b2_gen \
	modp_b64_gen \
	modp_b85_gen \
	modp_bjavascript_gen \
	modp_burl_gen

noinst_PROGRAMS = $(PROG_DATAGEN)

modp_b2_gen_SOURCES = arraytoc.c modp_b2_gen.c
modp_b16_gen_SOURCES = arraytoc.c modp_b16_gen.c
modp_b64_gen_SOURCES = arraytoc.c modp_b64_gen.c
modp_b85_gen_SOURCES = arraytoc.c modp_b85_gen.c
modp_burl_gen_SOURCES = arraytoc.c modp_burl_gen.c
modp_ascii_gen_SOURCES = arraytoc.c modp_ascii_gen.c
modp_bjavascript_gen_SOURCES = arraytoc.c modp_bjavascript_gen.c

CLEANFILES = *.log \
	*~ *~ \
	*.tmp *.tmp \
	*.gcda *.gcno *.gcov stringencoders.info \
	$(DEP_LIBMODPBASE64) \
	$(PROG_DATAGEN)

