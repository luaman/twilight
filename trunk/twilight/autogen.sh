#!/bin/sh
#
# $Id$

set -e

echo "Setting up your build system..."
if [ ! -f ChangeLog ]; then
	touch ChangeLog
fi
aclocal
autoheader
automake --add-missing --copy
autoconf
echo "Okay, you should be able to run ./configure now."

