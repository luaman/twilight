"""SCons.Scanner.C

This module implements the depenency scanner for C/C++ code. 

"""

#
# Copyright (c) 2001, 2002, 2003 Steven Knight
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "/home/scons/scons/branch.0/baseline/src/engine/SCons/Scanner/C.py 0.94.D001 2003/11/07 06:02:01 knight"

import SCons.Node.FS
import SCons.Scanner

def CScan(fs = SCons.Node.FS.default_fs):
    """Return a prototype Scanner instance for scanning source files
    that use the C pre-processor"""
    cs = SCons.Scanner.ClassicCPP("CScan",
                                  [".c", ".C", ".cxx", ".cpp", ".c++", ".cc",
                                   ".h", ".H", ".hxx", ".hpp", ".hh",
                                   ".F", ".fpp", ".FPP",
                                   ".S", ".spp", ".SPP"],
                                  "CPPPATH",
                                  '^[ \t]*#[ \t]*(?:include|import)[ \t]*(<|")([^>"]+)(>|")',
                                  fs = fs)
    return cs
