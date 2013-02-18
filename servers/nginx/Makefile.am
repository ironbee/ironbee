# Only make anything on "make nginx" or "make nginx-install"
# To do anything earlier would fail, as we can't make nginx without
# buiding the executable, which in turn requires that the libraries
# be already installed
#
CLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk
include $(top_srcdir)/build/servers.mk

NGINX_URL=http://nginx.org/download/nginx-1.3.10.tar.gz
NGINX_TARBALL=nginx-1.3.10.tar.gz
NGINX_DIR=nginx-1.3.10

NGXIB_SRC=..
NGXIB_PREFIX=${prefix}/nginx
NGXIB_COPTS="${CFLAGS} ${PCRE_CFLAGS} -I${prefix}/include"
NGXIB_LDOPTS="${PCRE_LDFLAGS} ${PCRE_LDADD} -L${prefix}/lib -lhtp -libutil -lironbee"

nginx:				${NGINX_DIR}/objs/nginx

# FIXME: maybe hack up an m4 to detect a downloader?
${NGINX_TARBALL}:
				wget -o ${NGINX_TARBALL} ${NGINX_URL}		\
				|| curl -o ${NGINX_TARBALL} ${NGINX_URL}	\
				|| echo "### Please download ${NGINX_TARBALL} from ${NGINX_URL} to the nginx directory ###"	\
				&& exit 1

${NGINX_DIR}:			${NGINX_TARBALL}
				tar xzf ${NGINX_TARBALL}			\
				&& cd ${NGINX_DIR}				\
				&& patch -p0 < ${NGXIB_SRC}/nginx.patch

${NGINX_DIR}/Makefile:		${NGINX_DIR} Makefile
				cd ${NGINX_DIR}					\
				&& ./configure	--with-debug			\
						--prefix=${NGXIB_PREFIX}	\
						--with-cc-opt=${NGXIB_COPTS}	\
						--with-ld-opt=${NGXIB_LDOPTS}	\
						--add-module=${NGXIB_SRC}

${NGINX_DIR}/objs/nginx:	${NGINX_DIR}/Makefile ${prefix}/lib/libironbee.la
				cd ${NGINX_DIR} && make

${prefix}/lib/libironbee.la:	../../engine/libironbee.la
				echo "### Please 'make install' to install libraries before making nginx ###" \
				&& exit 1

nginx-install:			nginx
				cd ${NGINX_DIR} && make install
