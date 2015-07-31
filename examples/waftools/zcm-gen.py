#!/usr/bin/env python
import os
from waflib import Task
from waflib.TaskGen import extension, feature

def configure(conf):
    conf.find_program('zcm-gen', var='ZCMGEN',  mandatory=True)

@extension('.zcm')
def process_zcmtypes(self, node):
    self.create_task('zcmgen', node)

class zcmgen(Task.Task):
    color = 'PINK'
    quiet = True

    ## This method processes the inputs and determines the outputs that will be generated
    ## when the zcm-gen program is run
    def runnable_status(self):
        gen = self.generator
        inp = self.inputs[0]
        if 'cxx' in gen.lang:
            self.outputs.append(inp.change_ext(ext='.hpp'))
        if 'java' in gen.lang and getattr(gen, 'javapkg', None):
            pathparts = inp.abspath().split('/')
            dirparts = '/'.join(pathparts[:-1])
            nameparts = '/'.join(pathparts[-1:])
            dirparts = dirparts.replace(gen.path.abspath(), '')
            nameparts = nameparts.replace('.zcm', '.java')
            java_pkgpath = gen.javapkg.replace('.', '/')
            outp = '/'.join([dirparts, 'java', java_pkgpath, nameparts])
            outp_node = gen.path.find_or_declare(outp)
            self.outputs.append(outp_node)
        return super(zcmgen, self).runnable_status()

    ## This method is responsible for producing the files in the self.outputs list
    ## (e.g. actually executing the zcm-gen program)
    def run(self):
        zcmgen = self.env['LCMGEN'][0]
        zcmfile = self.inputs[0].abspath()
        bld = self.generator.path.get_bld().abspath()
        for outp in self.outputs:
            if outp.name.endswith('.hpp'):
                outpath = os.path.dirname(outp.abspath())
                self.exec_command('%s --cpp %s --cpp-hpath %s' % (zcmgen, zcmfile, outpath))
            elif outp.name.endswith('.java'):
                gen = self.generator
                zcm_jflags = '--jdefaultpkg=%s' % (gen.javapkg)
                pkgpath = gen.javapkg.replace('.', '/')
                outpath = os.path.dirname(outp.abspath())
                jpath = outpath[:-len(pkgpath)]
                self.exec_command('%s --java %s --jpath %s %s' % (zcmgen, zcmfile, jpath, zcm_jflags))
