"""SCons.Defaults

Builders and other things for the local site.  Here's where we'll
duplicate the functionality of autoconf until we move it into the
installation procedure or use something like qmconf.

The code that reads the registry to find MSVC components was borrowed
from distutils.msvccompiler.

"""

#
# Copyright (c) 2001, 2002, 2003, 2004 The SCons Foundation
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

__revision__ = "/home/scons/scons/branch.0/branch.96/baseline/src/engine/SCons/Defaults.py 0.96.90.D001 2005/02/15 20:11:37 knight"



import os
import os.path
import shutil
import stat
import string
import time
import types
import sys

import SCons.Action
import SCons.Builder
import SCons.Environment
import SCons.Tool
import SCons.Sig

# A placeholder for a default Environment (for fetching source files
# from source code management systems and the like).  This must be
# initialized later, after the top-level directory is set by the calling
# interface.
_default_env = None

# Lazily instantiate the default environment so the overhead of creating
# it doesn't apply when it's not needed.
def DefaultEnvironment(*args, **kw):
    global _default_env
    if not _default_env:
        _default_env = apply(SCons.Environment.Environment, args, kw)
        _default_env._build_signature = 1
        _default_env._calc_module = SCons.Sig.default_module
    return _default_env

# Emitters for setting the shared attribute on object files,
# and an action for checking that all of the source files
# going into a shared library are, in fact, shared.
def StaticObjectEmitter(target, source, env):
    for tgt in target:
        tgt.attributes.shared = None
    return (target, source)

def SharedObjectEmitter(target, source, env):
    for tgt in target:
        tgt.attributes.shared = 1
    return (target, source)

def SharedFlagChecker(source, target, env):
    same = env.subst('$STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME')
    if same == '0' or same == '' or same == 'False':
        for src in source:
            try:
                shared = src.attributes.shared
            except AttributeError:
                shared = None
            if not shared:
                raise SCons.Errors.UserError, "Source file: %s is static and is not compatible with shared target: %s" % (src, target[0])

SharedCheck = SCons.Action.Action(SharedFlagChecker, None)

# Some people were using these variable name before we made
# SourceFileScanner part of the public interface.  Don't break their
# SConscript files until we've given them some fair warning and a
# transition period.
CScan = SCons.Tool.CScanner
DScan = SCons.Tool.DScanner
ObjSourceScan = SCons.Tool.SourceFileScanner
ProgScan = SCons.Tool.ProgramScanner

# This isn't really a tool scanner, so it doesn't quite belong with
# the rest of those in Tool/__init__.py, but I'm not sure where else it
# should go.  Leave it here for now.
import SCons.Scanner.Dir
DirScanner = SCons.Scanner.Dir.DirScanner()

# Actions for common languages.
CAction = SCons.Action.Action("$CCCOM", "$CCCOMSTR")
ShCAction = SCons.Action.Action("$SHCCCOM", "$SHCCCOMSTR")
CXXAction = SCons.Action.Action("$CXXCOM", "$CXXCOMSTR")
ShCXXAction = SCons.Action.Action("$SHCXXCOM", "$SHCXXCOMSTR")

ASAction = SCons.Action.Action("$ASCOM", "$ASCOMSTR")
ASPPAction = SCons.Action.Action("$ASPPCOM", "$ASPPCOMSTR")

LinkAction = SCons.Action.Action("$LINKCOM", "$LINKCOMSTR")
ShLinkAction = SCons.Action.Action("$SHLINKCOM", "$SHLINKCOMSTR")

LdModuleLinkAction = SCons.Action.Action("$LDMODULECOM", "$LDMODULECOMSTR")

def DVI():
    """Common function to generate a DVI file Builder."""
    return SCons.Builder.Builder(action = {},
                                 # The suffix is not configurable via a
                                 # construction variable like $DVISUFFIX
                                 # because the output file name is
                                 # hard-coded within TeX.
                                 suffix = '.dvi')

def PDF():
    """A function for generating the PDF Builder."""
    return SCons.Builder.Builder(action = { },
                                 prefix = '$PDFPREFIX',
                                 suffix = '$PDFSUFFIX')

