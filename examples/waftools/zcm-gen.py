#!/usr/bin/env python
# encoding: utf-8

import os
import waflib
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
    # TODO: We should do the above; changes to the zcm-gen binary do not force the regeneration
    #       of the zcmtypes. This is generally not a huge problem because
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
#   build:        False if only zcmtype generation (no compiling or linking) is desired
#                 Defaults to true.
#   lang:         list of languages for which zcmtypes should be generated, options are:
#                 ['c_stlib', 'c_shlib', 'cpp', 'java', 'python', 'nodejs', 'julia'].
#                 Can also be a space separated string.
#   littleEndian: True or false based on desired endianess of output. Should almost always
#                 be false. Don't use this option unless you really know what you're doing
#   javapkg:      name of the java package
#                 default = 'zcmtypes' (though it is encouraged to name it something more unique
#                                       to avoid library naming conflicts)
#
#   TODO: add support for changing c/c++ include directory. Currently defaults so that the
#         description below works
#
# Using the output zcmtypes:
#   For the following explanations, assume:
#     ${name} = value entered for the "name" keyword,
#     ${dir}  = dir containing the zcmtypes (eg for ~/zcm/examples/types/*.zcm, ${dir} = "types")
#     ${pkg}  = value entered for the "javapkg" keyword
#
#   C:             types will be compiled into a static library and linked into targets
#     wscript:     add '${name}_c' to the list of "use" dependencies
#     c files:     target include directives at "${dir}/type_name_t.h"
#
#   C++:           types are header only and will be included directly in source
#     wscript:     add '${name}_cpp' to the list of "use" dependencies
#     cpp files:   target include directives at "${dir}/type_name_t.hpp"
#
#   Java:          types will be compiled into a .jar, see details below on execution
#     wscript:     add '${name}_java' to the list of "use" dependencies
#     java files:  target import directives at "$pkg/type_name_t"
#
#   Python:        types will be compiled into .py python files and are imported
#                  into final python script.
#     wscript:     add '${name}_python' to the list of "use" dependencies if you
#                  have any build time dependencies (most python dependencies,
#                  however, are runtime so this will often, if not always, go unused).
#     py files:    add directory containing .py files to your sys path and target
#                  import directives at "$pkg/type_name_t"
#
#   NodeJS:        types will be compiled into a zcmtypes.js file which is
#                  'require'd in your nodejs main app.js file and passed into
#                  nodes zcm constructor
#     wscript:     add '${name}_nodejs to the list of "use" dependencies if you
#                  have any build time dependencies (most nodejs dependencies,
#                  however, are runtime so this will often, if not always, go unused).
#
#   Julia:         types will be compiled into .jl files. Simply `include()` them
#                  into the final julia script
#     wscript:     add '${name}_julia' to the list of "use" dependencies
#     julia files: target import directives at "${dir}/type_name_t"
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

def outFileName(ctx, inp, lang, absPath=False):
    fileparts = getFileParts(ctx, inp)

    def defaultOutFileName(fileparts, absPath):
        ret = ""
        if absPath:
            if fileparts[3] != "":
                ret = fileparts[3]
        if fileparts[1] != "":
            if ret != "":
                ret = ret + "/"
            ret = ret + '/'.join(fileparts[1].split('.'))
        if fileparts[2] != "":
            if ret != "":
                ret = ret + "/"
            ret = ret + fileparts[2]
        return ret

    if lang == 'zcm':
        return defaultOutFileName(fileparts, absPath)
    if lang == 'c':
        hfileparts = fileparts[:]
        cfileparts = fileparts[:]
        hfileparts[2] = fileparts[2].replace('.zcm', '.h')
        cfileparts[2] = fileparts[2].replace('.zcm', '.c')
        if fileparts[1] != "":
            hfileparts[2] = '_'.join(fileparts[1].split('.')) + "_" + hfileparts[2]
            cfileparts[2] = '_'.join(fileparts[1].split('.')) + "_" + cfileparts[2]
        return [defaultOutFileName(hfileparts, absPath).replace('.zcm', '.h'),
                defaultOutFileName(cfileparts, absPath).replace('.zcm', '.c')]
    if lang == 'cpp':
        return defaultOutFileName(fileparts, absPath).replace('.zcm', '.hpp')
    if lang == 'python':
        return defaultOutFileName(fileparts, absPath).replace('.zcm', '.py')
    if lang == 'julia':
        return defaultOutFileName(fileparts, absPath).replace('.zcm', '.jl')

    raise WafError('This should not be possible')


