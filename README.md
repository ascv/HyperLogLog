
![Build Status](https://travis-ci.org/ascv/HyperLogLog.png?branch=master)
(https://travis-ci.org/ascv/HyperLogLog)

v 2.0.1

Overview
========

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module is written in C
for python >= 3.5 and python 2.7.x. It implements a 64 bit version of
HyperLogLog using a Murmur64A hash, with support for both sparse and dense
encoding.

Quick start
===========

Install python development libraries. On Ubuntu:
```
sudo apt-get install python-dev
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
```

Documentation
=============

HyperLogLog objects
-------------------

HyperLogLogs estimate the cardinality of a multi-set [x]. The estimation power
is proportional to the number of registers which is controlled by the
parameter `k` using the formula `2^k`:
```
>>> hll = HyperLogLog(k=3) # Use 2^3=8 registers
>>> hll.size()
8
>>> for data in ['one', 'two', 'three', 'four',]:
...     hll.add(data)
>>> hll.cardinality()
4
```
The default number of registers, k=12, allows for accurate estimation of
cardinalities in the range [0, x] which is sufficient for many applications.
The graph below shows the accuracy at various cardinalities and register
counts:

<insert graph>

HyperLogLogs use a Murmur64A hash. This hash function is fast and has a good
uniform distribution of bits. The seed to this hash function can be set:
```
>>> hll = HyperLogLog(k=2, seed=123456789)
>>> hll.seed()
12345679
```

The hash function can also be called directly:
```
>>> HyperLogLog.hash('something')
393810339
```

HyperLogLogs can be merged. This is done by taking the maximum value of the
respective registers:
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
>>> for i in range(0, 2**k):
        print(hll.get_register(i))
0
0
3
0
4
```

Register representation
-----------------------

When a HyperLogLog is created the registers are initialized to zero. Storing
these zeroes individually is wasteful. Instead the registers are stored in a
sparse representation where only non-zero values are stored. These values are
kept in a sorted linked list [2]. When this list reaches sufficient size then
the HyperLogLog switches to a dense representation where registers are stored
individually using 6 bits.

The maximum list size, which determines when the switch from sparse to dense
representation occurs, can be set using `max_list_size`:
```
>>>> HyperLogLog(k=2, max_list_size=100)
```
To avoid the expense of traversing the list for every register comparison
an append only buffer is used. Items added to the HyperLogLog are first
appended to the buffer. When the buffer is full the items are sorted and
items are inserted into the sorted link list in one pass.

inserted into the sorted linked list in one pass. This behavior defers
register comparison until insertion. The default buffer size is
`xxxx` and can be set using `max_buffer_size`:
```
>>>> HyperLogLog(k=2, max_buffer_size=10)
```
Note that some methods will cause the buffer to cleared upon invocation. This
is because these methods require full access to current register values. These
methods are: `cardinality()` and `merge()`. The buffer is also cleared when
pickling e.g. `pickle.dumps(my_hyperloglog)`.

Sparse representation can be disabled using the `sparse` flag:
```
>>>> HyperLogLog(k=2, sparse=False)
```

API
---

```
add(data)
```
Adds `data` to the HyperLogLog where data is a string, buffer, or bytes
type. In dense representation, this function also returns `True` if the
registers were updated. Otherwise it returns `False`.

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