# Common tasks that we allow users to perform in platform-independent
# ways by creating ActionFactory instances.
ActionFactory = SCons.Action.ActionFactory

Chmod = ActionFactory(os.chmod,
                      lambda dest, mode: 'Chmod("%s", 0%o)' % (dest, mode))

def copy_func(dest, src):
    if os.path.isfile(src):
        return shutil.copy(src, dest)
    else:
        return shutil.copytree(src, dest, 1)

Copy = ActionFactory(copy_func,
                     lambda dest, src: 'Copy("%s", "%s")' % (dest, src))

def delete_func(entry, must_exist=0):
    if not must_exist and not os.path.exists(entry):
        return None
    if not os.path.exists(entry) or os.path.isfile(entry):
        return os.unlink(entry)
    else:
        return shutil.rmtree(entry, 1)

def delete_strfunc(entry, must_exist=0):
    return 'Delete("%s")' % entry

Delete = ActionFactory(delete_func, delete_strfunc)

Mkdir = ActionFactory(os.makedirs,
                      lambda dir: 'Mkdir("%s")' % dir)

Move = ActionFactory(lambda dest, src: os.rename(src, dest),
                     lambda dest, src: 'Move("%s", "%s")' % (dest, src))

def touch_func(file):
    mtime = int(time.time())
    if os.path.exists(file):
        atime = os.path.getatime(file)
    else:
        open(file, 'w')
        atime = mtime
    return os.utime(file, (atime, mtime))

Touch = ActionFactory(touch_func,
                      lambda file: 'Touch("%s")' % file)

# Internal utility functions
def copyFunc(dest, source, env):
    """Install a source file into a destination by copying it (and its
    permission/mode bits)."""
    shutil.copy2(source, dest)
    st = os.stat(source)
    os.chmod(dest, stat.S_IMODE(st[stat.ST_MODE]) | stat.S_IWRITE)
    return 0

def _concat(prefix, list, suffix, env, f=lambda x: x, target=None):
    """Creates a new list from 'list' by first interpolating each
    element in the list using the 'env' dictionary and then calling f
    on the list, and finally concatenating 'prefix' and 'suffix' onto
    each element of the list. A trailing space on 'prefix' or leading
    space on 'suffix' will cause them to be put into separate list
    elements rather than being concatenated."""
    
    if not list:
        return list

    if SCons.Util.is_List(list):
        list = SCons.Util.flatten(list)
    list = f(env.subst_path(list, target=target))

    result = []

    # ensure that prefix and suffix are strings
    prefix = str(env.subst(prefix, SCons.Util.SUBST_RAW))
    suffix = str(env.subst(suffix, SCons.Util.SUBST_RAW))

    for x in list:
        if isinstance(x, SCons.Node.FS.File):
            result.append(x)
            continue
        x = str(x)
        if x:

            if prefix:
                if prefix[-1] == ' ':
                    result.append(prefix[:-1])
                elif x[:len(prefix)] != prefix:
                    x = prefix + x

            result.append(x)

            if suffix:
                if suffix[0] == ' ':
                    result.append(suffix[1:])
                elif x[-len(suffix):] != suffix:
                    result[-1] = result[-1]+suffix

    return result

def _stripixes(prefix, list, suffix, stripprefix, stripsuffix, env, c=None):
    """This is a wrapper around _concat() that checks for the existence
    of prefixes or suffixes on list elements and strips them where it
    finds them.  This is used by tools (like the GNU linker) that need
    to turn something like 'libfoo.a' into '-lfoo'."""
    
    if not callable(c):
        if callable(env["_concat"]):
            c = env["_concat"]
        else:
            c = _concat
    def f(list, sp=stripprefix, ss=stripsuffix):
        result = []
        for l in list:
            if isinstance(l, SCons.Node.FS.File):
                result.append(l)
                continue
            if not SCons.Util.is_String(l):
                l = str(l)
            if l[:len(sp)] == sp:
                l = l[len(sp):]
            if l[-len(ss):] == ss:
                l = l[:-len(ss)]
            result.append(l)
        return result
    return c(prefix, list, suffix, env, f)

