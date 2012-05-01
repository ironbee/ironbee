ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

etcdir = $(sysconfdir)

SITE_CONFIGS = site-1.conf site-2.conf site-default.conf
etc_DATA = ironbee-cli.conf.example $(SITE_CONFIGS)

if BUILD_HTTPD_SERVER
etc_DATA += ironbee-httpd.conf.example
endif

if BUILD_TS_SERVER
etc_DATA += ironbee-trafficserver.conf.example
endif

EXTRA_DIST = ironbee-httpd.conf.example \
           ironbee-trafficserver.conf.example \
           ironbee-cli.conf.example \
           $(SITE_CONFIGS)
