Overview
========

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large datasets. This module is written in C
for Python >= 3.6 and Python 2.7.x. It implements a 64 bit version of
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

2.3 Changelog
=============

* Fix bug causing cardinalities on the order of $2^{45}$ to be
  under-estimated.

2.2 Changelog
=============

* Remove support for Python 2.7.

2.1 Changelog
=============

**Deprecation notice**: this is the last supported version for Python 2.7.x.

* Fixed bug where HyperLogLogs of unequal sizes could be merged.
* Fixed bug causing cardinality estimates to be off when repeatedly merging
  sparse HyperLogLogs loaded from a pickle dump.

2.0 Changelog
=============

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
>>> from hll import HyperLogLog
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

Register representation
-----------------------

Registers are stored using both sparse and dense representation. Originally
all registers are initialized to zero. However storing all these zeroes
individually is wasteful. Instead a sorted linked list [3] is used to store
only registers that have been set (e.g. have a non-zero value). When this list
reaches sufficient size the `HyperLogLog` object will switch to using dense
representation where registers are stored invidiaully using 6 bits.

Sparse representation can be disabled using the `sparse` flag:
```
>>> HyperLogLog(p=2, sparse=False)
```

The maximum list size for the sparse register list determines when the
`HyperLogLog` object switches to dense representation. This can be set
using `max_list_size`:
```
>>> HyperLogLog(p=15, max_list_size=10**6)
```

Traversing the sparse register list every time an item is added to the
`HyperLogLog` to update a register is expensive. A temporary buffer is instead
used to defer this operation. Items added to the `HyperLogLog` are first added
to the temporary buffer. When the buffer is full the items are sorted and then
any register updates occur. These updates can be done in one pass since both
the temproary buffer and sparse register list are sorted.

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

[3] S. Heule, M. Nunkesser, A. Hall. "HyperLogLog in Practice: Algorithimic
    Engineering of a State of the Art Cardinality Estimation Algorithm,"
    Proceedings of the EDBT 2013 Conference, ACM, Genoa March 2013.
