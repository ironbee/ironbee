#! /usr/bin/env python
import os
import re
import sys
import subprocess

if len(sys.argv) != 3 :
    print >>sys.stderr, "usage: run-ragel.py <ragel> <base>"
    sys.exit(1)

ragel = sys.argv[1]
base  = sys.argv[2]
infile = base+".rl"
outfile = base+".c"
srcdir = os.path.dirname(base)

if ragel == "" :
    print >>sys.stderr, \
        "WARNING: %s was modified, but there is no ragel compiler "+ \
        "installed to regenerate the %s file." % ( infile, outfile )
    sys.exit(1)

print "Generating %s from %s using %s" % ( infile, outfile, ragel )
cmd = [ ragel, '-s', infile ]
print "Running '%s'" % ( cmd )
status = subprocess.call(cmd)
if status :
    print >>sys.stderr, "%s exited with status %d" % ( cmd, status )
    sys.exit( status )

try :
    cfile = open(outfile)
    lines = cfile.readlines()
    cfile.close( )
except IOError as e :
    print >>sys.stderr, "Error reading ragel generated C file "+outfile+" :", e
    sys.exit(1)

try :
    cfile = open(outfile, "w")
    for line in lines :
        line = line.rstrip()
        line = re.sub( r'(#line \d+) "'+srcdir+r'/(.*)\"', r'\g<1> "\g<2>"', line )
        print >>cfile, line
    cfile.close( )
except IOError as e :
    print >>sys.stderr, "Error rewriting generated C file "+outfile+" :", e
    sys.exit(1)