def _defines(prefix, defs, suffix, env, c=_concat):
    """A wrapper around _concat that turns a list or string
    into a list of C preprocessor command-line definitions.
    """
    if SCons.Util.is_List(defs):
        l = []
        for d in defs:
            if SCons.Util.is_List(d) or type(d) is types.TupleType:
                l.append(str(d[0]) + '=' + str(d[1]))
            else:
                l.append(str(d))
    elif SCons.Util.is_Dict(defs):
        # The items in a dictionary are stored in random order, but
        # if the order of the command-line options changes from
        # invocation to invocation, then the signature of the command
        # line will change and we'll get random unnecessary rebuilds.
        # Consequently, we have to sort the keys to ensure a
        # consistent order...
        l = []
        keys = defs.keys()
        keys.sort()
        for k in keys:
            v = defs[k]
            if v is None:
                l.append(str(k))
            else:
                l.append(str(k) + '=' + str(v))
    else:
        l = [str(defs)]
    return c(prefix, l, suffix, env)
    
class NullCmdGenerator:
    """This is a callable class that can be used in place of other
    command generators if you don't want them to do anything.

    The __call__ method for this class simply returns the thing
    you instantiated it with.

    Example usage:
    env["DO_NOTHING"] = NullCmdGenerator
    env["LINKCOM"] = "${DO_NOTHING('$LINK $SOURCES $TARGET')}"
    """

    def __init__(self, cmd):
        self.cmd = cmd

    def __call__(self, target, source, env, for_signature=None):
        return self.cmd

class Variable_Method_Caller:
    """A class for finding a construction variable on the stack and
    calling one of its methods.

    We use this to support "construction variables" in our string
    eval()s that actually stand in for methods--specifically, use
    of "RDirs" in call to _concat that should actually execute the
    "TARGET.RDirs" method.  (We used to support this by creating a little
    "build dictionary" that mapped RDirs to the method, but this got in
    the way of Memoizing construction environments, because we had to
    create new environment objects to hold the variables.)
    """
    def __init__(self, variable, method):
        self.variable = variable
        self.method = method
    def __call__(self, *args, **kw):
        try: 1/0
        except ZeroDivisionError: frame = sys.exc_info()[2].tb_frame
        variable = None
        while frame:
            try:
                variable = frame.f_locals[self.variable]
            except KeyError:
                pass
            frame = frame.f_back
        if variable is None:
            return None
        method = getattr(variable, self.method)
        return apply(method, args, kw)

ConstructionEnvironment = {
    'BUILDERS'      : {},
    'SCANNERS'      : [],
    'CPPSUFFIXES'   : SCons.Tool.CSuffixes,
    'DSUFFIXES'     : SCons.Tool.DSuffixes,
    'IDLSUFFIXES'   : SCons.Tool.IDLSuffixes,
    'PDFPREFIX'     : '',
    'PDFSUFFIX'     : '.pdf',
    'PSPREFIX'      : '',
    'PSSUFFIX'      : '.ps',
    'ENV'           : {},
    'INSTALL'       : copyFunc,
    '_concat'       : _concat,
    '_defines'      : _defines,
    '_stripixes'    : _stripixes,
    '_LIBFLAGS'     : '${_concat(LIBLINKPREFIX, LIBS, LIBLINKSUFFIX, __env__)}',
    '_LIBDIRFLAGS'  : '$( ${_concat(LIBDIRPREFIX, LIBPATH, LIBDIRSUFFIX, __env__, RDirs, TARGET)} $)',
    '_CPPINCFLAGS'  : '$( ${_concat(INCPREFIX, CPPPATH, INCSUFFIX, __env__, RDirs, TARGET)} $)',
    '_CPPDEFFLAGS'  : '${_defines(CPPDEFPREFIX, CPPDEFINES, CPPDEFSUFFIX, __env__)}',
    'TEMPFILE'      : NullCmdGenerator,
    'Dir'           : Variable_Method_Caller('TARGET', 'Dir'),
    'File'          : Variable_Method_Caller('TARGET', 'File'),
    'RDirs'         : Variable_Method_Caller('TARGET', 'RDirs'),
}
