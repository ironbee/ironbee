#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Generate
set -x
aclocal
autoconf
automake --add-missing --force --copy --foreign
