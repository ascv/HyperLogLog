Overview
========

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large datasets. This module is written in C
for Python >= 3.6. It implements a 64 bit version of
HyperLogLog [2] using a Murmur64A hash.


Quick start
===========

Install Python development libraries. This step will depend on your OS. On
Ubuntu 24.0.4 LTS:
```
sudo apt install python-dev-is-python3
```

Install HLL:
```
pip install HLL
```

Example usage:
```
from HLL import HyperLogLog

hll = HyperLogLog(10) # use 2^10 registers
hll.add('some data')

estimate = hll.cardinality()
print(estimate)
```

Changelog
=========

3.0
---

**New features:**

* Added `intersection_cardinality()` for estimating the intersection of two
  HyperLogLogs using Ertl's Joint Maximum Likelihood Estimation (JMLE) method.
* Added `registers()` which returns all register values as a `bytes` object
  of length 2^p.
* Added `add_range(start, count)` for bulk insertion of sequential integers
  without Python-to-C overhead.
* Added type checking to `merge()` — passing non-HyperLogLog objects now
  raises `TypeError`.

**Sparse representation rewrite:**

* Replaced the sparse linked list with a sorted dynamic array of packed
  entries. Each entry is 4 bytes (down from 16+ bytes per linked list node),
  improving cache locality and reducing memory overhead.
* Sparse-to-dense conversion is now based on allocated capacity vs dense
  representation size, instead of a fixed count threshold. The HyperLogLog
  switches to dense when the sparse array allocation would exceed the
  equivalent dense storage.
* Sparse-sparse `merge()` now uses a two-pointer merge in O(n) time instead
  of iterating all 2^p registers.

**Bug fixes:**

* Fixed `transformToDense` not returning an error on allocation failure,
  leading to NULL pointer dereference.
* Fixed memory leaks in error paths where `malloc`'d error messages were
  never freed.
* Fixed double-counting in `add_range()` where `added` was incremented twice
  per element.
* Fixed incorrect `Py_ssize_t` types for `PyArg_ParseTuple` `s#` format in
  `add()` and `hash()`.
* Fixed `_get_meta()` reporting incorrect `max_buffer_size`.

**Cleanup:**

* Removed stale `setMemoryErrorMsg` declaration from header.
* Removed unused `.travis.yml`.
* Synced version strings across `hll.c` and `setup.py`.

2.4
---

* Use compiler built-ins for leading zero counts if using GCC or Clang.

2.3
---

* Fix bug causing cardinalities on the order of $2^{45}$ to be
  under-estimated.

2.2
---

* Remove support for Python 2.7.

2.1
---

**Deprecation notice**: this is the last supported version for Python 2.7.x.

* Fixed bug where HyperLogLogs of unequal sizes could be merged.
* Fixed bug causing cardinality estimates to be off when repeatedly merging
  sparse HyperLogLogs loaded from a pickle dump.

2.0
---

* Algorithm has been updated to a 64 bit version [2]. This fixes the
  spike in relative error when switching from linear counting in the
  original HyperLogLog algorithm.
* Hash function has been updated to the 64 bit Murmur64A function.
* More efficiently store registers using a combination of sparse and dense
  representations.
* Improved method for counting the number of leading zeroes.
* Changed the return type of `cardinality()` from float to integer.
* Changed the return logic of `add()`. This method no longer always indicates
  if a register was updated using its return value. This behavior is only
  preserved in dense representation. In sparse representation, `add()` always
  returns `False`.
* `HyperLogLog` objects pickled in 1.x and 2.x are not compatible.
* Added `get_register()`
* Added `hash()`
* Added `_get_meta()`
* Deprecated `murmur2_hash()`
* Deprecated `registers()`
* Deprecated `set_register()`

Documentation
=============

HyperLogLog objects
-------------------

`HyperLogLog` objects implement a 64 bit HyperLogLog algorithm [2]. They can
be used to estimate the cardinality of very large datasets. The estimation
accuracy is proportional to the number of registers. Using more registers
increases the accuracy and using less registers decreases the accuracy. The
number of registers is set in powers of 2 using the parameter `p` and defaults
to `p=12` or $2^{12}$ registers.
```
>>> from HLL import HyperLogLog
>>> hll = HyperLogLog() # Default to 2^12 registers
>>> hll.size()
4096
>>> hll = HyperLogLog(3) # Use 2^3 registers
>>> hll.size()
8
>>> for data in ['one', 'two', 'three', 'four',]:
...     hll.add(data)
>>> hll.cardinality()
4
```

HyperLogLogs use a Murmur64A hash. This function is fast and has a good
uniform distribution of bits which is necessary for accurate estimations. The
seed to this hash function can be set in the `HyperLogLog` constructor:
```
>>> hll = HyperLogLog(p=2, seed=123456789)
>>> hll.seed()
123456789
```

The hash function can also be called directly:
```
>>> hll.hash('something')
393810339
```

Individual registers can be printed:
```
>>> for i in range(2**4):
...     print(hll.get_register(i))
0
0
3
0
4
```

`HyperLogLog` objects can be merged. This is done by taking the maximum value
of their respective registers:
```
>>> A = HyperLogLog(p=4)
>>> A.add('hello')
>>> B = HyperLogLog(p=4)
>>> B.add('world')
>>> A.merge(B)
>>> A.cardinality()
2
```

Intersection cardinality
------------------------

The intersection cardinality of two `HyperLogLog` objects can be estimated
using Ertl's Joint Maximum Likelihood Estimation (JMLE) method [2]. Both
objects must have the same `p` value:
```
>>> A = HyperLogLog(p=12)
>>> B = HyperLogLog(p=12)
>>> for i in range(50000):
...     A.add(str(i))
...     B.add(str(i))
>>> for i in range(50000, 100000):
...     A.add(str(i))
>>> A.intersection_cardinality(B)
50164
```

Register representation
-----------------------

Registers are stored using both sparse and dense representation. Originally
all registers are initialized to zero. However storing all these zeroes
individually is wasteful. Inspired by the sorted linked list approach in [3],
a sorted dynamic array of packed 4-byte entries is used to store only registers
that have been set (e.g. have a non-zero value). When the array's allocated
memory would exceed the equivalent dense storage, the `HyperLogLog` switches
to dense representation where registers are stored individually using 6 bits.

Sparse representation can be disabled using the `sparse` flag:
```
>>> HyperLogLog(p=2, sparse=False)
```

Adding an element to the `HyperLogLog` does not immediately update the
sorted array. A temporary buffer is used to defer this operation. When the
buffer is full the items are sorted and merged into the array in one pass.

The buffer size can be set using `max_buffer_size`:
```
>>> HyperLogLog(p=15, max_buffer_size=10**5)
```

License
=======

This software is released under the [MIT License](LICENSE).

References
==========

[1] P. Flajolet, E. Fusy, O. Gandouet, F. Meunier. "HyperLogLog: the analysis
    of a near-optimal cardinality estimation algorithm," Conference on the
    Analysis of Algorithms 2007.

[2] O. Ertl, "New Cardinality Estimation Methods for HyperLogLog Sketches,"
    arXiv:1706.07290 [cs], June 2017.

[3] S. Heule, M. Nunkesser, A. Hall. "HyperLogLog in Practice: Algorithmic
    Engineering of a State of the Art Cardinality Estimation Algorithm,"
    Proceedings of the EDBT 2013 Conference, ACM, Genoa March 2013.
