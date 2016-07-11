#! /usr/bin/env python
# encoding: utf-8

import sys,optparse
import waflib
from waflib import Logs
from waflib.Errors import WafError

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

# Allow import of custom tools
sys.path.append('examples/waftools')

def options(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.load('python')
    add_zcm_configure_options(ctx)
    add_zcm_build_options(ctx)

def add_zcm_configure_options(ctx):
    gr = ctx.add_option_group('ZCM Configuration Options')

    def add_use_option(name, desc):
        gr.add_option('--use-'+name, dest='use_'+name, default=False, action='store_true', help=desc)

    def add_trans_option(name, desc):
        gr.add_option('--use-'+name, dest='use_'+name, default=False, action='store_true', help=desc)

    add_use_option('all',         'Attempt to enable every ZCM feature')
    add_use_option('java',        'Enable java features')
    add_use_option('nodejs',      'Enable nodejs features')
    add_use_option('python',      'Enable python features')
    add_use_option('zmq',         'Enable ZeroMQ features')
    add_use_option('cxxtest',     'Enable build of cxxtests')
    gr.add_option('--use-third-party', dest='use_third_party', default=False, \
                  action='store_true', help='Enable inclusion of 3rd party transports.')

    gr.add_option('--hash-member-names',  dest='hash_member_names', default='false',
                  type='choice', choices=['true', 'false'],
                  action='store', help='Include the zcmtype members names in the hash generation')
    gr.add_option('--hash-typename', dest='hash_typename', default='true',
                  type='choice', choices=['true', 'false'],
                  action='store', help='Include the zcmtype name in the hash generation')

    add_trans_option('inproc', 'Enable the In-Process transport (Requires ZeroMQ)')
    add_trans_option('ipc',    'Enable the IPC transport (Requires ZeroMQ)')
    add_trans_option('udpm',   'Enable the UDP Multicast transport (LCM-compatible)')
    add_trans_option('serial', 'Enable the Serial transport')

def add_zcm_build_options(ctx):
    gr = ctx.add_option_group('ZCM Build Options')

    gr.add_option('-s', '--symbols', dest='symbols', default=False, action='store_true',
                   help='Leave the debugging symbols in the resulting object files')
    gr.add_option('-d', '--debug', dest='debug', default=False, action='store_true',
                   help='Compile all C/C++ code in debug mode: no optimizations and full symbols')

def configure(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.recurse('gen')
    ctx.recurse('config')
    ctx.load('zcm-gen')
    process_zcm_configure_options(ctx)

def process_zcm_configure_options(ctx):
    opt = waflib.Options.options
    env = ctx.env
    def hasopt(key):
        return opt.use_all or getattr(opt, key)

    env.VERSION='1.0.0'

    env.USING_CPP         = True
    env.USING_JAVA        = hasopt('use_java') and attempt_use_java(ctx)
    env.USING_NODEJS      = hasopt('use_nodejs') and attempt_use_nodejs(ctx)
    env.USING_PYTHON      = hasopt('use_python') and attempt_use_python(ctx)
    env.USING_ZMQ         = hasopt('use_zmq') and attempt_use_zmq(ctx)
    env.USING_CXXTEST     = hasopt('use_cxxtest') and attempt_use_cxxtest(ctx)
    env.USING_THIRD_PARTY = getattr(opt, 'use_third_party') and attempt_use_third_party(ctx)

    env.USING_TRANS_IPC    = hasopt('use_ipc')
    env.USING_TRANS_INPROC = hasopt('use_inproc')
    env.USING_TRANS_UDPM   = hasopt('use_udpm')
    env.USING_TRANS_SERIAL = hasopt('use_serial')

    env.HASH_TYPENAME = getattr(opt, 'hash_typename')
    env.HASH_MEMBER_NAMES = getattr(opt, 'hash_member_names')

    ZMQ_REQUIRED = env.USING_TRANS_IPC or env.USING_TRANS_INPROC
    if ZMQ_REQUIRED and not env.USING_ZMQ:
        raise WafError("Using ZeroMQ is required for some of the selected transports (--use-zmq)")

    def print_entry(name, enabled, post="", invertColors=False):
        Logs.pprint("NORMAL", "    {:20}".format(name), sep='')
        if enabled:
            if invertColors:
                Logs.pprint("RED", "Enabled", sep='')
            else:
                Logs.pprint("GREEN", "Enabled", sep='')
        else:
            if invertColors:
                Logs.pprint("GREEN", "Disabled", sep='')
            else:
                Logs.pprint("RED", "Disabled", sep='')
        Logs.pprint("BLUE", " " + post)

    Logs.pprint('BLUE',     '\nDependency Configuration:')
    print_entry("C/C++",       env.USING_CPP)
    print_entry("Java",        env.USING_JAVA)
    print_entry("NodeJs",      env.USING_NODEJS)
    print_entry("Python",  env.USING_PYTHON)
    print_entry("ZeroMQ",      env.USING_ZMQ)
    print_entry("CxxTest",     env.USING_CXXTEST)
    if not env.USING_THIRD_PARTY and opt.use_all:
        print_entry("Third Party", env.USING_THIRD_PARTY, "Not included in --use-all")
    else:
        print_entry("Third Party", env.USING_THIRD_PARTY)

    Logs.pprint('BLUE', '\nTransport Configuration:')
    print_entry("ipc",    env.USING_TRANS_IPC)
    print_entry("inproc", env.USING_TRANS_INPROC)
    print_entry("udpm",   env.USING_TRANS_UDPM)
    print_entry("serial", env.USING_TRANS_SERIAL)

    Logs.pprint('BLUE', '\nType Configuration:')
    print_entry("hash-typename", env.HASH_TYPENAME == 'true')
    print_entry("hash-member-names",  env.HASH_MEMBER_NAMES == 'true', '', True)

    Logs.pprint('NORMAL', '')

def attempt_use_java(ctx):
    ctx.load('java')
    ctx.check_jni_headers()
    return True

def attempt_use_nodejs(ctx):
    # nodejs isn't really required for build, but it felt weird to leave it
    # out since the user is expecting zcm to build for nodejs. It will
    # technically build, but you wont be able to run it without the nodejs package
    ctx.find_program('nodejs', var='NODEJS',  mandatory=True)
    ctx.find_program('npm',    var='NPM',     mandatory=True)
    return True

def attempt_use_python(ctx):
    ctx.load('python')
    ctx.check_python_headers()
    ctx.find_program('cython', var='CYTHON', mandatory=True)
    return True

def attempt_use_zmq(ctx):
    ctx.check_cfg(package='libzmq', args='--cflags --libs', uselib_store='zmq')
    return True

def attempt_use_cxxtest(ctx):
    ctx.load('cxxtest')
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

def process_zcm_build_options(ctx):
    opt = waflib.Options.options
    ctx.env.USING_OPT = not opt.debug
    ctx.env.USING_SYM = opt.debug or opt.symbols

def setup_environment(ctx):
    ctx.post_mode = waflib.Build.POST_LAZY
    process_zcm_build_options(ctx)

    WARNING_FLAGS = ['-Wall', '-Werror', '-Wno-unused-function', '-Wno-format-zero-length']
    SYM_FLAGS = ['-g']
    OPT_FLAGS = ['-O3']
    ctx.env.CFLAGS_default    = ['-std=gnu99', '-fPIC', '-pthread'] + WARNING_FLAGS
    ctx.env.CXXFLAGS_default  = ['-std=c++11', '-fPIC', '-pthread'] + WARNING_FLAGS
    ctx.env.INCLUDES_default  = [ctx.path.abspath()]
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

    if ctx.env.USING_OPT:
        ctx.env.CFLAGS_default   += OPT_FLAGS
        ctx.env.CXXFLAGS_default += OPT_FLAGS
    if ctx.env.USING_SYM:
        ctx.env.CFLAGS_default   += SYM_FLAGS
        ctx.env.CXXFLAGS_default += SYM_FLAGS

    ## Run special compiler-specific configuration
    if ctx.env.USING_CXXTEST:
        ctx.setup_cxxtest()

    ctx.env.ENVIRONMENT_SETUP = True

def generate_signature(ctx):
    rootpath = ctx.path.get_src().abspath()
    bldpath = ctx.path.get_bld().abspath()
    ### XXX this rule for generating .gitid is nasty, refactor...
    ###     .gitid should be pulled into a reusable tool like zcm-gen
    ctx(rule = 'cd %s && (git rev-parse HEAD && ((git tag --contains ; \
               echo "<no-tag>") | head -n1) && git diff) > %s/${TGT} \
               2>/dev/null' % (rootpath, bldpath),
        name = 'zcmgitid',
        target = 'zcm.gitid',
        always = True,
        on_results = True)

def build(ctx):
    if not ctx.env.ENVIRONMENT_SETUP:
        setup_environment(ctx)

    ctx.recurse('zcm')
    ctx.recurse('config')
    ctx.recurse('gen')
    ctx.recurse('tools')
    generate_signature(ctx)

    ctx.add_group()

    ctx.recurse('test')
