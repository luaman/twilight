#! /usr/bin/python

import os
import sys

SConscript ("scons_config.py")

Import ("env", "opts", "building")

if building == 1:
	SConscript (dirs=['src/include/', 'src/base/', 'src/fs/'])
	if opts['clients']:
		SConscript (dirs=['src/client/', 'src/renderer/', 'src/sound/', 'src/image/'])
	SConscript (dirs=['src/server/'])
	SConscript (dirs=['src/nq/', 'src/qw/'])
