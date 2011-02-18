#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Generate
set -x
aclocal
# TODO: detect glibtoolize
if [ "$OSTYPE" == "darwin10.0" ]; then
    glibtoolize --automake --force --copy
else
    libtoolize --automake --force --copy
fi
autoconf
autoheader
automake --add-missing --force --copy --foreign
