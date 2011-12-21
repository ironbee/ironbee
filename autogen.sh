#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Generate
touch build/config.rpath
autoreconf -i -f  -v
