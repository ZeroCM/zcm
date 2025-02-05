#! /usr/bin/env python
# encoding: utf-8

import sys,optparse
import waflib
from waflib import Logs
from waflib.Errors import WafError
import os
import os.path
import subprocess
import re

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

# Allow import of custom tools
sys.path.append('waftools')

variants = [         'asan',  ## Core Sanitizers (Address, Undefined-Behavior)
                     'tsan',  ## Thread Sanitizer
                 'examples',  ## Build zcm examples
            'examples_asan',  ## Build zcm examples in asan
            'examples_tsan',  ## Build zcm examples in tsan
                    'tests',  ## Build zcm tests
               'tests_asan',  ## Build zcm tests in asan
               'tests_tsan',  ## Build zcm tests in tsan
            ]

def options(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.load('python')
    add_zcm_configure_options(ctx)
    add_zcm_build_options(ctx)

    gr = ctx.add_option_group('Verification Options')
    gr.add_option('--match-version', dest = 'version_match', action = 'store',
                  help = 'Ensure ZCM version matches this number')

def add_zcm_configure_options(ctx):
    gr = ctx.add_option_group('ZCM Configuration Options')

    def add_use_option(name, desc):
        gr.add_option('--use-' + name, dest = 'use_' + re.sub('-', '_', name),
                      default = False, action = 'store_true', help = desc)

    def add_trans_option(name, desc):
        gr.add_option('--use-' + name, dest = 'use_' + re.sub('-', '_', name),
                      default = False, action = 'store_true', help = desc)

    add_use_option('all',         'Attempt to enable every ZCM feature')
    add_use_option('java',        'Enable java features')
    add_use_option('nodejs',      'Enable nodejs features')
    add_use_option('python',      'Enable python features')
    add_use_option('julia',       'Enable julia features')
    add_use_option('zmq',         'Enable ZeroMQ features')
    add_use_option('elf',         'Enable runtime loading of shared libs')
    add_use_option('gtk',         'Enable tools made with gtk')
    add_use_option('third-party', 'Enable inclusion of 3rd party transports.')

    gr.add_option('--hash-member-names',  dest='hash_member_names', default='false',
                  type='choice', choices=['true', 'false'],
                  action='store', help='Include the zcmtype members names in the hash generation')
    gr.add_option('--hash-typename', dest='hash_typename', default='true',
                  type='choice', choices=['true', 'false'],
                  action='store', help='Include the zcmtype name in the hash generation')

    add_use_option('dev',         'Enable all dev tools')
    add_use_option('build-cache', 'Enable the build cache for faster rebuilds')
    add_use_option('clang',       'Enable build using clang sanitizers')
    add_use_option('cxxtest',     'Enable build of cxxtests')

    gr.add_option('--track-traffic-topology',
                  dest='track_traffic_topology', default='false',
                  type='choice', choices=['true', 'false'], action='store',
                  help='Track channels published and subscribed for every zcm instance')

    add_trans_option('inproc', 'Enable the In-Process transport (Requires ZeroMQ)')
    add_trans_option('ipc',    'Enable the IPC transport (Requires ZeroMQ)')
    add_trans_option('udp',    'Enable the UDP transports (unicast and multicast)')
    add_trans_option('serial', 'Enable the Serial transport')
    add_trans_option('can',    'Enable the Canbus transport')
    add_trans_option('ipcshm', 'Enable the IPC Shared-Memory transport')

def add_zcm_build_options(ctx):
    gr = ctx.add_option_group('ZCM Build Options')

    gr.add_option('-s', '--symbols', dest='symbols', default=False, action='store_true',
                   help='Leave the debugging symbols in the resulting object files')
    gr.add_option('-d', '--debug', dest='debug', default=False, action='store_true',
                   help='Compile all C/C++ code in debug mode: no optimizations and full symbols')
    gr.add_option('--march', default='native',
                  help='The machine architecture for which zcm should be compiled. The default (and '
                  'most appropriate option in general) is "native". Please see the help text for '
                  'gcc to see the valid values for this option. Set to the empty string to not pass '
                  'the argument at all to the compiler (which results in it using the default)')
    gr.add_option('--mtune', default='native',
                  help='The name of the target processor for which zcm should tune the performance '
                  'of the code. The default (and most appropriate option in general) is "native". '
                  'Please see the help text for gcc to see the valid values for this option. '
                  'Set to the empty string to not pass the argument at all to the compiler '
                  '(which results in it using the default)')
    ctx.add_option('-a', '--no-outline-atomics', dest='no_outline_atomics', default=False, action='store_true',
                   help='Disable calls to out-of-line helpers to implement atomic operations')
    ctx.add_option('-g', '--skip-signatures', dest='skip_git', default=False, action='store_true',
                   help='Skip building the git signature')

def configure(ctx):
    for e in variants:
        ctx.setenv(e) # start with a copy instead of a new env

    ctx.setenv('')

    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.recurse('config')

    ctx.env.variantsEnabledByConfigure = ['examples', 'tests']

    process_zcm_configure_options(ctx)

def processCppVersion(ctx, f):
    version = ctx.cmd_and_log('grep VERSION %s | cut -d \' \' -f3' % (f),
                              output=waflib.Context.STDOUT,
                              quiet=waflib.Context.BOTH).strip()
    version = version.split("\n")
    version = '.'.join(version)
    return version

def processNodeVersion(ctx, f):
    version = ctx.cmd_and_log('grep version %s | cut -d \'"\' -f4' % (f),
                              output=waflib.Context.STDOUT,
                              quiet=waflib.Context.BOTH).strip()
    return version

def version(ctx):
    versionNODE_EX = processNodeVersion(ctx, 'examples/node-client/package.json')
    versionNODE    = processNodeVersion(ctx, 'zcm/js/node/package.json')
    versionZCM     = processCppVersion(ctx, 'zcm/zcm.h')
    versionGEN     = processCppVersion(ctx, 'gen/version.h')

    if versionZCM != versionGEN:
        raise WafError("Version mismatch between core and zcm gen")

    if versionZCM != versionNODE:
        raise WafError("Version mismatch between core and nodejs")

    if versionZCM != versionNODE_EX:
        raise WafError("Version mismatch between core and nodejs")

    versionMatch = getattr(waflib.Options.options, "version_match")
    if versionMatch:
        versionMatch = versionMatch.lstrip("v")
        if versionZCM != versionMatch:
            raise WafError("Version does not match: %s != %s" % (versionMatch, versionZCM))

    Logs.pprint('RED','ZCM Version: %s' % (versionZCM))

    return versionZCM

def process_zcm_configure_options(ctx):
    opt = waflib.Options.options
    env = ctx.env
    def hasopt(key):
        return opt.use_all or getattr(opt, key)
    def hasoptDev(key):
        return opt.use_dev or getattr(opt, key)

    env.USING_CPP         = True
    env.USING_JAVA        = hasopt('use_java') and attempt_use_java(ctx)
    env.USING_NODEJS      = hasopt('use_nodejs') and attempt_use_nodejs(ctx)
    env.USING_PYTHON      = hasopt('use_python') and attempt_use_python(ctx)
    env.USING_JULIA       = hasopt('use_julia') and attempt_use_julia(ctx)
    env.USING_ZMQ         = hasopt('use_zmq') and attempt_use_zmq(ctx)
    env.USING_ELF         = hasopt('use_elf') and attempt_use_elf(ctx)
    env.USING_GTK3        = hasopt('use_gtk') and attempt_use_gtk(ctx)
    env.USING_THIRD_PARTY = getattr(opt, 'use_third_party') and attempt_use_third_party(ctx)

    env.USING_TRANS_IPC    = hasopt('use_ipc')
    env.USING_TRANS_INPROC = hasopt('use_inproc')
    env.USING_TRANS_UDP    = hasopt('use_udp')
    env.USING_TRANS_SERIAL = hasopt('use_serial')
    env.USING_TRANS_CAN    = hasopt('use_can')
    env.USING_TRANS_IPCSHM = hasopt('use_ipcshm')

    env.HASH_TYPENAME      = getattr(opt, 'hash_typename')
    env.HASH_MEMBER_NAMES  = getattr(opt, 'hash_member_names')

    env.USING_CACHE            = hasoptDev('use_build_cache') and attempt_use_cache(ctx)
    env.USING_CLANG            = hasoptDev('use_clang') and attempt_use_clang(ctx)
    env.USING_CXXTEST          = hasoptDev('use_cxxtest') and attempt_use_cxxtest(ctx)
    env.TRACK_TRAFFIC_TOPOLOGY = getattr(opt, 'track_traffic_topology')

    ZMQ_REQUIRED = env.USING_TRANS_IPC or env.USING_TRANS_INPROC
    if ZMQ_REQUIRED and not env.USING_ZMQ:
        raise WafError("Using ZeroMQ is required for some of the selected transports (--use-zmq)")

    env.VERSION = version(ctx)

    for e in variants:
        ctx.setenv(e, env=ctx.env.derive()) # start with a copy instead of a new env

    def print_entry(name, enabled, invertColors=False):
        Logs.pprint("NORMAL", "    {:30}".format(name), sep='')
        if enabled:
            if invertColors:
                Logs.pprint("RED", "Enabled")
            else:
                Logs.pprint("GREEN", "Enabled")
        else:
            if invertColors:
                Logs.pprint("GREEN", "Disabled")
            else:
                Logs.pprint("RED", "Disabled")

    Logs.pprint('BLUE',     '\nDependency Configuration:')
    print_entry("C/C++",       env.USING_CPP)
    print_entry("Java",        env.USING_JAVA)
    print_entry("NodeJs",      env.USING_NODEJS)
    print_entry("Python",      env.USING_PYTHON)
    print_entry("Julia",       env.USING_JULIA)
    print_entry("ZeroMQ",      env.USING_ZMQ)
    print_entry("Elf",         env.USING_ELF)
    print_entry("GTK",         env.USING_GTK3)
    print_entry("Third Party", env.USING_THIRD_PARTY)

    Logs.pprint('BLUE', '\nTransport Configuration:')
    print_entry("ipc",    env.USING_TRANS_IPC)
    print_entry("inproc", env.USING_TRANS_INPROC)
    print_entry("udp",    env.USING_TRANS_UDP)
    print_entry("serial", env.USING_TRANS_SERIAL)
    print_entry("can",    env.USING_TRANS_CAN)
    print_entry("ipcshm", env.USING_TRANS_IPCSHM)

    Logs.pprint('BLUE', '\nType Configuration:')
    print_entry("hash-typename",     env.HASH_TYPENAME == 'true')
    print_entry("hash-member-names", env.HASH_MEMBER_NAMES == 'true', True)

    Logs.pprint('BLUE', '\nDev Configuration:')
    print_entry("Build Cache",             env.USING_CACHE)
    print_entry("Clang",                   env.USING_CLANG)
    print_entry("CxxTest",                 env.USING_CXXTEST)
    print_entry("Record Traffic Topology", env.TRACK_TRAFFIC_TOPOLOGY == 'true')

    Logs.pprint('NORMAL', '')

def attempt_use_java(ctx):
    ctx.load('java')
    ctx.check_jni_headers()
    return True

def attempt_use_nodejs(ctx):
    # nodejs isn't really required for build, but it felt weird to leave it
    # out since the user is expecting zcm to build for nodejs. It will
    # technically build, but you wont be able to run it without the nodejs package
    ctx.find_program('node', var='NODE', mandatory=True)
    ctx.env.NODE = ctx.env.NODE[0]
    ctx.find_program('npm', var='NPM', mandatory=True)
    ctx.env.NPM = ctx.env.NPM[0]
    return True

def attempt_use_python(ctx):
    ctx.load('python')
    ctx.check_python_headers()
    ctx.find_program('cython', var='CYTHON', mandatory=True)
    return True

def attempt_use_julia(ctx):
    ctx.find_program('julia', var='julia', mandatory=True)
    ctx.env.julia = ctx.env.julia[0]

    try:
        version = subprocess.check_output('%s --version | cut -d \' \' -f3' %
                                          ctx.env.julia, shell=True, stderr=open(os.devnull,'wb'))
        version = str(version.decode('utf-8')).strip()

        Logs.pprint('NORMAL', '{:41}:'.format('Julia version identified as'), sep='')
        Logs.pprint('GREEN', '%s' % version)

        versionArr = version.split('.')
        major = int(versionArr[0])
        minor = int(versionArr[1])

        if version != '0.6.4' and (major != 1 or minor < 6):
            raise WafError('Wrong Julia version, requires 0.6.4 or >= 1.6\nfound %s' % version)

        # Note: because of how the include structure **internal** to the julia 1.0 uv headers
        #       works, you actually **have** to point the include directory at
        #       JULIA_HOME/include/julia instead of just JULIA_HOME/include
        if version == '0.6.4':
            res = subprocess.check_output('%s -E "abspath(JULIA_HOME, Base.INCLUDEDIR, %s)"' %
                                          (ctx.env.julia, '\\\"julia\\\"'),
                                          shell=True, stderr=open(os.devnull,'wb'))
        else:
            if version != '1.6.0':
                Logs.pprint('RED', 'Using a version of Julia > 1.6.0 is not fully tested')

            res = subprocess.check_output('%s -E "abspath(Sys.BINDIR, Base.INCLUDEDIR, %s)"' %
                                          (ctx.env.julia, '\\\"julia\\\"'),
                                          shell=True, stderr=open(os.devnull,'wb'))

        ctx.env.INCLUDES_julia = str(res.decode('utf-8')).strip().strip('"')
        Logs.pprint('NORMAL', '{:41}:'.format('Julia include path identified as'), sep='')
        Logs.pprint('GREEN', '%s' % ctx.env.INCLUDES_julia)
    except subprocess.CalledProcessError as e:
        raise WafError('Failed to find julia include path\n%s' % e)

    return True

def attempt_use_zmq(ctx):
    ctx.check_cfg(package='libzmq', args='--cflags --libs', uselib_store='zmq')
    return True

def attempt_use_cxxtest(ctx):
    ctx.recurse('deps/cxxtest')
    return True

def attempt_use_elf(ctx):
    if not os.path.exists('/usr/include/libelf.h'):
        raise WafError('Failed to find libelf')
    ctx.env.LIB_elf = ['elf', 'dl']
    return True

def attempt_use_gtk(ctx):
    ctx.check_cfg(package='gtk+-3.0', args='--cflags --libs', uselib_store='gtk+3')
    return True

def attempt_use_third_party(ctx):
    submodules = [ 'zcm/transport/third-party' ]
    foundAll = True
    for name in submodules:
        found = not ctx.path.find_dir(name) is None
        ctx.msg('Checking for submodule: %s' % name, found)
        foundAll &= found
    if not foundAll:
        raise WafError('Failed to find all required submodules. You should run: \n' + \
                       'git submodule update --init --recursive\n' + \
                       'and then reconfigure')
    return True

def attempt_use_cache(ctx):
    os.environ['WAFCACHE'] = os.environ['HOME'] + '/.cache/wafcache'
    ctx.load('wafcache')
    return True

def attempt_use_clang(ctx):
    ctx.load('clang-custom')
    ctx.env.CLANG_VERSION = ctx.assert_clang_version(3.6)
    ctx.env.variantsEnabledByConfigure.extend(['asan', 'examples_asan', 'tests_asan'])
    ctx.env.variantsEnabledByConfigure.extend(['tsan', 'examples_tsan', 'tests_tsan'])
    return True

def process_zcm_build_options(ctx):
    opt = waflib.Options.options
    ctx.env.USING_OPT = not opt.debug
    ctx.env.USING_SYM = opt.debug or opt.symbols
    ctx.env.MARCH = opt.march
    ctx.env.MTUNE = opt.mtune
    ctx.env.NO_OUTLINE_ATOMICS = opt.no_outline_atomics
    if ctx.env.USING_CACHE:
        attempt_use_cache(ctx)
    if not ctx.env.USING_SYM:
        ctx.load('strip_on_install')

def setup_environment_gnu(ctx):
    FLAGS = ['-Wno-unused-local-typedefs',
            ]
    ctx.env.CFLAGS_default   += FLAGS
    ctx.env.CXXFLAGS_default += FLAGS

def setup_environment_asan(ctx):
    ctx.set_clang_compiler()

    FLAGS = ['-fcolor-diagnostics',
             '-fsanitize=address',    # AddressSanitizer, a memory error detector.
             '-fsanitize=integer',    # Enables checks for undefined or suspicious integer behavior.
             '-fsanitize=undefined',  # Fast and compatible undefined behavior checker.
             '-Wno-deprecated-declarations', # Bug in cython/clang
    ]

    ctx.env.CFLAGS_default    += FLAGS
    ctx.env.CXXFLAGS_default  += FLAGS
    ctx.env.LINKFLAGS_default += FLAGS

def setup_environment_tsan(ctx):
    ctx.set_clang_compiler()

    FLAGS = ['-fcolor-diagnostics',
             '-fsanitize=thread',     # ThreadSanitizer, a data race detector.
             '-Wno-deprecated-declarations', # Bug in cython/clang
    ]

    ctx.env.CFLAGS_default    += FLAGS
    ctx.env.CXXFLAGS_default  += FLAGS
    ctx.env.LINKFLAGS_default += FLAGS


def setup_environment(ctx):
    ctx.post_mode = waflib.Build.POST_LAZY
    process_zcm_build_options(ctx)

    ctx.env.SRCPATH = ctx.path.get_src().abspath()

    WARNING_FLAGS = ['-Wall', '-Werror', '-Wno-unused-function']
    SYM_FLAGS = ['-g']
    OPT_FLAGS = ['-O3']
    ctx.env.CFLAGS_default    = ['-std=gnu99', '-fPIC', '-pthread'] + WARNING_FLAGS
    ctx.env.CXXFLAGS_default  = ['-std=c++11', '-fPIC', '-pthread'] + WARNING_FLAGS
    if ctx.env.MARCH:
        ctx.env.CFLAGS_default.append('-march=%s' % ctx.env.MARCH)
        ctx.env.CXXFLAGS_default.append('-march=%s' % ctx.env.MARCH)
    if ctx.env.MTUNE:
        ctx.env.CFLAGS_default.append('-mtune=%s' % ctx.env.MTUNE)
        ctx.env.CXXFLAGS_default.append('-mtune=%s' % ctx.env.MTUNE)
    if ctx.env.NO_OUTLINE_ATOMICS:
        ctx.env.CFLAGS_default.append('-mno-outline-atomics ')
        ctx.env.CXXFLAGS_default.append('-mno-outline-atomics ')

    ctx.env.INCLUDES_default  = [ctx.path.abspath()]
    ctx.env.LIB_default       = ['rt']
    ctx.env.LINKFLAGS_default = ['-pthread']

    ctx.env.DEFINES_default   = ['_LARGEFILE_SOURCE', '_FILE_OFFSET_BITS=64']
    for k in ctx.env.keys():
        if k.startswith('USING_'):
            if getattr(ctx.env, k):
                ctx.env.DEFINES_default.append(k)

    if ctx.env.HASH_TYPENAME == 'true':
        ctx.env.DEFINES_default.append("ENABLE_TYPENAME_HASHING")
    if ctx.env.HASH_MEMBER_NAMES == 'true':
        ctx.env.DEFINES_default.append("ENABLE_MEMBERNAME_HASHING")

    if ctx.env.TRACK_TRAFFIC_TOPOLOGY == 'true':
        ctx.env.DEFINES_default.append("TRACK_TRAFFIC_TOPOLOGY")

    if ctx.env.USING_OPT:
        ctx.env.CFLAGS_default   += OPT_FLAGS
        ctx.env.CXXFLAGS_default += OPT_FLAGS
    if ctx.env.USING_SYM:
        ctx.env.CFLAGS_default   += SYM_FLAGS
        ctx.env.CXXFLAGS_default += SYM_FLAGS

    ## Building for asan?
    if ctx.variant.endswith('asan'):
        setup_environment_asan(ctx)
    ## Building for tsan?
    elif ctx.variant.endswith('tsan'):
        setup_environment_tsan(ctx)
    else:
        setup_environment_gnu(ctx)

    ctx.env.ENVIRONMENT_SETUP = True

def generate_signature(ctx):
    rootpath = ctx.path.get_src().abspath()
    bldpath = ctx.path.get_bld().abspath()
    ### TODO: this rule for generating .gitid is nasty, refactor...
    ###       .gitid should be pulled into a reusable tool like zcm-gen
    ctx(rule   = 'cd %s && ((git rev-parse HEAD && ((git tag --contains ; \
                  echo "<no-tag>") | head -n1) && git diff) || \
                  echo "Not built from git" > %s/${TGT}) \
                  > %s/${TGT} 2>/dev/null' % (rootpath, bldpath, bldpath),
        name   = 'zcmgitid',
        target = 'zcm.gitid',
        always = True)

    ctx.install_files('${PREFIX}/lib/', ['zcm.gitid'])

from waflib.Build import BuildContext, CleanContext, ListContext, InstallContext, UninstallContext
for x in variants:
    contexts = [BuildContext, CleanContext, ListContext]
    if not x.startswith('examples') and not x.startswith('tests'):
        contexts.extend([InstallContext, UninstallContext])
    for y in contexts:
        name = y.__name__.replace('Context','').lower()
        class tmp(y):
            cmd = name + '_' + x
            variant = x

def build(ctx):
    if ctx.variant and not ctx.variant in ctx.env.variantsEnabledByConfigure:
        ctx.fatal('Please configure for %s build' % (ctx.variant))

    if not ctx.env.ENVIRONMENT_SETUP:
        setup_environment(ctx)

    if ctx.variant.startswith('examples'):
        ctx.recurse('examples')
    elif ctx.variant.startswith('tests'):
        ctx.recurse('tools/cpp/util')
        ctx.recurse('test')
        if ctx.env.USING_CXXTEST:
            ctx.cxxtest(use = ['zcm', 'zcm_tools_util',
                               'testzcmtypes', 'testzcmtypes_cpp', 'testzcmtypes_c_stlib',
                               'multifile_lib'])
    else:
        ctx.recurse('scripts')
        ctx.recurse('zcm')
        ctx.recurse('config')
        ctx.recurse('gen')
        ctx.recurse('tools')
        ctx.recurse('DEBIAN')
        #ctx.recurse('bench')
        ctx.install_as('${PREFIX}/share/doc/zcm/copyright', 'LICENSE')
        if not waflib.Options.options.skip_git:
            generate_signature(ctx)
        else:
            Logs.pprint('RED', '\nSkipping git signatures\n')


def distclean(ctx):
    ctx.exec_command('rm -f waftools/*.pyc')
    waflib.Scripting.distclean(ctx)

def cache_invalidate(ctx):
    ret = os.system('rm -rf ~/.cache/wafcache_`whoami`')
    if ret != 0:
        raise WafError('Failed to invalidate cache')
