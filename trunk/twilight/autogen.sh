#!/bin/sh

set -e

aclocal
autoheader
automake -a
autoconf
