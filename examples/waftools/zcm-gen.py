#!/usr/bin/env python
# encoding: utf-8

import os
from waflib import Task
from waflib.Errors import WafError
from waflib.TaskGen import extension
from waflib.Configure import conf

def configure(ctx):
    # Note: This tool will also work if zcm-gen is a tool built by this build if:
    #       configuration:
    #           ctx.env.ZCMGEN is set to the programs path prior to loading the zcm-gen tool
    #       build:
    #           all zcmtype generation happens after the tool is compiled.
    #           That is, use ctx.add_group() while ctx.post_mode = waflib.Build.POST_LAZY
    #           to separate zcm-gen compilation and zcmtype generation.
    #
    # TODO: could improve this to actually use the waf node of the zcm-gen binary as in the
    #       ex. of building a compiler in the waf book (https://waf.io/book/#_advanced_scenarios)
    # XXX:  I believe because we are not doing the above, changes to the zcm-gen binary do not
    #       force the regeneration of the zcmtypes. This is generally not a huge problem because
    #       zcm-gen stays pretty static, but it is technically a build error.
    # TODO: The user MUST have uselib variables for zcm and zcmjar defined through their own
    #       configuration. We should add them here or move them into a different waf module

    # find program leaves the program itself in an array (probably for if there are multiple),
    # but we only expect / want one
    if not getattr(ctx.env, 'ZCMGEN', []):
        ctx.find_program('zcm-gen', var='ZCMGEN', mandatory=True)
        ctx.env.ZCMGEN = ctx.env.ZCMGEN[0]

# Context method used to set up proper include paths and simplify user code for zcmtype builds
# keywords:
#   name:         identifier for uselib linking (e.g. c/c++ builds may simply add 'name'
#                 to their 'use' list to get proper include path and library linking)
#                 default = 'zcmtypes' (though it is encouraged to name it something more unique
#                                       to avoid library naming conflicts)
#   source:       list of zcmtype source files to be interpreted
#   lang:         list of languages for which zcmtypes should be generated, options are:
#                 ['c', 'cpp', 'java', 'python', 'nodejs']
#                 TODO: add python and nodejs support
#   javapkg:      name of the java package
#                 default = 'zcmtypes' (though it is encouraged to name it something more unique
#                                       to avoid library naming conflicts)
#
#   TODO: add support for changing include directory
#
# Using the output zcmtypes:
#   For the following explanations, assume:
#     ${name} = value entered for the "name" keyword,
#     ${dir}  = dir containing the zcmtypes (eg for ~/zcm/examples/types/*.zcm, ${dir} = "types")
#     ${pkg}  = value entered for the "javapkg" keyword
#
#   C:            types will be compiled into a static library and linked into targets
#     wscript:    add '${name}_c' to the list of "use" dependencies
#     c files:    target include directives at "${dir}/type_name_t.h"
#
#   C++:          types are header only and will be included directly in source
#     wscript:    add '${name}_cpp' to the list of "use" dependencies
#     cpp files:  target include directives at "${dir}/type_name_t.hpp"
#
#   Java:         types will be compiled into a .jar, see details below on execution
#     wscript:    add '${name}_java' to the list of "use" dependencies
#     java files: target import directives at "$pkg/type_name_t"
#
#   TODO: Python and NodeJS
#
# Note on running the output java classes:
#   Because of the way that java's CLASSPATH works, even though waf will link the appropriate jar
#   files for the build, their location is not maintained in the output jar. This means that to
#   run a java program compiled in this manner, the final output jar AND the zcmtype jar MUST be
#   in the CLASSPATH. Indeed, this waf build system will even be able to find the primary zcm.jar
#   if it is not in the CLASSPATH and will be able to build without issue, but any attempt to run
#   the output java classes without having zcm.jar in the CLASSPATH will fail. The zcmtype jar
#   will be called ${name}.jar and will be located in the mirrored build path to wherever the
#   waf zcmgen tool was invoked.
@conf
def zcmgen(ctx, **kw):
    # TODO: should raise an error if ctx.env.ZCMGEN is not set
    uselib_name = 'zcmtypes'
    if 'name' in kw:
        uselib_name = kw['name']

    javapkg_name = 'zcmtypes'
    if 'javapkg' in kw:
        javapkg_name = kw['javapkg']

    if 'lang' not in kw:
        # TODO: this should probably be a more specific error type
        raise WafError('zcmgen requires keword argument: "lang"')

    # Add .zcm files to build so the process_zcmtypes rule picks them up
    genfiles_name = uselib_name + '_genfiles'
    tg = ctx(name     = genfiles_name,
             source   = kw['source'],
             lang     = kw['lang'],
             javapkg  = javapkg_name)
    tg.post()

    bld = ctx.path.get_bld().abspath()
    inc = os.path.dirname(bld)

    if 'c_stlib' in kw['lang']:
        cstlibtg = ctx.stlib(name            = uselib_name + '_c_stlib',
                             target          = uselib_name,
                             use             = ['default', 'zcm'],
                             includes        = inc,
                             export_includes = inc,
                             source          = [src.change_ext('.c') for src in tg.source])
        cstlibtg.post()
        for cstlibtask in cstlibtg.tasks:
            for task in tg.tasks:
                cstlibtask.set_run_after(task)

    if 'c_shlib' in kw['lang']:
        cshlibtg = ctx.shlib(name            = uselib_name + '_c_shlib',
                             target          = uselib_name,
                             use             = ['default', 'zcm'],
                             includes        = inc,
                             export_includes = inc,
                             source          = [src.change_ext('.c') for src in tg.source])
        cshlibtg.post()
        for cshlibtask in cshlibtg.tasks:
            for task in tg.tasks:
                cshlibtask.set_run_after(task)

    if 'cpp' in kw['lang']:
        cpptg = ctx(target          = uselib_name + '_cpp',
                    rule            = 'touch ${TGT}',
                    export_includes = inc)
        cpptg.post()
        for cpptask in cpptg.tasks:
            for task in tg.tasks:
                cpptask.set_run_after(task)

    if 'java' in kw['lang']:
        javatg = ctx(name       = uselib_name + '_java',
                     features   = 'javac jar',
                     use        = ['zcmjar'],
                     srcdir     = ctx.path.find_or_declare('java/' +
                                                           javapkg_name.split('.')[0]),
                     outdir     = 'java/classes',  # path to output (for .class)
                     basedir    = 'java/classes',  # basedir for jar
                     destfile   = uselib_name + '.jar')
        javatg.post()
        for javatask in javatg.tasks:
            for task in tg.tasks:
                javatask.set_run_after(task)

