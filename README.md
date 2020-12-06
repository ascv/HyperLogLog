
![Build Status](https://travis-ci.org/ascv/HyperLogLog.png?branch=master)
(https://travis-ci.org/ascv/HyperLogLog)

v 2.0.1

Overview
========

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module is written in C
for python 2.7.x and python >= 3.5. It implements an 64 bit version of the
algorithm utilizing a Murmur64A hash with support for sparse and dense
register representations [3].

Quick start
===========

Install python development libraries. On Ubuntu:

    sudo apt-get install python-dev

Install HLL:

    pip install HLL

Example usage:

    from HLL import HyperLogLog

    hll = HyperLogLog(10) # use 2^10 registers
    hll.add('some data')
    estimate = hll.cardinality()

API Documentation
=================

HyperLogLog objects
-------------------

HyperLogLogs estimate the cardinality of a multi-set [X]. The number of registers
is set by the parameter `k` using the formula `2^k`.
```
>>> hll = HyperLogLog(k=3)
>>> hll.size()
8
>>> for data in ['one', 'two', 'three', 'four',]:
...     hll.add(data)
>>> hll.cardinality()
4
```
Note that the estimation power is proportional to the number of registers.
Increasing the number of registers increases the estimation accuracy of
larger cardinalities. For practical purposes, the default k=12 allows
accurate estimation of cardinalities in the range [0, x] which is
sufficient for many applications.

HyperLogLog's use a Murmur64A hash. The seed to this hash can be set:
```
>>> hll = HyperLogLog(k=2, seed=123456789)
>>> hll.seed()
12345679
```

The Murmur64A hash function can be called directly:
```
>>> HyperLogLog.hash('something')
393810339
```

HyperLogLogs can be merged by taking the maximum value of the respective
registers:
```
>>> hll = HyperLogLog(k=4)
>>> hll.add('hello')
>>> another_hll = HyperLogLog(k=4)
>>> another_hll.add('world')
>>> hll.merge(another_HLL)
>>> hll.cardinality()
2
```

Individual registers can be printed:
```
>>> for i in xrange(0, 2**k):
        print(hll.get_register(i))
0
0
3
0
4
```

Register representation
-----------------------

When a HyperLogLog is created registers are initialized to zero. Since the
registers are all zero storing them individually is wasteful. Instead the
registers use a sparse representation where only non-zero values are stored
in a sorted difference encoded linked list [2]. When this list reaches
sufficient size a dense representation is used where register values are
individually stored using 6 bits.

The maximum linked list size, which determines when the HyperLogLog switches
from sparse to dense representation, defaults to XXXX and can be set using
`max_list_size`:
```
>>>> HyperLogLog(k=2, max_list_size=100)
```
To avoid the expense of traversing the list for every register comparison
an append only buffer is used. Items added to the HyperLogLog are appended
to the end of the buffer. When the buffer is full the items are sorted and
inserted into the linked list in one pass. The default buffer size is
`xxxx` and can be set using `max_buffer_size`:
```
>>>> HyperLogLog(k=2, max_buffer_size=10)
```
Sparse representation can be disabled using the `sparse` flag:
```
>>>> HyperLogLog(k=2, sparse=False)
```

API Documentation
=================

```
add(data)
```
Adds `data` to the estimator where data is a string, buffer, or bytes
type. Returns `True` if the registers were updated otherwise returns `False`.

```
cardinality()
```
Gets the cardinality estimate.

```
merge(hll)
```
Merges another HyperLogLog into the current one. Merging compares individual
registers and takes the maximum value for each one. The registers of the other
HyperLogLog are unaffected.

```
get_register(i)
```
Gets the register at index `i`.

```
hash(data, seed=314)
```
Gets an unsigned integer from a Murmur64A hash of `data` where `data` is a
string, buffer, or bytes (python 3.x).

```
seed()
```
Gets the seed value used in the Murmur3 hash.

```
size()
```
Gets the number of registers.

License
=======

This software is released under the [MIT License](LICENSE).

References
==========

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
[2] https://github.com/PeterScott/murmur3
