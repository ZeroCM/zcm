#!/usr/bin/env python
import os
import re
import copy
from waflib import Task, Utils, Context, Logs
from waflib import TaskGen
from waflib.Errors import WafError
from waflib.Utils import subprocess
from waflib.TaskGen import extension, feature
from waflib.Configure import conf

def configure(ctx):
    ctx.find_program('cxxtestgen', var='CXXTESTGEN', mandatory=True)
    ctx.env.CXXTESTGEN = ctx.env.CXXTESTGEN[0]

    ctx.env.INCLUDES_cxxtest = os.path.dirname(os.path.dirname(ctx.env.CXXTESTGEN))

@conf
def setup_cxxtest(ctx):
    ctx.env.CXXTESTFLAGS_suite   = ['--error-printer', '--part']
    ctx.env.CXXTESTFLAGS_runner  = ['--error-printer', '--root']

@conf
def check_cxxtest_version(ctx):
    reg = re.compile(r'This is CxxTest version\s(.*)',re.M)
    out = ctx.cmd_and_log(ctx.env['CXXTESTGEN']+' --version')
    ver_s = reg.findall(out)[0].split('.')
    ver_i = tuple([int(s) for s in ver_s[0:2]])

    msg='Checking for cxxtest version'
    ctx.msg(msg,'.'.join(map(str,ver_i)))

    return ver_i

@conf
def cxxtest(ctx, **kw):
    # Check if the 'test' directory exists and if there are any tests in it
    if (ctx.path.find_dir('test') is None):
        return

    if 'SRCPATH' not in ctx.env:
        raise WafError('ctx.env requires : "SRCPATH"')
    relpath = ctx.path.path_from(ctx.path.find_or_declare(ctx.env.SRCPATH))

    suites = ctx.path.ant_glob('test/**/*Test.hpp')
    if (len(suites) == 0):
        return

    cxxtestgen = ctx.root.find_or_declare(ctx.env.CXXTESTGEN)
    cxxtestgen_src = ctx.root.find_or_declare(os.path.join(os.path.dirname(ctx.env.CXXTESTGEN), '../python/python3/cxxtest/cxxtestgen.py'))

    # generate runner src
    runnerTg = ctx(rule      = cxxtest_generate_runner,
                   target    = 'test/runner.cpp',
                   name      = relpath + '/test/runner.cpp',
                   shell     = False,
                   reentrant = False)
    runnerTg.post()

    ctx.add_manual_dependency('test/runner.cpp', cxxtestgen)
    ctx.add_manual_dependency('test/runner.cpp', cxxtestgen_src)

    # generate suite src
    tg = ctx(source = suites)
    tg.post()

    for s in tg.source:
        ctx.add_manual_dependency(s, cxxtestgen)
        ctx.add_manual_dependency(s, cxxtestgen_src)

    # compile list of all src
    cpp_src = [ t.outputs[0] for t in tg.tasks ]
    cpp_src += [ runnerTg.tasks[0].outputs[0] ]

    # compile test program
    if hasattr(kw, 'target'):
        del kw['target']
    if hasattr(kw, 'includes'):
        del kw['includes']
    if hasattr(kw, 'source'):
        del kw['source']

    kw['use'] += ['cxxtest']

    ctx.program(target   = 'test/runner',
                name     = relpath + '/test/runner',
                includes = '.',
                source   = cpp_src,
                install_path = None,
                **kw)

def cxxtest_generate_suite(tsk):
    pathparts = tsk.inputs[0].abspath().split('/')
    testIdx = (pathparts[::-1].index('test') + 1) * -1
    nameparts = '/'.join(pathparts[testIdx:])
    # adding the --include flag with the relative path to the test suite is
    # required for waf to properly generate the dependency tree, otherwise it
    # misses the dependency on the hpp file
    cmd = '%s %s -o %s %s --include=%s' % (tsk.env['CXXTESTGEN'],
                                           ' '.join(tsk.env['CXXTESTFLAGS_suite']),
                                           tsk.outputs[0].abspath(),
                                           tsk.inputs[0].bldpath(),
                                           nameparts)
    tsk.exec_command(cmd)

def cxxtest_generate_runner(tsk):
    tsk.exec_command('%s %s -o %s' % (tsk.env['CXXTESTGEN'],
                                      ' '.join(tsk.env['CXXTESTFLAGS_runner']),
                                      tsk.outputs[0].abspath()))

# Create a task generator that can make test suites
TaskGen.declare_chain(
    name      = 'cxxtest-gen',
    rule      = cxxtest_generate_suite,
    shell     = False,
    reentrant = False,
    ext_out   = 'Test.cpp',
    ext_in    = 'Test.hpp')
