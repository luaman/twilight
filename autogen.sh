#!/bin/sh

set -e

echo "*** Setting up your build system"
aclocal
autoheader
automake --add-missing --gnu
autoconf
echo "*** Done, you should be able to run ./configure now"

