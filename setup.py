from distutils.core import setup, Extension

setup(
    name='HLL',
    version='2.1.0',
    description='Fast HyperLogLog',
    author='Joshua Andersen',
    author_email='josh.h.andersen@gmail.com',
    maintainer='Joshua Andersen',
    url='https://github.com/ascv/HyperLogLog',
    ext_modules=[
        Extension('HLL', ['src/hll.c', 'lib/murmur2.c', ]),
    ],
    headers=['src/hll.h', 'lib/murmur2.h', ],
    keywords=['algorithm', 'approximate counting', 'big data', 'big data', 'cardinality', 'cardinality estimate', 'counting', 'data analysis', 'data processing', 'data science', 'data sketching', 'efficient computation', 'estimating cardinality', 'fast', 'frequency estimation', 'hyper log log', 'hyper loglog', 'hyperloglog', 'large-scale data', 'log log', 'loglog', 'memory efficient', 'probability estimate', 'probability sketch', 'probablistic counting', 'probablistic data structures', 'real-time analytics', 'scalable', 'set cardinality', 'set operations', 'sketch', 'statistical analysis', 'streaming algorithms', 'streaming algorithms', 'unique count', 'unique element counting'],
    license='MIT',
    long_description=\
"""
Fast HyperLogLog for Python.

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This library implements a 64
bit variant [2] for Python, written in C, that uses a MurmurHash64A hash
function.

[1] Flajolet, Philippe; Fusy, Eric; Gandouet, Olivier; Meunier, Frederic
(2007). "Hyperloglog: The analysis of a near-optimal cardinality estimation
algorithm" (PDF). Disc. Math. and Theor. Comp. Sci. Proceedings. AH: 127146.
CiteSeerX 10.1.1.76.4286.

[2] Omar Ertl, "New cardinality estimation algorithms for HyperLogLog Sketches"
arXiv:1702.01284 [cs] Feb. 2017.
"""
)
