from distutils.core import setup, Extension

setup(
    name='HLL',
    version='2.0.0',
    description='HyperLogLog implementation in C for python.',
    author='Joshua Andersen',
    author_email='josh.h.andersen@gmail.com',
    maintainer='Joshua Andersen',
    url='https://github.com/ascv/HyperLogLog',
    ext_modules=[
        Extension('HLL', ['src/hll.c', 'lib/murmur3.c', 'lib/murmur2.c']),
    ],
    headers=['src/hll.h', 'lib/murmur3.h', 'lib/murmur2.h'],
    keywords=['HyperLogLog', 'Hyper LogLog', 'Hyper Log Log', 'LogLog', 'Log Log', 'cardinality', 'counting', 'sketch'],
    license='MIT',
    long_description=\
"""
The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module is written in C
and uses the Murmurhash64A hash function. It uses an improved version of the
algorithm [2] that significantly larger cardinalities with better accuracy.


[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
"""
)
