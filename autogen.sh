#!/bin/sh

# Cleanup
rm -rf autom4te.cache

# Create the macro directory
mkdir - m4

# Generate
autoreconf -i -f -v
