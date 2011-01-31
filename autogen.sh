#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Generate
set -x
aclocal
libtoolize --automake --force --copy
autoconf
autoheader
automake --add-missing --force --copy --foreign
