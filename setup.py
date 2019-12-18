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
cardinality of extraordinarily large data sets. This implementation [2] is
written in C. Specifically

* Improved version of the algorithm eliminates the need to do any bias
  correction
* Uses the 64 bit MurmurHash64A hash function
* 6 bit register encoding
* No bias correction (e.g. HLL+, SlidingHyperLogLog [3]) since it isn't

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
[2] New Paper
"""
)
