#!/bin/sh

# Cleanup
rm -rf autom4te.cache

touch build/config.rpath
autoreconf -i -f  -v
