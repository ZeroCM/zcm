from distutils.core import setup
from Cython.Build import cythonize

setup(
    name = "zcm",
    ext_modules = cythonize('*.pyx'),
)
