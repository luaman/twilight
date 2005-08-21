#! /usr/bin/python

import os
import sys

SConscript ("scons_config.py")

Import ("env")

env.Append (CPPPATH = ["#/src"])
SConscript (dirs=['src/', 'src/include/', 'src/base/', 'src/fs/', 'src/tools/'])
SConscript (dirs=['src/client/', 'src/renderer/', 'src/sound/', 'src/image/'])
SConscript (dirs=['src/server/'])
SConscript (dirs=['src/nq/', 'src/qw/'])
Import ("nq_sources")
nq = env.Program (target = "twilight-nq", source = nq_sources)
env.Alias ('nq', 'twilight-nq');

Import ("qw_sources")
qw = env.Program (target = "twilight-qw", source = qw_sources)
env.Alias ('qw', 'twilight-qw');
Import ("qwsv_sources")
qwsv = env.Program (target = "twilight-qwsv", source = qwsv_sources)
env.Alias ('qwsv', 'twilight-qwsv');
if int(env['clients']):
	env.Default (nq, qw)
if int(env['servers']):
	env.Default (qwsv)

Import ("tools_sources")
env.Program (target = "lhbin2c", source = tools_sources)