@extension('.zcm')
def process_zcmtypes(self, node):
    tsk = self.create_task('zcmgen', node)

class zcmgen(Task.Task):
    color   = 'PINK'
    quiet   = False
    ext_in  = ['.zcm']
    ext_out = ['.c', '.h', '.hpp', '.java']

    ## This method processes the inputs and determines the outputs that will be generated
    ## when the zcm-gen program is run
    def runnable_status(self):
        gen = self.generator
        inp = self.inputs[0]

        if ('c_stlib' in gen.lang) or ('c_shlib' in gen.lang):
            self.set_outputs(inp.change_ext(ext='.h'))
            self.set_outputs(inp.change_ext(ext='.c'))
        if 'cpp' in gen.lang:
            self.set_outputs(inp.change_ext(ext='.hpp'))
        if 'java' in gen.lang:
            if not getattr(gen, 'javapkg', None):
                raise WafError('Java ZCMtypes must define a "javapkg"')
            pathparts = inp.abspath().split('/')
            dirparts = '/'.join(pathparts[:-1])
            nameparts = '/'.join(pathparts[-1:])
            dirparts = dirparts.replace(gen.path.abspath(), '')
            nameparts = nameparts.replace('.zcm', '.java')
            java_pkgpath = gen.javapkg.replace('.', '/')
            outp = '/'.join([dirparts, 'java', java_pkgpath, nameparts])
            outp_node = gen.path.find_or_declare(outp)
            self.outputs.append(outp_node)

        if not self.outputs:
            raise WafError('No ZCMtypes generated, ensure a valid lang is specified')

        return super(zcmgen, self).runnable_status()

    ## This method is responsible for producing the files in the self.outputs list
    ## (e.g. actually executing the zcm-gen program)
    def run(self):
        gen = self.generator

        zcmgen = self.env['ZCMGEN']
        zcmfile = self.inputs[0].abspath()
        bld = gen.path.get_bld().abspath()
        inc = bld.split('/')[-1]

        langs = {}
        if ('c_stlib' in gen.lang) or ('c_shlib' in gen.lang):
            langs['c'] = '--c --c-typeinfo --c-cpath %s --c-hpath %s --c-include %s' % (bld, bld, inc)
        if 'cpp' in gen.lang:
            langs['cpp'] = '--cpp --cpp-hpath %s --cpp-include %s' % (bld, inc)
        if 'java' in gen.lang:
            langs['java'] = '--java --jpath %s --jdefaultpkg %s' % (bld + '/java', gen.javapkg)

        # no need to check if langs is empty here, already handled in runnable_status()
        return self.exec_command('%s %s %s' % (zcmgen, zcmfile, ' '.join(langs.values())))
