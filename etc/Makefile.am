ACLOCAL_AMFLAGS = -I ../acinclude

include $(top_srcdir)/build/common.mk

etcdir = $(sysconfdir)

etc_DATA = ironbee-cli.conf.example

if BUILD_HTTPD_PLUGIN
etc_DATA += ironbee.conf.example
endif

if BUILD_TS_PLUGIN
etc_DATA += ironbee_trafficserver.conf.example
endif

EXTRA_DIST = ironbee.conf.example \
           ironbee_trafficserver.conf.example \
           ironbee-cli.conf.example
 
