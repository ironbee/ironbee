CLEANFILES = *.pdf

clean-local:
	rm -rf doxygen

doxygen: doxygen.conf $(DOXYGEN_DEPS)
	@if test -n "$(DOXYGEN)"; then \
	 $(DOXYGEN) $<; \
	else \
	 echo "You need doxygen installed to build the docs."; \
	fi;

doxygen-pdf: doxygen doxygen.pdf

doxygen.pdf: doxygen
	(cd doxygen/latex && make refman.pdf) && cp doxygen/latex/refman.pdf doxygen.pdf

.PHONY: doxygen-pdf
