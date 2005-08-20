#! /usr/bin/python

import os
import sys

SConscript ("scons_config.py")

Import ("env", "opts", "building")

if building == 1:
	env.Append (CPPPATH = ["#/src"])
	SConscript (dirs=['src/', 'src/include/', 'src/base/', 'src/fs/', 'src/tools/'])
	if opts['clients']:
		SConscript (dirs=['src/client/', 'src/renderer/', 'src/sound/', 'src/image/'])
	SConscript (dirs=['src/server/'])
	SConscript (dirs=['src/nq/', 'src/qw/'])
	Import ("nq_sources")
	env.Program (target = "twilight-nq", source = nq_sources)
	env.Alias ('nq', 'twilight-nq');

	if int(opts['clients']):
		Import ("qw_sources")
		env.Program (target = "twilight-qw", source = qw_sources)
		env.Alias ('qw', 'twilight-qw');
	if int(opts['servers']):
		Import ("qwsv_sources")
		env.Program (target = "twilight-qwsv", source = qwsv_sources)
		env.Alias ('qwsv', 'twilight-qwsv');

#	Import ("tools_sources")
#	env.Program (target = "lhbin2c", source = tools_sources)
