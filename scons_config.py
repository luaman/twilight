import commands
import os
import sys
import string

from scons_lib import *

opts = My_Options ()
env_defs = My_Options ()
config_defs = My_Options ()
building = 0

def parse_SDL_conf(env, output):
	dict = {
		'CPPPATH' : [],
		'LIBPATH' : [],
		'LIBS'    : [],
		'CCFLAGS' : [],
		'LINKFLAGS' : [],
	}
	static_libs = []

	params = string.split(output)
	framework = False
	for arg in params:
		switch = arg[0:1]
		opt = arg[1:2]
		if framework:
			dict['LINKFLAGS'].append(arg)
			framework = False
			continue

		if arg == '-framework':
			dict['LINKFLAGS'].append(arg)
			framework = True
			continue

		if switch == '-':
			if opt == 'L':
				dict['LIBPATH'].append(arg[2:])
			elif opt == 'l':
				dict['LIBS'].append(arg[2:])
			elif opt == 'I':
				dict['CPPPATH'].append(arg[2:])
			else:
				dict['CCFLAGS'].append(arg)
		else:
			static_libs.append(arg)
	apply(env.Append, (), dict)
	return static_libs

def check_SDL_config (context):
	context.Message ('Checking for sdl-config ... ')

	# Version.
	try:
		sdl_ver_str = os.popen("sdl-config --version").read().strip()
		sdl_ver = map(int, sdl_ver_str.split("."))
		context.Result (repr(sdl_ver))
		return (1, sdl_ver)
	except: # Ok, we don't have sdl-config, let's get desperate.
		context.Result (0)
		return (0, [])

#	if ver > sdl_ver:
#		context.Result (repr(sdl_ver) + " (failed)")
#		return 0

	# Ok, sdl-config exists, and reports a usable version.
#	ParseConfig (context.env, "sdl-config --cflags")
#	ParseConfig (context.env, "sdl-config --libs")
#	context.Result (1)
#	return 1

def check_SDL_headers (context):
	context.Message ('Checking for SDL headers ... ')
	ret = context.TryRun ("""
#include <SDL/SDL_version.h>
#include <stdio.h>

int main (int argc, char *argv[])
{
	argc = argc;
	argv = argv;
	printf("%d.%d.%d\\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
	return 0;
}
""", ".c")
	if ret[0]:
		ver = map (int, ret[1].split("."))
		context.Result (repr(ver))
		return (1, ver)
	else:
		context.Result (0)
		return (0, [])

	sdl_ver = context.SDL_config ()

	return sdl_ver

def check_cflag (context, cflag, add = 1):
	context.Message('Checking to see if compiler flag ' + cflag + ' works ... ')
	old_flags = context.env['CCFLAGS']
	context.env['CCFLAGS'] = cflag
	ret = context.TryCompile ("""
	int main (int argc, char *argv[]) {
		return 0;
	}
""", ".c")

	context.env['CCFLAGS'] = old_flags

	if (ret and add):
		context.env.Append (CCFLAGS = [cflag])

	context.Result (ret)
	return ret

def conf_base ():
	global opts
	opts.create('bitchiness', 1, 'Enable (many) extra compiler warnings')
	opts.create('werror', 1, 'Enable error on compiler warnings')
	opts.create('profile', 0, 'Enable profiling with gprof')
	opts.create('servers', 1, 'Enable dedicated servers')
	opts.create('clients', 1, 'Enable clients')
	opts.create('CC', env['CC'], 'C compiler command')
	opts.create('CFLAGS', '-O2 -g -Wall', 'Base C compiler flags')
	opts.create('save-temps', 0, 'Save temporary compilation files')

	if env['PLATFORM'] == "cygwin" or env['PLATFORM'] == "mingw" or env['PLATFORM'] == "win32":
		opts.create('dir_mode', 'dir_win32', 'Directory access mode')
		opts.create('shareconf', '~/twilight.conf', 'Default shared config')
		opts.create('userconf', '~/twilight.rc', 'Default user config')

		opts.create('sharepath', '.', 'Default shared data path')
		opts.create('userpath', '.', 'Default user data path')
		opts.create('sdl_image', 'SDL_image.dll', 'Name of your SDL_image library')
		opts.create('libgl', 'opengl32.dll', 'Name of your OpenGL library')
		config_defs.create('DYNGLENTRY', 'APIENTRY')
	else:
		opts.create('dir_mode', 'dir_posix', 'Directory access mode')
		opts.create('shareconf', '/etc/twilight.conf', 'Default shared config')
		opts.create('userconf', '~/twilight.rc', 'Default user config')

		opts.create('sharepath', '/usr/local/share/games/twilight', 'Default shared data path')
		opts.create('userpath', '~/.twilight', 'Default user data path')
		opts.create('sdl_image', 'libSDL_image-1.2.so.0', 'Name of your SDL_image library')
		opts.create('libgl', 'libGL.so.1', 'Name of your OpenGL library')
		config_defs.create('DYNGLENTRY', '')

	return opts

