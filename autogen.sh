#!/bin/sh
#
# $Id$

set -e

echo "*** Setting up your build system"
if [ ! -e ChangeLog ]; then
	touch ChangeLog
fi
aclocal
autoheader
automake --add-missing --gnu
autoconf
echo "*** Done, you should be able to run ./configure now"

