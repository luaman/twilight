#! /bin/sh
exec perl -w -x $0 ${1+"$@"}
# -*- mode: perl; perl-indent-level: 4; -*-
# vi: ts=4 sw=4
# vim: ft=perl
#! perl -w

# USAGE: ./getglfunc.pl <path to gl.h> <path to glext.h> <function>
# Simple enough.
#
# Copyright (C) 2002 Zephaniah E. Hull.

use strict;
use C::Scan;

sub main {
	my ($glh, $glexth, $func) = @_;
	my ($c, $pfuncs, $pfunc, $arg, @args);

	$c = C::Scan->new('filename' => $glexth, 'add_cppflags' => '-DGL_GLEXT_PROTOTYPES -DAPIENTRY="" -include ' . $glh);

	$pfuncs = $c->get('parsed_fdecls');

	foreach $pfunc (@{$pfuncs}) {
		if ($pfunc->[1] eq $func) {
			foreach $arg (@{$pfunc->[2]}) {
			push (@args, sprintf("%s %s", $arg->[0], $arg->[1]));
		}
	    printf("OGL_NEED (%s, %s, (%s))\n",
			$pfunc->[0], $pfunc->[1], join(', ', @args));
			exit();
		}
	}
}

&main(@ARGV);
