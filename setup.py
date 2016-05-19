from distutils.core import setup, Extension

setup(
    name='HLL',
    version='1.1',
    description='HyperLogLog implementation in C.',
    author='Joshua Andersen',
    author_email='anderj0@uw.edu',
    maintainer='Joshua Andersen',
    url='https://github.com/ascv/HyperLogLog',
    ext_modules=[
        Extension('HLL', ['hll.c', 'murmur3.c']),
    ],
    headers=['hll.h', 'murmur3.h'],
    keywords=['HyperLogLog', 'Hyper LogLog', 'LogLog', 'cardinality', 'probablistic counting'],
    long_description=\
"""
The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module is written in C
using a Murmur3 hash, for python 2.7.x or python 3.x.

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
"""
)
