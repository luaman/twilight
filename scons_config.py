import commands
import os
import sys
import string

from scons_lib import *

opts = My_Options ();
config_defs = My_Options ()
building = 0

def check_SDL (context, ver):
	context.Message ('Checking for SDL ' + repr(ver) + ' ... ')

	# Version.
	try:
		sdl_ver_str = os.popen("sdl-config --version").read().strip()
		sdl_ver = map(int, sdl_ver_str.split("."))
	except:
		context.Result (0)
		return 0

	if ver > sdl_ver:
		context.Result (repr(sdl_ver) + " (failed)")
		return 0

	# Ok, sdl-config exists, and reports a usable version.
	ParseConfig (context.env, "sdl-config --cflags")
	ParseConfig (context.env, "sdl-config --libs")
	context.Result (1)
	return 1

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

def handle_opts (conf, opts, config_defs):
	ccflags = opts['CFLAGS']
	if int(opts['bitchiness']):
		ccflags += ' -Wcast-qual -Wsign-compare -W'
	if int(opts['werror']):
		ccflags += ' -Werror'
	if int(opts['profile']):
		ccflags += ' -pg -g'
	conf.env.Replace (CCFLAGS = Split (ccflags))
	conf.env.Replace (CC = opts['CC'])
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
	global opts

	env_defs = My_Options ()
	config_defs.set('VERSION', '"0.2.02.cvs"')
	conf_base ()
	opts.args (ARGUMENTS)
	opts.save('config_opts.py')
	conf = Configure(env, custom_tests = {'SDL' : check_SDL, 'cflag' : check_cflag})
	handle_opts (conf, opts, config_defs)
	if conf.SDL ([1, 2, 5]):
		config_defs.set('HAVE_SDL_H', 1)
	else:
		print "Dying, we need SDL 1.2.5 or greater."
		Exit (1)

	check_funcs (conf, config_defs, ['strlcat', 'strlcpy', 'snprintf', \
		'_snprintf', 'vsnprintf', '_vsnprintf', 'strcasecmp', '_stricmp', \
		'strncasecmp', '_strnicmp', 'fcntl', 'stat', '_stat', 'mkdir', \
		'_mkdir', 'SDL_LoadObject'])
	check_cheaders (conf, config_defs, ['unistd.h', 'fcntl.h', 'windef.h', \
		'pwd.h', 'sys/types.h', 'sys/stat.h', 'limits.h', 'signal.h', \
		'sys/time.h', 'time.h', 'execinfo.h', 'dlfcn.h'])

	if conf.CheckLib ('dl', 'dlopen', 1):
		config_defs.set('HAVE_DLOPEN', 1)
	conf.Finish ()
	print "\nWriting src/config.h"
	write_c_defines ("#/src/config.h", config_defs)
	env_defs.set ('CCFLAGS', env['CCFLAGS'])
	env_defs.set ('CC', env['CC'])
	env_defs.set ('LIBS', env['LIBS'])
	env_defs.set ('CPPPATH', env['CPPPATH'])
	if env.has_key ('CPPDEFINES'):
		env_defs.set ('CPPDEFINES', env['CPPDEFINES'])
	env_defs.save('config_env.py')
	config_defs.save('config_defs.py')

	print "\nConfiguration saved."
	print """
  Project Twilight v""" + config_defs.get('VERSION')[1:-1] + """ configuration:
    Build platform              : """ + env['PLATFORM'] + """
    Compiler                    : """ + env['CC'] + """
    Compiler flags              : """ + string.join(env['CCFLAGS'], " ") + """
    Default OpenGL library      : """ + opts['libgl'] + """

  Path information
    Shared (read-only) data in  : """ + opts['sharepath'] + """
    User (writable) data in     : """ + opts['userpath'] + """
    Shared configuration        : """ + opts['shareconf'] + """
    User's configuration        : """ + opts['userconf'] + """
  """

def load_configure (env):
	global building, opts
	env_defs = My_Options ()
	config_defs = My_Options ()
	conf_base ()
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
	building = 1

env = Environment ()

if (ARGUMENTS.has_key('configure')):
	do_configure (env)
else:
	load_configure (env)

Export ("env", "opts", "building")
