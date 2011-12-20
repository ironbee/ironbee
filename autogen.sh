#!/bin/sh

# Cleanup
rm -rf autom4te.cache
touch build/config.rpath

# Generate
set -x
aclocal
# TODO: detect glibtoolize
case x"$OSTYPE" in
    xdarwin*) glibtoolize --automake --force --copy ;;
    *) libtoolize --automake --force --copy ;;
esac
autoconf
autoheader
automake --add-missing --force --copy --foreign
