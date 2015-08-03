#!/usr/bin/env python
import os
from waflib import Task
from waflib.TaskGen import extension, feature, before_method
from waflib.Configure import conf

def configure(ctx):
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
@conf
def zcmgen(ctx, **kw):
    tg = ctx(source = kw['source'],
             lang = kw['lang'])

    uselib_name = 'zcmtypes'
    if 'name' in kw:
        uselib_name = kw['name']

    ctx.add_group()

    if 'c' in kw['lang']:
        ctx.stlib(name            = uselib_name + '_c',
                  target          = uselib_name,
                  use             = ['default', 'zcm'],
                  includes        = os.path.dirname(ctx.path.get_bld().abspath()),
                  export_includes = os.path.dirname(ctx.path.get_bld().abspath()),
                  source          = [src.change_ext('.c') for src in tg.source])

@extension('.zcm')
def process_zcmtypes(self, node):
    tsk = self.create_task('zcmgen', node)

class zcmgen(Task.Task):
    color   = 'PINK'
    quiet   = False
    ext_in  = ['.zcm']
    ext_out = ['.c', '.h', '.hpp']

    ## This method processes the inputs and determines the outputs that will be generated
    ## when the zcm-gen program is run
    def runnable_status(self):
        gen = self.generator
        inp = self.inputs[0]

        if 'c' in gen.lang:
            self.set_outputs(inp.change_ext(ext='.h'))
            self.set_outputs(inp.change_ext(ext='.c'))
        if 'cpp' in gen.lang:
            self.set_outputs(inp.change_ext(ext='.hpp'))
        #if 'java' in gen.lang and getattr(gen, 'javapkg', None):
            #pathparts = inp.abspath().split('/')
            #dirparts = '/'.join(pathparts[:-1])
            #nameparts = '/'.join(pathparts[-1:])
            #dirparts = dirparts.replace(gen.path.abspath(), '')
            #nameparts = nameparts.replace('.zcm', '.java')
            #java_pkgpath = gen.javapkg.replace('.', '/')
            #outp = '/'.join([dirparts, 'java', java_pkgpath, nameparts])
            #outp_node = gen.path.find_or_declare(outp)
            #self.outputs.append(outp_node)
        ret = super(zcmgen, self).runnable_status()
        return ret

    ## This method is responsible for producing the files in the self.outputs list
    ## (e.g. actually executing the zcm-gen program)
    def run(self):
        gen = self.generator

        zcmgen = self.env['ZCMGEN']
        zcmfile = self.inputs[0].abspath()
        bld = self.generator.path.get_bld().abspath()
        inc = bld.split('/')[-1]
        # TODO: need to return success / failure

        if 'c' in gen.lang:
            self.exec_command('%s --c %s --c-cpath %s --c-hpath %s --c-include %s' % \
                              (zcmgen, zcmfile, bld, bld, inc))
        if 'cpp' in gen.lang:
            self.exec_command('%s --cpp %s --cpp-hpath %s --cpp-include %s' % \
                              (zcmgen, zcmfile, bld, inc))


        #for outp in self.outputs:
            #if outp.name.endswith('.h'): # This covers output endswith '.c' as well
                #outpath = os.path.dirname(outp.abspath())
                #self.exec_command('%s --c %s --c-cpath %s --c-hpath %s' % \
                                  #(zcmgen, zcmfile, outpath, outpath))
            #if outp.name.endswith('.hpp'):
                #outpath = os.path.dirname(outp.abspath())
                #self.exec_command('%s --cpp %s --cpp-hpath %s' % (zcmgen, zcmfile, outpath))
            #elif outp.name.endswith('.java'):
                #gen = self.generator
                #zcm_jflags = '--jdefaultpkg=%s' % (gen.javapkg)
                #pkgpath = gen.javapkg.replace('.', '/')
                #outpath = os.path.dirname(outp.abspath())
                #jpath = outpath[:-len(pkgpath)]
                #self.exec_command('%s --java %s --jpath %s %s' % (zcmgen, zcmfile, jpath, zcm_jflags))
