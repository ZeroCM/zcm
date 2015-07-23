#! /usr/bin/env python
# encoding: utf-8

import waflib
from waflib.Context import Context
import waflib.Options

# these variables are mandatory ('/' are converted automatically)
top = '.'
out = 'build'

def options(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.add_option('-s', '--symbols', dest='symbols', default=False, action='store_true',
                   help='Leave the debugging symbols in the resulting object files (auto-enabled for --debug')
    ctx.add_option('-d', '--debug', dest='debug', default=False, action='store_true',
                   help='Compile all C/C++ code in debug mode: no opts and full symbols')

def configure(ctx):
    ctx.load('compiler_c')
    ctx.load('compiler_cxx')
    ctx.load('java')
    ctx.check_jni_headers()
    ctx.check_cfg(package='libzmq', args='--cflags --libs', uselib_store='zmq')

def setup_environment(ctx):
    useOptimize = not waflib.Options.options.debug
    useSymbols = waflib.Options.options.debug or waflib.Options.options.symbols

    WARNING_FLAGS = ['-Wall', '-Werror', '-Wno-unused-function', '-Wno-format-zero-length']
    SYM_FLAGS = ['-g']
    OPT_FLAGS = ['-O3']
    ctx.env.CFLAGS_default   = ['-std=gnu99', '-fPIC'] + WARNING_FLAGS
    ctx.env.CXXFLAGS_default = ['-std=c++11', '-fPIC'] + WARNING_FLAGS
    if useOptimize:
        ctx.env.CFLAGS_default   += OPT_FLAGS
        ctx.env.CXXFLAGS_default += OPT_FLAGS
    if useSymbols:
        ctx.env.CFLAGS_default   += SYM_FLAGS
        ctx.env.CXXFLAGS_default += SYM_FLAGS

    ctx.env.INCLUDES_default = [ctx.path.abspath()+'/zcm-c']

def build(ctx):
    setup_environment(ctx)
    ctx.recurse('zcm-gen')
    ctx.recurse('zcm-c')
    ctx.recurse('zcm-lspy')
    ctx.recurse('zcm-java')
    ctx.recurse('zcm-js')
