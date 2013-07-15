from distutils.core import setup, Extension

setup(
    name="HLL", 
    version="0.6.1", 
    description='HyperLogLog algorithm written in C',
    author="Joshua Andersen",
    url='https://github.com/ascv/HLL',
    ext_modules=[
        Extension("HLL", ["hll.c", "murmur3.c"]),
    ]
)
