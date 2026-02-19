from pathlib import Path
from setuptools import setup, Extension

here = Path(__file__).parent
readme = (here / "README.md").read_text()

module = Extension(
    'HLL',
    sources=['src/hll.c', 'lib/murmur2.c'],
    include_dirs=['src', 'lib']
)

setup(
    name='HLL',
    version='3.0.0',
    description='Fast HyperLogLog for Python',
    author='Joshua Andersen',
    author_email='josh.h.andersen@gmail.com',
    maintainer='Joshua Andersen',
    url='https://github.com/ascv/HyperLogLog',
    ext_modules=[module],
    zip_safe=False,
    python_requires='>=3.9',
    keywords=[
        'hyperloglog', 'cardinality', 'cardinality estimate',
        'approximate counting', 'probabilistic data structures',
        'sketch', 'data science', 'big data', 'streaming algorithms',
        'memory efficient', 'set cardinality', 'unique count',
    ],
    license='MIT',
    long_description=readme,
    long_description_content_type='text/markdown',
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: POSIX :: Linux',
        'Operating System :: MacOS',
        'Programming Language :: C',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Programming Language :: Python :: 3.13',
        'Topic :: Scientific/Engineering',
    ],
    project_urls={
        'Source': 'https://github.com/ascv/HyperLogLog',
        'Bug Tracker': 'https://github.com/ascv/HyperLogLog/issues',
    },
)
