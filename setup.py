from pathlib import Path
from setuptools import setup, Extension

here = Path(__file__).parent
readme = (here/"README.md").read_text()

module = Extension(
    'HLL',
    sources=['src/hll.c', 'lib/murmur2.c'],
    include_dirs=['src', 'lib']
)

setup(
    name='HLL',
    version='2.1.7',
    description='Fast HyperLogLog for Python',
    author='Joshua Andersen',
    author_email='josh.h.andersen@gmail.com',
    maintainer='Joshua Andersen',
    url='https://github.com/ascv/HyperLogLog',
    ext_modules=[module],
    zip_safe=False,
    keywords=['algorithm', 'approximate counting', 'big data', 'big data', 'cardinality', 'cardinality estimate', 'counting', 'data analysis', 'data processing', 'data science', 'data sketching', 'efficient computation', 'estimating cardinality', 'fast', 'frequency estimation', 'hyper log log', 'hyper loglog', 'hyperloglog', 'large-scale data', 'log log', 'loglog', 'memory efficient', 'probability estimate', 'probability sketch', 'probablistic counting', 'probablistic data structures', 'real-time analytics', 'scalable', 'set cardinality', 'set operations', 'sketch', 'statistical analysis', 'streaming algorithms', 'streaming algorithms', 'unique count', 'unique element counting'],
    license='MIT',
    long_description=readme,
    long_description_content_type='text/markdown'
)
