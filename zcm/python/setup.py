from setuptools import setup, find_packages
from setuptools.extension import Extension
from Cython.Build import cythonize
import os

from ctypes.util import find_library
if find_library("zcm") is None:
    print("\n\n"
          "##################################################################################################\n"
          "#  Warning! You need to install the ZeroCM library before installing the zerocm python package!  #\n"
          "#  Build it from source here: https://github.com/ZeroCM/zcm                                      #\n"
          "##################################################################################################\n\n")

def parse_version():
    with open("version.txt", "r") as fh:
        version = fh.read()
    return version

setup(
    name='zerocm',
    version=parse_version(),
    url='https://github.com/ZeroCM/zcm',
    license='LGPL',
    packages=find_packages(),
    py_modules='zerocm',
    ext_modules=cythonize([
        Extension(
            "zerocm",
            ["zerocm.pyx"],
            libraries=['zcm']
        )
    ]),
)
