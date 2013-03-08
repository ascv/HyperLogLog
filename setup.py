from distutils.core import setup, Extension

setup(name="HyperLogLog", version="1.0", ext_modules=[
        Extension("HLL", ["hll.c", "murmur3.c"]),])
