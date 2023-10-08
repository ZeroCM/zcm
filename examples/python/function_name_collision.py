#!/usr/bin/python

blddir= os.path.dirname(os.path.realpath(__file__)) + '/../../build/examples/examples/'
sys.path.insert(0, blddir + "types/")
from function_name_collision_t import function_name_collision_t

# declare a new msg and populate it
msg = function_name_collision_t()
