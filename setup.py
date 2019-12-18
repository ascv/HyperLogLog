from distutils.core import setup, Extension

setup(
    name='HLL',
    version='2.0.0',
    description='Fast HyperLogLog library written in C for python.',
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
and implements an improved variant of the algorithm [2]. Specifically:

* Uses a 64 bit MurmurHash64A which supports much larger cardinalities so
  long range correction is no longer necessary.
* No bias correction is necessary near 2^k (e.g. Sliding HyperLogLog, HLL++ [3])
  since there is no spike in relative error.
* 6 bit register encoding to minimize memory usage

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
[2] New Paper
[3] Google
"""
)