def getFileParts(ctx, path):
    package = ctx.cmd_and_log('zcm-gen --package %s' % (path),
                              output=waflib.Context.STDOUT,
                              quiet=waflib.Context.BOTH).strip()
    pathparts = path.split('/')
    absdirparts = '/'.join(pathparts[:-1])
    nameparts = '/'.join(pathparts[-1:])
    reldirparts = absdirparts.replace(ctx.path.abspath() + '/', '')
    return [reldirparts, package, nameparts, absdirparts]


@conf
def zcmgen(ctx, **kw):
    if not getattr(ctx.env, 'ZCMGEN', []):
        raise WafError('zcmgen requires ctx.env.ZCMGEN set to the zcm-gen executable')

    uselib_name = 'zcmtypes'
    if 'name' in kw:
        uselib_name = kw['name']

    javapkg_name = 'zcmtypes'
    if 'javapkg' in kw:
        javapkg_name = kw['javapkg']

    building = True
    if 'build' in kw:
        building = kw['build']

    littleEndian = False
    if 'littleEndian' in kw:
        littleEndian = kw['littleEndian']

    if 'lang' not in kw:
        # TODO: this should probably be a more specific error type
        raise WafError('zcmgen requires keword argument: "lang"')

    lang = kw['lang']
    if isinstance(kw['lang'], basestring):
        lang = kw['lang'].split(' ')

    if 'source' not in kw:
        # TODO: this should probably be a more specific error type
        raise WafError('zcmgen requires keword argument: "source"')

    # exit early if no source files input
    if not kw['source']:
        return

    zcmgen = ctx.root.find_or_declare(ctx.env.ZCMGEN)

    bld = ctx.path.get_bld().abspath()
    inc = os.path.dirname(bld)

    if 'nodejs' in lang:
        bldcmd = '%s --node --npath %s ' % (ctx.env['ZCMGEN'], bld)
        ctx(name   = uselib_name + '_nodejs',
            target = 'zcmtypes.js',
            source = kw['source'],
            rule   = bldcmd + '${SRC}')

    # Add .zcm files to build so the process_zcmtypes rule picks them up
    if len(lang) == 0 or ('nodejs' in lang and len(lang) == 1):
        return

    genfiles_name = uselib_name + '_genfiles'
    tg = ctx(name         = genfiles_name,
             source       = kw['source'],
             lang         = lang,
             littleEndian = littleEndian,
             javapkg      = javapkg_name)
    for s in tg.source:
        ctx.add_manual_dependency(s, zcmgen)

    if not building:
        return

    if 'c_stlib' in lang or 'c_shlib' in lang:
        csrc = []
        for src in tg.source:
            outfile = outFileName(ctx, src.abspath(), 'c')
            outnode = ctx.path.find_or_declare(outfile[1])
            csrc.append(outnode)

    if 'c_stlib' in lang:
        ctx.stlib(name            = uselib_name + '_c_stlib',
                  target          = uselib_name,
                  use             = ['default', 'zcm'],
                  includes        = inc,
                  export_includes = inc,
                  source          = csrc)

    if 'c_shlib' in lang:
        ctx.shlib(name            = uselib_name + '_c_shlib',
                  target          = uselib_name,
                  use             = ['default', 'zcm'],
                  includes        = inc,
                  export_includes = inc,
                  source          = csrc)

    if 'cpp' in lang:
        ctx(target          = uselib_name + '_cpp',
            rule            = 'touch ${TGT}',
            export_includes = inc)

    if 'java' in lang:
        ctx(name       = uselib_name + '_java',
            features   = 'javac jar',
            use        = ['zcmjar', genfiles_name],
            srcdir     = ctx.path.find_or_declare('java/' +
                                                  javapkg_name.split('.')[0]),
            outdir     = 'java/classes',  # path to output (for .class)
            basedir    = 'java/classes',  # basedir for jar
            destfile   = uselib_name + '.jar')

    if 'python' in lang:
        ctx(target = uselib_name + '_python',
            rule   = 'touch ${TGT}')

    if 'julia' in lang:
        ctx(target = uselib_name + '_julia',
            rule   = 'touch ${TGT}')

