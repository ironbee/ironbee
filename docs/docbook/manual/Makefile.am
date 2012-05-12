clean-local:
	rm -rf output

output: *.xml *.xsl *.xpr *.sh
	@echo "Building docbook manual..."; \
	if test -n "$(JAVA)"; then \
	  ./generate-docs.sh \
	else \
	  echo "You need java installed to build the manual."; \
	fi;
