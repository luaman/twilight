import commands
import os
import sys
import string

from scons_lib import *

opts = My_Options ()
env_defs = My_Options ()
config_defs = My_Options ()
building = 0

def check_mmx_asm (context):
	context.Message('Checking to see if computer understands MMX asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("movq %%mm0, %%mm1" ::: "mm0", "mm1");
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_sse_asm (context):
	context.Message('Checking to see if computer understands SSE asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("movhlps %%xmm0, %%xmm1" ::: "xmm0", "xmm1");
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_func_flag (context, flag):
	context.Message ('Checking for function flag "' + flag + '" ... ')
	ret = context.TryCompile ("""
""" + flag + """ int test (int a, int b)
{
	return a + b;
}
""", ".c")
	context.Result (ret);
	return ret;

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
	framework = 0
	for arg in params:
		switch = arg[0:1]
		opt = arg[1:2]
		if framework:
			dict['LINKFLAGS'].append(arg)
			framework = 0
			continue

		if arg == '-framework':
			dict['LINKFLAGS'].append(arg)
			framework = 1
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
		sdl_ver_str = string.strip(os.popen("sdl-config --version").read())
		sdl_ver = map(int, string.split(sdl_ver_str, "."))
		context.Result (repr(sdl_ver))
		return (1, sdl_ver)
	except: # Ok, we don't have sdl-config, let's get desperate.
		context.Result (0)
		return (0, [])

def check_SDL_headers (context):
	context.Message ('Checking for SDL headers ... ')
	ret = context.TryRun ("""
#include "SDL_version.h"
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

def check_lflag (context, lflag, add = 1):
	context.Message('Checking to see if linker flag ' + lflag + ' works ... ')
	old_flags = context.env['LINKFLAGS']
	context.env['LINKFLAGS'] = lflag
	ret = context.TryCompile ("""
	int main (int argc, char *argv[]) {
		return 0;
	}
""", ".c")

	context.env['LINKFLAGS'] = old_flags

	if (ret and add):
		context.env.Append (LINKFLAGS = [lflag])

	context.Result (ret)
	return ret

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
	opts.create('warnings', 1, 'Enable most warnings')
	opts.create('werror', 1, 'Enable error on compiler warnings')
	opts.create('profile', 0, 'Enable profiling with gprof')
	opts.create('servers', 1, 'Enable dedicated servers')
	opts.create('clients', 1, 'Enable clients')
	opts.create('debug', 1, 'Enable debugging')
	opts.create('optimize', 1, 'Enable optimizations')
	opts.create('CC', env['CC'], 'C compiler command')
	opts.create('CFLAGS', '', 'Base C compiler flags')
	opts.create('save-temps', 0, 'Save temporary compilation files')
	opts.create('sdl_include', '', 'Path to your SDL headers')

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
			str = string.upper(header)
			str = string.replace(str, '.', '_')
			str = string.replace(str, '/', '_')
			defs.set('HAVE_' + str, 1)

def handle_opts (conf, opts, config_defs, destructive):
	if destructive:
		if ('gcc' in env['TOOLS']):
			if int(opts['werror']):
				conf.cflag ('-Werror')
		if ('msvc' in env['TOOLS']):
			conf.cflag ('/MD')
			if int(opts['werror']):
				conf.cflag ('/WX')
	else:
		conf.env.Replace (CC = opts['CC'])
		conf.env.Replace (CCFLAGS = Split (opts['CFLAGS']))
		if ('gcc' in env['TOOLS']):
			if int(opts['optimize']):
				conf.cflag ('-O2')
			if int(opts['debug']):
				conf.cflag ('-g')
			if int(opts['warnings']):
				conf.cflag ('-Wall')
			if int(opts['bitchiness']):
				conf.cflag ('-Wcast-qual')
				conf.cflag ('-Wsign-compare')
				conf.cflag ('-W')
			if int(opts['profile']):
				conf.lflag ('-pg')
				conf.cflag ('-pg')
			if int(opts['save-temps']):
				conf.cflag ('-save-temps', 1)
			else:
				conf.cflag ('-pipe', 1)
			conf.cflag ('-fno-strict-aliasing', 1)
			conf.cflag ('-finline', 1)
		if ('msvc' in env['TOOLS']):
			if int(opts['debug']):
				conf.cflag ('/Zi')
				conf.cflag ('/GZ')
			if int(opts['optimize']):
				conf.cflag ('/Oi', 1)
				conf.cflag ('/G5', 1)
				if (int(opts['debug']) == 0):
					conf.cflag ('/Og', 1)
					conf.cflag ('/O2', 1)

		config_defs.set('SDL_IMAGE_LIBRARY', '"' + opts['sdl_image'] + '"')
		config_defs.set('USERPATH', '"' + opts['userpath'] + '"')
		config_defs.set('SHAREPATH', '"' + opts['sharepath'] + '"')
		config_defs.set('USERCONF', '"' + opts['userconf'] + '"')
		config_defs.set('SHARECONF', '"' + opts['userconf'] + '"')
		config_defs.set('GL_LIBRARY', '"' + opts['libgl'] + '"')

def write_c_defines (filename, defs):
	env.Append (CPPDEFINES = "HAVE_CONFIG_H")
	fh = open(str(File ("#/src/config.h")), "w")
	for key in defs.keys():
		try:
			fh.write("#define " + key + "\t\t" + defs[key] + "\n")
		except TypeError:
			fh.write("#define " + key + "\t\t" + str(defs[key]) + "\n")
	fh.close();

def do_configure (env):
	global opts, config_defs, env_defs

	config_defs.set('VERSION', '"0.2.02.cvs"')
	if (not (ARGUMENTS.has_key('reset') and ARGUMENTS['reset'])):
		opts.load('config_opts.py')
	opts.args (ARGUMENTS)
	opts.save('config_opts.py')
	tests = {'SDL_config' : check_SDL_config, 'SDL_headers' : check_SDL_headers, 'cflag' : check_cflag, 'lflag' : check_lflag, 'func_flag' : check_func_flag, 'mmx_asm' : check_mmx_asm, 'sse_asm' : check_sse_asm}
	conf = Configure(env, custom_tests = tests)
	handle_opts (conf, opts, config_defs, 0)

	ret = conf.SDL_config ()
	sdl_ver = None
	if ret[0]:
		env.ParseConfig ("sdl-config --cflags --libs", parse_SDL_conf)
		check_cheaders (conf, config_defs, ['SDL.h'])
		sdl_ver = ret[1]
	else:
		if (opts['sdl_include']):
			env.Append (CPPPATH = [opts['sdl_include']])
		check_cheaders (conf, config_defs, ['SDL.h'])
		if not config_defs.has_key ('HAVE_SDL_H'):
			print "Twilight requires SDL 1.2.5. (None found.)"
			Exit (1)
		ret = conf.SDL_headers ()
		if ret[0]:
			sdl_ver = ret[1]
			env.Append (LIBS = ['SDL'])
			if env['PLATFORM'] == 'win32':
				env.Append (LIBS = ['SDLmain'])

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

	if conf.mmx_asm():
		config_defs.set('HAVE_MMX', 1)
	if conf.sse_asm():
		config_defs.set('HAVE_SSE', 1)

	if not config_defs.has_key('HAVE_DLOPEN'):
		if conf.CheckLib ('dl', 'dlopen', 1):
			config_defs.set('HAVE_DLOPEN', 1)
	handle_opts (conf, opts, config_defs, 1)

	if conf.func_flag('inline __attribute__((always_inline))'):
		config_defs.create('inline', 'inline __attribute__((always_inline))')
	elif conf.func_flag('inline'):
		config_defs.create('inline', 'inline')
	elif conf.func_flag('__inline'):
		config_defs.create('inline', '__inline')

	if env['PLATFORM'] == 'win32':
		env.Append (LIBS = ['user32', 'wsock32', 'shell32', 'gdi32'])
		env.Append (LINKFLAGS = ['/subsystem:windows'])
		env.Append (CCFLAGS = ['/nologo'])

	conf.Finish ()

	print "\nWriting src/config.h"
	write_c_defines ("#/src/config.h", config_defs)

	print """
  Project Twilight v""" + config_defs.get('VERSION')[1:-1] + """ configuration:
    Build platform              : """ + env['PLATFORM'] + """
    SDL version                 : """ + repr(sdl_ver) + """
    Compiler                    : """ + env['CC'] + """
    Compiler flags              : """ + string.join(env['CCFLAGS'], " ")
	if (env.has_key ('LINKFLAGS') and env['LINKFLAGS']):
		print """\
    Link flags                  : """ + string.join(env['LINKFLAGS'], " ")
	print """\
    Libraries                   : """ + string.join(env['LIBS'], " ")
	if (env.has_key ('LIBPATH') and env['LIBPATH']):
		print """\
    Lib path                    : """ + string.join(env['LIBPATH'], " ")
	print """\
    Default OpenGL library      : """ + opts['libgl'] + """

  Path information
    Shared (read-only) data in  : """ + opts['sharepath'] + """
    User (writable) data in     : """ + opts['userpath'] + """
    Shared configuration        : """ + opts['shareconf'] + """
    User's configuration        : """ + opts['userconf'] + """
  """

env = Environment ()
conf_base ()

do_configure (env)
building = 1

Export ("env", "opts", "building")
