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
        Extension('HLL', ['src/hll.c', 'lib/murmur2.c', ]),
    ],
    headers=['src/hll.h', 'lib/murmur2.h', ],
    keywords=['HyperLogLog', 'Hyper LogLog', 'Hyper Log Log', 'LogLog', 'Log Log', 'cardinality', 'counting', 'sketch'],
    license='MIT',
    long_description=\
"""
The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This library implements a 64
bit variant [2] written in C that uses a MurmurHash64A hash function.

[1] Flajolet, Philippe; Fusy, Eric; Gandouet, Olivier; Meunier, Frederic
(2007). "Hyperloglog: The analysis of a near-optimal cardinality estimation
algorithm" (PDF). Disc. Math. and Theor. Comp. Sci. Proceedings. AH: 127146.
CiteSeerX 10.1.1.76.4286.

[2] Omar Ertl, "New cardinality estimation algorithms for HyperLogLog Sketches"
arXiv:1702.01284 [cs] Feb. 2017.
"""
)
