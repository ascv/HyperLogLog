The HyperLogLog algorithm [1] is a space efficient method to estimate the 
cardinality of extraordinarily large data sets. This module is written in C
using a Murmur3 hash [2] for python 2.7.x or python 3.x.

![Build Status](https://travis-ci.org/ascv/HyperLogLog.png?branch=master)
(https://travis-ci.org/ascv/HyperLogLog)

v 2.0.0


Quick start
===========

    from HLL import HyperLogLog

    hll = HyperLogLog(5) # use 2^5 registers
    hll.add('some data')
    hll.cardinality()

Setup
=====

You will need the python development package. On Ubuntu, you can install this
package using:

    sudo apt-get install python-dev

or:

    sudo apt-get install python3-dev

Now install using pip:

    pip install HLL

Alternatively, install using setup.py:

    python setup.py install


Documentation
=============

`add(data)`: adds *data* to the estimator where data is a string, buffer, or bytes
type. Returns `True` if the registers were updated otherwise returns `False`.

`HyperLogLog(k, seed=314)`: create a new HyperLogLog using `2^k` registers, *k*
must be in the range [2, 16]. Set `seed` to determine the seed value for the hash
function.

`merge(hll)`: merges another HyperLogLog into the current one. Merging compares
individual registers and takes the maximum value for each one.

`hash(data)`: hashes `data`, where `data` is a buffer, string, or bytes type,
using Murmur64A hash function and returns the result as an unsigned integer.

    size()

Gets the number of registers.

License
=======

This software is released under the [MIT License](LICENSE).

References
==========

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
[2] https://github.com/PeterScott/murmur3
