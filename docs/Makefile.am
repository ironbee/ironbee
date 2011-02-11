SUBDIRS = docbook
CLEANFILES = *.pdf

clean-local:
	rm -rf doxygen

doxygen: fulldocs.doxy $(DOXYGEN_DEPS)
	@if test -n "$(DOXYGEN)"; then \
	  $(DOXYGEN) $<; \
	else \
	  echo "You need doxygen installed to build the docs."; \
	fi;

doxygen-pdf: doxygen doxygen.pdf

doxygen.pdf: doxygen
	(cd doxygen/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen.pdf

manual:
	@(cd docbook && $(MAKE) manual)

.PHONY: doxygen-pdf manual
