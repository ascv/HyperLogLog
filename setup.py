from distutils.core import setup, Extension

setup(
    name="HLL", 
    version="0.831", 
    description='The HyperLogLog algorithm implemented in C.',
    author="Joshua Andersen",
    url='https://github.com/ascv/HLL',
    ext_modules=[
        Extension("HLL", ["hll.c", "murmur3.c"]),
    ],
    headers=['hll.h','murmur3.h']
)
