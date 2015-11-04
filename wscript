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
    add_zcm_configure_options(ctx)
    add_zcm_build_options(ctx)

def add_zcm_configure_options(ctx):
    gr = ctx.add_option_group('ZCM Configuration Options')

    def add_use_option(name, desc):
        gr.add_option('--use-'+name, dest='use_'+name, default=False, action='store_true', help=desc)

    def add_trans_option(name, desc):
        gr.add_option('--use-'+name, dest='use_'+name, default=False, action='store_true', help=desc)

    add_use_option('all',  'Attempt to enable every ZCM feature')
    add_use_option('java', 'Enable java features')
    add_use_option('zmq',  'Enable ZeroMQ features')

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

    env.USING_CPP  = True
    env.USING_JAVA = hasopt('use_java') and attempt_use_java(ctx)
    env.USING_ZMQ  = hasopt('use_zmq')  and attempt_use_zmq(ctx)

    env.USING_TRANS_IPC    = hasopt('use_ipc')
    env.USING_TRANS_INPROC = hasopt('use_inproc')
    env.USING_TRANS_UDPM   = hasopt('use_udpm')
    env.USING_TRANS_SERIAL = hasopt('use_serial')

    ZMQ_REQUIRED = env.USING_TRANS_IPC or env.USING_TRANS_INPROC
    if ZMQ_REQUIRED and not env.USING_ZMQ:
        raise WafError("Using ZeroMQ is required for some of the selected transports (--use-zmq)")

    def print_entry(name, enabled):
        Logs.pprint("NORMAL", "    {:15}".format(name), sep='')
        if enabled:
            Logs.pprint("GREEN", "Enabled")
        else:
            Logs.pprint("RED", "Disabled")

    Logs.pprint('BLUE', '\nDependency Configuration:')
    print_entry("C/C++",  env.USING_CPP)
    print_entry("Java",   env.USING_JAVA)
    print_entry("ZeroMQ", env.USING_ZMQ)

    Logs.pprint('BLUE', '\nTransport Configuration:')
    print_entry("ipc",    env.USING_TRANS_IPC)
    print_entry("inproc", env.USING_TRANS_INPROC)
    print_entry("udpm",   env.USING_TRANS_UDPM)
    print_entry("serial", env.USING_TRANS_SERIAL)

    Logs.pprint('NORMAL', '')

def attempt_use_java(ctx):
    ctx.load('java')
    ctx.check_jni_headers()
    return True

def attempt_use_zmq(ctx):
    ctx.check_cfg(package='libzmq', args='--cflags --libs', uselib_store='zmq')
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
    ctx.env.CFLAGS_default   = ['-std=gnu99', '-fPIC', '-pthread'] + WARNING_FLAGS
    ctx.env.CXXFLAGS_default = ['-std=c++11', '-fPIC', '-pthread'] + WARNING_FLAGS
    ctx.env.INCLUDES_default = [ctx.path.abspath()]
    ctx.env.LINKFLAGS_default = ['-pthread']

    ctx.env.DEFINES_default = []
    for k in ctx.env.keys():
        if k.startswith('USING_'):
            if getattr(ctx.env, k):
                ctx.env.DEFINES_default.append(k)

    if ctx.env.USING_OPT:
        ctx.env.CFLAGS_default   += OPT_FLAGS
        ctx.env.CXXFLAGS_default += OPT_FLAGS
    if ctx.env.USING_SYM:
        ctx.env.CFLAGS_default   += SYM_FLAGS
        ctx.env.CXXFLAGS_default += SYM_FLAGS

    ctx.env.ENVIRONMENT_SETUP = True

def build(ctx):
    if not ctx.env.ENVIRONMENT_SETUP:
        setup_environment(ctx)

    ctx.recurse('zcm')
    ctx.recurse('config')
    ctx.recurse('gen')
    ctx.recurse('tools')

    ctx.add_group()

    ctx.recurse('test')
