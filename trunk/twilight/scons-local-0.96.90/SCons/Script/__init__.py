"""SCons.Script

This file implements the main() function used by the scons script.

Architecturally, this *is* the scons script, and will likely only be
called from the external "scons" wrapper.  Consequently, anything here
should not be, or be considered, part of the build engine.  If it's
something that we expect other software to want to use, it should go in
some other module.  If it's specific to the "scons" script invocation,
it goes here.

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

__revision__ = "/home/scons/scons/branch.0/branch.96/baseline/src/engine/SCons/Script/__init__.py 0.96.90.D001 2005/02/15 20:11:37 knight"

import time
start_time = time.time()

import os
import string
import sys
import UserList

# Special chicken-and-egg handling of the "--debug=memoizer" flags:
# SCons.Memoize contains a metaclass implementation that affects how
# the other classes are instantiated.  The Memoizer handles optional
# counting of the hits and misses by using a different, parallel set of
# functions, so we don't slow down normal operation any more than we
# have to.  But if we wait to enable the counting until we've parsed
# the command line options normally, it will be too late, because the
# Memoizer will have already analyzed the classes that it's Memoizing
# and bound the non-counting versions of the functions.  So we have to
# use a special-case, up-front check for the "--debug=memoizer" flag
# and turn on Memoizer counting, if desired, before we import any of
# the other modules that use it.
sconsflags = string.split(os.environ.get('SCONSFLAGS', ''))
if "--debug=memoizer" in sys.argv + sconsflags:
    import SCons.Memoize
    SCons.Memoize.EnableCounting()

import SCons.Action
import SCons.Builder
import SCons.Environment
import SCons.Node.FS
import SCons.Options
import SCons.Platform
import SCons.Scanner
import SCons.SConf
import SCons.Tool
import SCons.Util
import SCons.Defaults

import Main

main                    = Main.main

import SConscript
_SConscript = SConscript

call_stack              = _SConscript.call_stack

#
Action                  = SCons.Action.Action
BoolOption              = SCons.Options.BoolOption
Builder                 = SCons.Builder.Builder
Configure               = _SConscript.Configure
EnumOption              = SCons.Options.EnumOption
Environment             = SCons.Environment.Environment
ListOption              = SCons.Options.ListOption
PackageOption           = SCons.Options.PackageOption
PathOption              = SCons.Options.PathOption
Platform                = SCons.Platform.Platform
Return                  = _SConscript.Return
Scanner                 = SCons.Scanner.Base
Tool                    = SCons.Tool.Tool
WhereIs                 = SCons.Util.WhereIs

# Action factories.
Chmod                   = SCons.Defaults.Chmod
Copy                    = SCons.Defaults.Copy
Delete                  = SCons.Defaults.Delete
Mkdir                   = SCons.Defaults.Mkdir
Move                    = SCons.Defaults.Move
Touch                   = SCons.Defaults.Touch

# Pre-made, public scanners.
CScanner                = SCons.Tool.CScanner
DScanner                = SCons.Tool.DScanner
DirScanner              = SCons.Defaults.DirScanner
ProgramScanner          = SCons.Tool.ProgramScanner
SourceFileScanner       = SCons.Tool.SourceFileScanner

# Functions we might still convert to Environment methods.
CScan                   = SCons.Defaults.CScan
DefaultEnvironment      = SCons.Defaults.DefaultEnvironment

# Other variables we provide.
class TargetList(UserList.UserList):
    def _do_nothing(self, *args, **kw):
        pass
    def _add_Default(self, list):
        self.extend(list)
    def _clear(self):
        del self[:]

ARGUMENTS               = {}
ARGLIST                 = []
BUILD_TARGETS           = TargetList()
COMMAND_LINE_TARGETS    = []
DEFAULT_TARGETS         = []

def _Add_Arguments(alist):
    for arg in alist:
        a, b = string.split(arg, '=', 1)
        ARGUMENTS[a] = b
        ARGLIST.append((a, b))

def _Add_Targets(tlist):
    if tlist:
        COMMAND_LINE_TARGETS.extend(tlist)
        BUILD_TARGETS.extend(tlist)
        BUILD_TARGETS._add_Default = BUILD_TARGETS._do_nothing
        BUILD_TARGETS._clear = BUILD_TARGETS._do_nothing

def _Set_Default_Targets_Has_Been_Called(d, fs):
    return DEFAULT_TARGETS

def _Set_Default_Targets_Has_Not_Been_Called(d, fs):
    if d is None:
        d = [fs.Dir('.')]
    return d

_Get_Default_Targets = _Set_Default_Targets_Has_Not_Been_Called

def _Set_Default_Targets(env, tlist):
    global DEFAULT_TARGETS
    global _Get_Default_Targets
    _Get_Default_Targets = _Set_Default_Targets_Has_Been_Called
    for t in tlist:
        if t is None:
            # Delete the elements from the list in-place, don't
            # reassign an empty list to DEFAULT_TARGETS, so that the
            # variables will still point to the same object we point to.
            del DEFAULT_TARGETS[:]
            BUILD_TARGETS._clear()
        elif isinstance(t, SCons.Node.Node):
            DEFAULT_TARGETS.append(t)
            BUILD_TARGETS._add_Default([t])
        else:
            nodes = env.arg2nodes(t, env.fs.Entry)
            DEFAULT_TARGETS.extend(nodes)
            BUILD_TARGETS._add_Default(nodes)

#
help_text = None

def HelpFunction(text):
    global help_text
    if SCons.Script.help_text is None:
        SCons.Script.help_text = text
    else:
        help_text = help_text + text

#
# Will be set to 1 if we are reading a SConscript.
sconscript_reading = 0

#
def Options(files=None, args=ARGUMENTS):
    return SCons.Options.Options(files, args)

# The list of global functions to add to the SConscript name space
# that end up calling corresponding methods or Builders in the
# DefaultEnvironment().
GlobalDefaultEnvironmentFunctions = [
    # Methods from the SConsEnvironment class, above.
    'Default',
    'EnsurePythonVersion',
    'EnsureSConsVersion',
    'Exit',
    'Export',
    'GetLaunchDir',
    'GetOption',
    'Help',
    'Import',
    'SConscript',
    'SConscriptChdir',
    'SetOption',

    # Methods from the Environment.Base class.
    'AddPostAction',
    'AddPreAction',
    'Alias',
    'AlwaysBuild',
    'BuildDir',
    'CacheDir',
    'Clean',
    #The Command() method is handled separately, below.
    'Depends',
    'Dir',
    'Execute',
    'File',
    'FindFile',
    'Flatten',
    'GetBuildPath',
    'Ignore',
    'Install',
    'InstallAs',
    'Literal',
    'Local',
    'ParseDepends',
    'Precious',
    'Repository',
    'SConsignFile',
    'SideEffect',
    'SourceCode',
    'SourceSignatures',
    'Split',
    'TargetSignatures',
    'Value',
]

GlobalDefaultBuilders = [
    # Supported builders.
    'CFile',
    'CXXFile',
    'DVI',
    'Jar',
    'Java',
    'JavaH',
    'Library',
    'M4',
    'MSVSProject',
    'Object',
    'PCH',
    'PDF',
    'PostScript',
    'Program',
    'RES',
    'RMIC',
    'SharedLibrary',
    'SharedObject',
    'StaticLibrary',
    'StaticObject',
    'Tar',
    'TypeLibrary',
    'Zip',
]

for name in GlobalDefaultEnvironmentFunctions + GlobalDefaultBuilders:
    exec "%s = _SConscript.DefaultEnvironmentCall(%s)" % (name, repr(name))

# The global Command() function must be handled differently than the
# global functions for other construction environment methods because
# we want people to be able to use Actions that must expand $TARGET
# and $SOURCE later, when (and if) the Action is invoked to build
# the target(s).  We do this with the subst=1 argument, which creates
# a DefaultEnvironmentCall instance that wraps up a normal default
# construction environment that performs variable substitution, not a
# proxy that doesn't.
#
# There's a flaw here, though, because any other $-variables on a command
# line will *also* be expanded, each to a null string, but that should
# only be a problem in the unusual case where someone was passing a '$'
# on a command line and *expected* the $ to get through to the shell
# because they were calling Command() and not env.Command()...  This is
# unlikely enough that we're going to leave this as is and cross that
# bridge if someone actually comes to it.
Command = _SConscript.DefaultEnvironmentCall('Command', subst=1)
