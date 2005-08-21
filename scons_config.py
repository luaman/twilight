import commands
import os
import sys
import string

if (not (ARGUMENTS.has_key('reset') and ARGUMENTS['reset'])):
    opts = Options ('config_opts.py', ARGUMENTS)
else:
    opts = Options (None, ARGUMENTS)

config_defs = {};

def check_gcc_asm (context):
	context.Message('Checking to see if compiler gcc style asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("ret\\n" :: );
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_gcc_asm_x86_eflags (context):
	context.Message('Checking for x86 eflags asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("pushfl\\n"
	     "popfl\\n"
	     :: );
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_gcc_asm_x86_cpuid (context):
	context.Message('Checking for x86 cpuid asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("cpuid\\n" ::: "eax", "ebx", "ecx");
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_gcc_asm_x86_mmx (context):
	context.Message('Checking for x86 MMX asm ... ')
	ret = context.TryCompile ("""
int main ()
{
	asm ("movq %%mm0, %%mm1" ::: "mm0", "mm1");
	return 0;
}
""", ".c")
	context.Result (ret)
	return ret;

def check_gcc_asm_x86_sse (context):
	context.Message('Checking for x86 SSE asm ... ')
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
	opts.AddOptions (
		BoolOption ('bitchiness', 'Enable (many) extra compiler warnings', 1),
		BoolOption ('warnings', 'Enable most warnings', 1),
		BoolOption ('werror', 'Enable error on compiler warnings', 1),
		BoolOption ('profile', 'Enable profiling with gprof', 0),
		BoolOption ('servers', 'Enable dedicated servers', 1),
		BoolOption ('clients', 'Enable clients', 1),
		BoolOption ('debug', 'Enable debugging', 1),
		BoolOption ('optimize', 'Enable optimizations', 1),
		BoolOption ('save_temps', 'Save temporary compilation files', 0),
		('CC', 'C compiler command', env['CC']),
		('CFLAGS', 'Base C compiler flags', ''),
		('sdl_include', 'Path to your SDL headers', ''))

	if env['PLATFORM'] == "cygwin" or env['PLATFORM'] == "mingw" or env['PLATFORM'] == "win32":
		opts.Add('dir_mode', 'Directory access mode', 'dir_win32')
		opts.Add('shareconf', 'Default shared config', '~/twilight.conf')
		opts.Add('userconf', 'Default user config', '~/twilight.rc')

		opts.Add('sharepath', 'Default shared data path', '.')
		opts.Add('userpath', 'Default user data path', '.')
		opts.Add('sdl_image', 'Name of your SDL_image library', 'SDL_image.dll')
		opts.Add('libgl', 'Name of your OpenGL library', 'opengl32.dll')
		config_defs['DYNGLENTRY'] = 'APIENTRY'
        if ('msvc' in env['TOOLS']):
            opts.Add('MSVS_VERSION', 'which version of MSVC to use')
	elif env['PLATFORM'] == "darwin":
		opts.Add('dir_mode', 'Directory access mode', 'dir_posix')
		opts.Add('shareconf', 'Default shared config', '/Library/Preferences/com.icculus.Twilight.conf')
		opts.Add('userconf', 'Default user config', '~/Library/Preferences/com.icculus.Twilight.conf')
		opts.Add('sharepath', 'Default shared data path', '/Library/Application Support/Twilight')
		opts.Add('userpath', 'Default user data path', '~/Library/Application Support/Twilight')
		opts.Add('sdl_image', 'Name of your SDL_image library', '/sw/lib/libSDL_image-1.2.0.dylib')
		opts.Add('libgl', 'Name of your OpenGL library', 'Provided by SDL')
	else:
		opts.Add('dir_mode', 'Directory access mode', 'dir_posix')
		opts.Add('shareconf', 'Default shared config', '/etc/twilight.conf')
		opts.Add('userconf', 'Default user config', '~/twilight.rc')

		opts.Add('sharepath', 'Default shared data path', '/usr/local/share/games/twilight')
		opts.Add('userpath', 'Default user data path', '~/.twilight')
		opts.Add('sdl_image', 'Name of your SDL_image library', 'libSDL_image-1.2.so.0')
		opts.Add('libgl', 'Name of your OpenGL library', 'libGL.so.1')
		config_defs['DYNGLENTRY'] = '';

	opts.Update(env)
	Help(opts.GenerateHelpText(env))
	return opts

def check_func (conf, defs, func):
	if conf.CheckFunc (func):
		defs['HAVE_' + string.upper(func)] = 1
		return 1
	return 0

def check_funcs (conf, defs, funcs):
	for func in funcs:
		check_func (conf, defs, func)

def check_cheader (conf, defs, header):
	if conf.CheckCHeader (header):
		str = string.upper(header)
		str = string.replace(str, '.', '_')
		str = string.replace(str, '/', '_')
		defs['HAVE_' + str] = 1
		return 1
	return 0

def check_cheaders (conf, defs, headers):
	for header in headers:
		check_cheader (conf, defs, header)

def handle_opts (conf, opts, config_defs, destructive):
	if destructive:
		if ('gcc' in env['TOOLS']):
			if int(env['werror']):
				conf.cflag ('-Werror')
		elif ('msvc' in env['TOOLS']):
			conf.cflag ('/MD')
			if int(env['werror']):
				conf.cflag ('/WX')
	else:
		conf.env.Replace (CCFLAGS = Split (env['CFLAGS']))
		if ('gcc' in env['TOOLS']):
			if int(env['optimize']):
				conf.cflag ('-O2')
			if int(env['debug']):
				conf.cflag ('-g')
			if int(env['warnings']):
				conf.cflag ('-Wall')
			if int(env['bitchiness']):
				conf.cflag ('-Wcast-qual')
				conf.cflag ('-Wsign-compare')
				conf.cflag ('-W')
			if int(env['profile']):
				conf.lflag ('-pg')
				conf.cflag ('-pg')
			if int(env['save_temps']):
				conf.cflag ('-save-temps', 1)
			else:
				conf.cflag ('-pipe', 1)
			conf.cflag ('-fno-strict-aliasing', 1)
			conf.cflag ('-finline', 1)
		elif ('msvc' in env['TOOLS']):
			if int(env['debug']):
				conf.cflag ('/Zi')
				conf.cflag ('/GZ')
			if int(env['optimize']):
				conf.cflag ('/Oi', 1)
				conf.cflag ('/G5', 1)
				if (int(env['debug']) == 0):
					conf.cflag ('/Og', 1)
					conf.cflag ('/O2', 1)

		config_defs['SDL_IMAGE_LIBRARY'] = '"' + env['sdl_image'] + '"'
		config_defs['USERPATH'] = '"' + env['userpath'] + '"'
		config_defs['SHAREPATH'] = '"' + env['sharepath'] + '"'
		config_defs['USERCONF'] = '"' + env['userconf'] + '"'
		config_defs['SHARECONF'] = '"' + env['userconf'] + '"'
		config_defs['GL_LIBRARY'] = '"' + env['libgl'] + '"'

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
	global opts, config_defs;

	config_defs['VERSION'] = '"0.2.2"'
	opts.Save('config_opts.py', env)
	tests = {'SDL_config' : check_SDL_config,
		'SDL_headers' : check_SDL_headers,
		'cflag' : check_cflag,
		'lflag' : check_lflag,
		'func_flag' : check_func_flag,
		'gcc_asm' : check_gcc_asm,
		'gcc_asm_x86_eflags' : check_gcc_asm_x86_eflags,
		'gcc_asm_x86_cpuid' : check_gcc_asm_x86_cpuid,
		'gcc_asm_x86_mmx' : check_gcc_asm_x86_mmx,
		'gcc_asm_x86_sse' : check_gcc_asm_x86_sse}
	conf = Configure(env, custom_tests = tests)
	handle_opts (conf, opts, config_defs, 0)

	ret = conf.SDL_config ()
	sdl_ver = None
	if ret[0]:
		env.ParseConfig ("sdl-config --cflags --libs", parse_SDL_conf)
		check_cheaders (conf, config_defs, ['SDL.h'])
		sdl_ver = ret[1]
	else:
		if (env['sdl_include']):
			env.Append (CPPPATH = [env['sdl_include']])
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

	if not check_func(conf, config_defs, 'strncasecmp'):
		check_func(conf, config_defs, '_strnicmp')
	if not check_func(conf, config_defs, 'strcasecmp'):
		check_func(conf, config_defs, '_stricmp')
	if not check_func(conf, config_defs, 'snprintf'):
		check_func(conf, config_defs, '_snprintf')
	if not check_func(conf, config_defs, 'vsnprintf'):
		check_func(conf, config_defs, '_vsnprintf')

	check_funcs (conf, config_defs, ['strlcat', 'strlcpy', \
		'fcntl', 'mkdir', '_mkdir', 'SDL_LoadObject', 'sigaction'])
	check_cheaders (conf, config_defs, ['unistd.h', 'fcntl.h', 'windef.h', \
		'pwd.h', 'sys/types.h', 'sys/stat.h', 'limits.h', 'signal.h', \
		'sys/time.h', 'time.h', 'execinfo.h', 'dlfcn.h'])

	if conf.gcc_asm ():
		config_defs['HAVE_GCC_ASM'] = 1
		if conf.gcc_asm_x86_eflags ():
			config_defs['HAVE_GCC_ASM_X86_EFLAGS'] = 1
		if conf.gcc_asm_x86_cpuid ():
			config_defs['HAVE_GCC_ASM_X86_CPUID'] = 1
			if conf.gcc_asm_x86_mmx ():
				config_defs['HAVE_GCC_ASM_X86_MMX'] = 1
			if conf.gcc_asm_x86_sse ():
				config_defs['HAVE_GCC_ASM_X86_SSE'] = 1

	if not check_func(conf, config_defs, 'dlopen'):
		if conf.CheckLib ('dl', 'dlopen', 1):
			config_defs['HAVE_DLOPEN'] = 1
	handle_opts (conf, opts, config_defs, 1)

	if conf.func_flag('inline __attribute__((always_inline))'):
		config_defs['inline'] = 'inline __attribute__((always_inline))'
	elif conf.func_flag('inline'):
		config_defs['inline'] = 'inline'
	elif conf.func_flag('__inline'):
		config_defs['inline'] = '__inline'

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
    Default OpenGL library      : """ + env['libgl'] + """

  Path information
    Shared (read-only) data in  : """ + env['sharepath'] + """
    User (writable) data in     : """ + env['userpath'] + """
    Shared configuration        : """ + env['shareconf'] + """
    User's configuration        : """ + env['userconf'] + """
  """

env = Environment ()
conf_base ()

do_configure (env)

Export ("env")
