"""SCons.Tool.aixlink

Tool-specific initialization for the IBM Visual Age linker.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.
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

__revision__ = "/home/scons/scons/branch.0/baseline/src/engine/SCons/Tool/aixlink.py 0.94.D001 2003/11/07 06:02:01 knight"

import os
import os.path

import aixcc
import link

cplusplus = __import__('c++', globals(), locals(), [])

def smart_linkflags(source, target, env, for_signature):
    if cplusplus.iscplusplus(source):
        build_dir = env.subst('$BUILDDIR')
        if build_dir:
            return '-qtempinc=' + os.path.join(build_dir, 'tempinc')
    return ''

def generate(env):
    """
    Add Builders and construction variables for Visual Age linker to
    an Environment.
    """
    link.generate(env)

    env['SMARTLINKFLAGS'] = smart_linkflags
    env['LINKFLAGS']      = '$SMARTLINKFLAGS'
    env['SHLINKFLAGS']    = '$LINKFLAGS -qmkshrobj -qsuppress=1501-218'
    env['SHLIBSUFFIX']    = '.a'

def exists(env):
    path, _cc, _shcc, version = aixcc.get_xlc(env)
    if path and _cc:
        xlc = os.path.join(path, _cc)
        if os.path.exists(xlc):
            return xlc
    return None