def check_funcs (conf, defs, funcs):
	for func in funcs:
		if conf.CheckFunc (func):
			defs.set('HAVE_' + string.upper(func), 1)

def check_cheaders (conf, defs, headers):
	for header in headers:
		if conf.CheckCHeader (header):
			str = header.upper()
			str = str.replace('.', '_')
			str = str.replace('/', '_')
			defs.set('HAVE_' + str, 1)

def handle_opts (conf, opts, config_defs, destructive):
	if destructive:
		if int(opts['werror']):
			conf.cflag ('-Werror')
	else:
		ccflags = opts['CFLAGS']
		conf.env.Replace (CCFLAGS = Split (ccflags))
		conf.env.Replace (CC = opts['CC'])
		if int(opts['bitchiness']):
			conf.cflag ('-Wcast-qual')
			conf.cflag ('-Wsign-compare')
			conf.cflag ('-W')
		if int(opts['profile']):
			conf.cflag ('-pg -g')
		if int(opts['save-temps']):
			conf.cflag ('-save-temps', 1)
		else:
			conf.cflag ('-pipe', 1)
		conf.cflag ('-fno-strict-aliasing', 1)
		conf.cflag ('-finline', 1)
		config_defs.set('SDL_IMAGE_LIBRARY', '"' + opts['sdl_image'] + '"')
		config_defs.set('USERPATH', '"' + opts['userpath'] + '"')
		config_defs.set('SHAREPATH', '"' + opts['sharepath'] + '"')
		config_defs.set('USERCONF', '"' + opts['userconf'] + '"')
		config_defs.set('SHARECONF', '"' + opts['userconf'] + '"')
		config_defs.set('GL_LIBRARY', '"' + opts['libgl'] + '"')
		config_defs.set('WANT_CLIENTS', str(opts['clients']))
		config_defs.set('WANT_SERVERS', str(opts['servers']))
		config_defs.set('CFLAG_WERROR', str(opts['werror']))

def write_c_defines (filename, defs):
	env.Append (CPPDEFINES = "HAVE_CONFIG_H")
	fh = file(str(File ("#/src/config.h")), "w")
	for key in defs.keys():
		try:
			fh.write("#define " + key + "\t\t" + defs[key] + "\n")
		except TypeError:
			fh.write("#define " + key + "\t\t" + str(defs[key]) + "\n")
	fh.close();