@extension('.zcm')
def process_zcmtypes(self, node):
    tsk = self.create_task('zcmgen', node)

class zcmgen(Task.Task):
    color   = 'PINK'
    quiet   = False
    ext_in  = ['.zcm']
    ext_out = ['.c', '.h', '.hpp', '.java', '.py']

    ## This method processes the inputs and determines the outputs that will be generated
    ## when the zcm-gen program is run
    def runnable_status(self):
        gen = self.generator
        inp = self.inputs[0]

        if ('c_stlib' in gen.lang) or ('c_shlib' in gen.lang):
            filenames = outFileName(gen.bld, inp.abspath(), 'c')
            outh_node = gen.path.find_or_declare(filenames[0])
            outc_node = gen.path.find_or_declare(filenames[1])
            self.outputs.append(outh_node)
            self.outputs.append(outc_node)
        if 'cpp' in gen.lang:
            filename = outFileName(gen.bld, inp.abspath(), 'cpp')
            node = gen.path.find_or_declare(filename)
            self.outputs.append(node)
        if 'java' in gen.lang:
            fileparts = getFileParts(gen.bld, inp.abspath())
            fileparts[2] = fileparts[2].replace('.zcm', '.java')
            if fileparts[1] == "":
                if not getattr(gen, 'javapkg', None):
                    raise WafError('No package specified for java zcmtype ' \
                                   'generation. Specify package with a ' \
                                   '"package <pkg>;" statement at the top of ' \
                                   'the type or with the "javapkg" build keyword')
                else:
                    fileparts[1] = gen.javapkg.replace('.', '/')
            else:
                if getattr(gen, 'javapkg', None):
                    fileparts[1] = (gen.javapkg + "/" + fileparts[1]).replace('.', '/')
                else:
                    fileparts[1] = fileparts[1].replace('.', '/')

            outp = '/'.join(['java', fileparts[1], fileparts[2]])
            outp_node = gen.path.get_bld().make_node(outp)
            self.outputs.append(outp_node)
        if 'python' in gen.lang:
            filename = outFileName(gen.bld, inp.abspath(), 'python')
            node = gen.path.find_or_declare(filename)
            self.outputs.append(node)
        if 'julia' in gen.lang:
            filename = outFileName(gen.bld, inp.abspath(), 'julia')
            node = gen.path.find_or_declare(filename)
            self.outputs.append(node)

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
            langs['c'] = '--c --c-typeinfo --c-cpath %s --c-hpath %s --c-include %s' % \
                         (bld, bld, inc)
        if 'cpp' in gen.lang:
            langs['cpp'] = '--cpp --cpp-hpath %s --cpp-include %s' % (bld, inc)
        if 'java' in gen.lang:
            langs['java'] = '--java --jpath %s --jdefaultpkg %s' % (bld + '/java', gen.javapkg)
        if 'python' in gen.lang:
            langs['python'] = '--python --ppath %s' % (bld)
        if 'julia' in gen.lang:
            langs['julia'] = '--julia --julia-path %s' % (bld)

        if gen.littleEndian:
            langs['endian'] = ' --little-endian-encoding '

        # no need to check if langs is empty here, already handled in runnable_status()
        return self.exec_command('%s %s %s' % (zcmgen, zcmfile, ' '.join(langs.values())))
