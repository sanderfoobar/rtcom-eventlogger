#!/bin/sh
set -e
# Run this to set up the build system: configure, makefiles, etc.

srcdir=`dirname $0`
test -n "$srcdir" || srcdir=.

cd "$srcdir"

autoreconf -i -f
