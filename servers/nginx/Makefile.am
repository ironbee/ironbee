# Only make anything on "make nginx" or "make nginx-install"
# To do anything earlier would fail, as we can't make nginx without
# buiding the executable, which in turn requires that the libraries
# be already installed
#
CLOCAL_AMFLAGS = -I $(abs_top_srcdir)/acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

NGINX_NAME = nginx-1.7.1
NGINX_DIR = $(NGINX_NAME)
NGINX_TARBALL = $(NGINX_NAME).tar.gz
NGINX_URL = http://nginx.org/download/$(NGINX_TARBALL)

NGXIB_SRCDIR = $(abs_top_srcdir)/servers/nginx
NGXIB_BUILDDIR = $(abs_top_builddir)/servers/nginx
NGXIB_FILES =	$(NGXIB_SRCDIR)/ngx_ironbee.h		\
		$(NGXIB_SRCDIR)/ngx_ironbee.c		\
		$(NGXIB_SRCDIR)/ngx_ironbee_conn.c	\
		$(NGXIB_SRCDIR)/ngx_ironbee_in.c	\
		$(NGXIB_SRCDIR)/ngx_ironbee_module.c	\
		$(NGXIB_SRCDIR)/ngx_ironbee_plugin.c
NGXIB_PREFIX = $(prefix)/nginx
#NGXIB_COPTS = $(CFLAGS) $(PCRE_CFLAGS) -D_GNU_SOURCE -I$(prefix)/include
NGXIB_COPTS = -O0 -I$(prefix)/include -I. -D_GNU_SOURCE
NGXIB_LDOPTS = $(PCRE_LDFLAGS) $(PCRE_LDADD) -L$(prefix)/lib -lhtp -libutil -lironbee

# Include sources as not built by default target.
EXTRA_DIST = \
	config \
	config.nginx \
	nginx.patch \
	ngx_ironbee.c \
	ngx_ironbee.h \
	ngx_ironbee_conn.c \
	ngx_ironbee_in.c \
	ngx_ironbee_logger.c \
	ngx_ironbee_module.c \
	ngx_ironbee_plugin.c

nginx: $(NGINX_DIR)/objs/nginx

#config: config.in Makefile
#	sed 's#IB_COPTS#$(prefix)/include#' < config.in \
#	| sed 's#IB_LDOPTS#-L$(prefix)/lib -lhtp -libutil -lironbee#' > config

# FIXME: maybe hack up an m4 to detect a downloader?
$(NGINX_TARBALL):
	wget -O $(NGINX_TARBALL) $(NGINX_URL) \
	|| curl -o $(NGINX_TARBALL) $(NGINX_URL) \
	|| (echo "### Please download $(NGINX_TARBALL) from $(NGINX_URL) to the nginx directory ###" && exit 1)

$(NGINX_DIR): $(NGINX_TARBALL)
	tar xzf $(NGINX_TARBALL) \
	&& cd $(NGINX_DIR) \
	&& patch -p0 < $(NGXIB_SRCDIR)/nginx.patch

$(NGINX_DIR)/Makefile: $(NGINX_DIR) Makefile config.nginx
	cd $(NGINX_DIR) \
	&& NGINXIB_CONFIG_FILE=$(NGXIB_SRCDIR)/config.nginx LD_LIBRARY_PATH="$(prefix)/lib:$LD_LIBRARY_PATH" ./configure --with-debug \
	   --prefix=$(NGXIB_PREFIX) \
	   --with-cc-opt="$(NGXIB_COPTS)" \
	   --with-ld-opt="$(NGXIB_LDOPTS)" \
	   --add-module=$(NGXIB_SRCDIR)

$(NGINX_DIR)/objs/nginx: $(NGINX_DIR)/Makefile $(NGXIB_FILES) $(prefix)/lib/libironbee.la
	cd $(NGINX_DIR) && make

$(prefix)/lib/libironbee.la: ../../engine/libironbee.la
	echo "### Please 'make install' to install libraries before making nginx ###" \
	&& exit 1

nginx-install: nginx
	cd $(NGINX_DIR) && make install

tengine-install:	config.tengine.in Makefile
	if [ -z "$(NGX_DSO_TOOL)" ]; then \
		echo "ERROR: NGX_DSO_TOOL not found"; \
		exit 1; \
	fi; \
	NGINXIB_CONFIG_FILE=$(NGXIB_BUILDDIR)/config.tengine $(NGX_DSO_TOOL) --add-module=$(NGXIB_SRCDIR) --dst=$(prefix)/lib
