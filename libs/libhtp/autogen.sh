#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Generate
autoreconf -i -f -v
