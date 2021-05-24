
![Build Status](https://travis-ci.org/ascv/HyperLogLog.png?branch=master)
(https://travis-ci.org/ascv/HyperLogLog)

v 2.0.1

Overview
========

The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module is written in C
for python >= 3.6 and python 2.7.x. It implements a 64 bit version [2] of
HyperLogLog using a Murmur64A hash.

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

HyperLogLogs estimate the cardinality of a multi-set. The estimation power is
proportional to the number of registers which is controlled by the parameter
`k` using the formula `2^p`:
```
>>> from hll import HyperLogLog
>>> hll = HyperLogLog(p=3) # Use 2^3=8 registers
>>> hll.size()
8
>>> for data in ['one', 'two', 'three', 'four',]:
...     hll.add(data)
>>> hll.cardinality()
4L
```
The default number of registers, `k=12`, allows for accurate estimation of
a large range of cardinalities which is suitable for most applications.

HyperLogLogs use a Murmur64A hash. This hash function is fast and has a good
uniform distribution of bits. The seed to this hash function can be set:
```
>>> hll = HyperLogLog(k=2, seed=123456789)
>>> hll.seed()
12345679
```

The hash function can also be called directly:
```
>>> hll.hash('something')
393810339
```

HyperLogLogs can be merged. This is done by taking the maximum value of the
respective registers:
```
>>> red = HyperLogLog(k=4)
>>> red.add('hello')
>>> blue = HyperLogLog(k=4)
>>> blue.add('world')
>>> red.merge(blue)
>>> red.cardinality()
2
```

Individual registers can also be printed:
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

When a HyperLogLog is created all the registers are initialized to zero.
Storing all these zero value registers indvidually is wasteful. Instead a
sparse representation is used where only non-zero registers are stored in a
sorted linked list [3]. When this list reaches sufficient size the HyperLogLog
switches to a dense representation where registers are then stored
individually using 6 bits.

Sparse representation can be disabled using the `sparse` flag:
```
>>>> HyperLogLog(p=2, sparse=False)
```

The maximum list size, which determines when the switch from sparse to dense
representation occurs, can be set using `max_list_size`:
```
>>>> HyperLogLog(p=2, max_list_size=100)
```

To avoid the expense of traversing the list every time `add()` is called a
temporary only buffer is used. Items added to the `HyperLogLog` are first
added to the temporary buffer. When the buffer is full the items are sorted
and then the registers in the linked list are updated in one pass since both
the buffer and linked list can be traversed at the same time.

The buffer size can be set using `max_buffer_size`:
```
>>> HyperLogLog(p=2, max_buffer_size=10)
```
Note that some methods will cause the buffer to be cleared if the registers
are using the sparse representation. This is because these methods require
access to the current register values. Methods that will clear the buffer
are: `cardinality()`, `merge()`, and if the HyperLogLog is pickled e.g.
`pickle.dumps()`.

This can be helpful when the number of registers is very small or when the
expected cardinality is large enough that it is pointless to engage in
additional space saving techniques.

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
    Engineering of a State of the Art Cartdinality Estimation Algorithm,"
    Proceedings of the EDBT 2013 Conference, ACM, Genoa March 2013.
