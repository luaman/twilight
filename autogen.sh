#!/bin/sh

set -e

aclocal
automake -a
autoconf
