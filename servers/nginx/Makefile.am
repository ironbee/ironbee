# Only make anything on "make nginx" or "make nginx-install"
# To do anything earlier would fail, as we can't make nginx without
# buiding the executable, which in turn requires that the libraries
# be already installed
#
CLOCAL_AMFLAGS = -I $(abs_top_srcdir)/acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

NGINX_NAME = nginx-1.3.10
NGINX_DIR = $(NGINX_NAME)
NGINX_TARBALL = $(NGINX_NAME).tar.gz
NGINX_URL = http://nginx.org/download/$(NGINX_TARBALL)

NGXIB_SRC = $(abs_top_srcdir)/servers/nginx
NGXIB_PREFIX = $(prefix)/nginx
NGXIB_COPTS = $(CFLAGS) $(PCRE_CFLAGS) -I$(prefix)/include
NGXIB_LDOPTS = $(PCRE_LDFLAGS) $(PCRE_LDADD) -L$(prefix)/lib -lhtp -libutil -lironbee

nginx: $(NGINX_DIR)/objs/nginx

# FIXME: maybe hack up an m4 to detect a downloader?
$(NGINX_TARBALL):
	wget -O $(NGINX_TARBALL) $(NGINX_URL) \
	|| curl -o $(NGINX_TARBALL) $(NGINX_URL) \
	|| (echo "### Please download $(NGINX_TARBALL) from $(NGINX_URL) to the nginx directory ###" && exit 1)

$(NGINX_DIR): $(NGINX_TARBALL)
	tar xzf $(NGINX_TARBALL) \
	&& cd $(NGINX_DIR) \
	&& patch -p0 < $(NGXIB_SRC)/nginx.patch

$(NGINX_DIR)/Makefile: $(NGINX_DIR) Makefile
	cd $(NGINX_DIR) \
	&& ./configure --with-debug \
	   --prefix=$(NGXIB_PREFIX) \
	   --with-cc-opt="$(NGXIB_COPTS)" \
	   --with-ld-opt="$(NGXIB_LDOPTS)" \
	   --add-module=$(NGXIB_SRC)

$(NGINX_DIR)/objs/nginx: $(NGINX_DIR)/Makefile $(prefix)/lib/libironbee.la
	cd $(NGINX_DIR) && make

$(prefix)/lib/libironbee.la: ../../engine/libironbee.la
	echo "### Please 'make install' to install libraries before making nginx ###" \
	&& exit 1

nginx-install: nginx
	cd $(NGINX_DIR) && make install