def do_configure (env):
	global opts, config_defs, env_defs

	config_defs.set('VERSION', '"0.2.02.cvs"')
	opts.args (ARGUMENTS)
	opts.save('config_opts.py')
	tests = {'SDL_config' : check_SDL_config, 'SDL_headers' : check_SDL_headers, 'cflag' : check_cflag}
	conf = Configure(env, custom_tests = tests)
	handle_opts (conf, opts, config_defs, 0)

	ret = conf.SDL_config ()
	sdl_ver = None
	if ret[0]:
		env.ParseConfig ("sdl-config --cflags --libs", parse_SDL_conf)
		check_cheaders (conf, config_defs, ['SDL.h'])
		sdl_ver = ret[1]
	else:
		check_cheaders (conf, config_defs, ['SDL.h'])
		if not config_defs.has_key ('HAVE_SDL_H'):
			print "Twilight requires SDL 1.2.5. (None found.)"
			Exit (1)
		ret = conf.SDL_headers ()
		if ret[0]:
			sdl_ver = ret[1]
			env.Append (LIBS = ['SDL'])

	if [1, 2, 5] > sdl_ver:
		print "Twilight requires SDL 1.2.5. (" + repr (sdl_ver) + " found)"
		Exit (1)

	check_funcs (conf, config_defs, ['strlcat', 'strlcpy', 'snprintf', \
		'_snprintf', 'vsnprintf', '_vsnprintf', 'strcasecmp', '_stricmp', \
		'strncasecmp', '_strnicmp', 'fcntl', 'stat', '_stat', 'mkdir', \
		'_mkdir', 'SDL_LoadObject', 'dlopen'])
	check_cheaders (conf, config_defs, ['unistd.h', 'fcntl.h', 'windef.h', \
		'pwd.h', 'sys/types.h', 'sys/stat.h', 'limits.h', 'signal.h', \
		'sys/time.h', 'time.h', 'execinfo.h', 'dlfcn.h'])

	if not config_defs.has_key('HAVE_DLOPEN'):
		if conf.CheckLib ('dl', 'dlopen', 1):
			config_defs.set('HAVE_DLOPEN', 1)
	handle_opts (conf, opts, config_defs, 1)
	conf.Finish ()

	print "\nWriting src/config.h"
	write_c_defines ("#/src/config.h", config_defs)
	env_defs.set ('CCFLAGS', env['CCFLAGS'])
	env_defs.set ('CC', env['CC'])
	env_defs.set ('LIBS', env['LIBS'])
	env_defs.set ('CPPPATH', env['CPPPATH'])
	if env.has_key ('CPPDEFINES'):
		env_defs.set ('CPPDEFINES', env['CPPDEFINES'])
	if env.has_key ('LINKFLAGS'):
		env_defs.set ('LINKFLAGS', env['LINKFLAGS'])
	if env.has_key ('LIBPATH'):
		env_defs.set ('LIBPATH', env['LIBPATH'])
	env_defs.save('config_env.py')
	config_defs.save('config_defs.py')

	print "\nConfiguration saved."
	print """
  Project Twilight v""" + config_defs.get('VERSION')[1:-1] + """ configuration:
    Build platform              : """ + env['PLATFORM'] + """
    SDL version                 : """ + repr(sdl_ver) + """
    Compiler                    : """ + env['CC'] + """
    Compiler flags              : """ + string.join(env['CCFLAGS'], " ") + """
    Link flags                  : """ + string.join(env['LINKFLAGS'], " ") + """
    Libraries                   : """ + string.join(env['LIBS'], " ") + """
    Lib path                    : """ + string.join(env['LIBPATH'], " ") + """
    Default OpenGL library      : """ + opts['libgl'] + """

  Path information
    Shared (read-only) data in  : """ + opts['sharepath'] + """
    User (writable) data in     : """ + opts['userpath'] + """
    Shared configuration        : """ + opts['shareconf'] + """
    User's configuration        : """ + opts['userconf'] + """
  """

def load_configure (env):
	global building, opts, env_defs, config_defs
	if not env_defs.load ("config_env.py"):
		print "Unable to load config. (1)"
		print "Please run 'scons configure=1'."
		Exit (1)

	if not config_defs.load ("config_defs.py"):
		print "Unable to load config. (2)"
		print "Please run 'scons configure=1'."
		Exit (1)

	if not opts.load ("config_opts.py"):
		print "Unable to load config. (3)"
		print "Please run 'scons configure=1'."
		Exit (1)
	if env_defs.has_key('CCFLAGS'):
		env.Replace (CCFLAGS = env_defs['CCFLAGS'])
	if env_defs.has_key('CC'):
		env.Replace (CC = env_defs['CC'])
	if env_defs.has_key('LIBS'):
		env.Replace (LIBS = env_defs['LIBS'])
	if env_defs.has_key('CPPPATH'):
		env.Replace (CPPPATH = env_defs['CPPPATH'])
	if env_defs.has_key('CPPDEFINES'):
		env.Replace (CPPDEFINES = env_defs['CPPDEFINES'])
	if env_defs.has_key('LINKFLAGS'):
		env.Replace (LINKFLAGS = env_defs['LINKFLAGS'])
	if env_defs.has_key('LIBPATH'):
		env.Replace (LIBPATH = env_defs['LIBPATH'])
	building = 1

env = Environment ()
conf_base ()

#env.Command('configure', None, write_configure)
#AddPreAction('configure', do_configure)
if (ARGUMENTS.has_key('configure')):
	do_configure (env)
else:
	load_configure (env)

Export ("env", "opts", "building")
