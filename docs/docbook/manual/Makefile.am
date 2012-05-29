clean-local:
	rm -rf output

output: $(abs_srcdir)/*.xml $(abs_srcdir)/*.xsl $(abs_srcdir)/*.xpr $(abs_srcdir)/*.sh
	@echo "Building docbook manual..."; \
	if test -n "$(JAVA)"; then \
	  $(abs_srcdir)/generate-docs.sh $(abs_srcdir) $(abs_builddir) \
	else \
	  echo "You need java installed to build the manual."; \
	fi;
