The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module provides an
implementation, written in C using a Murmur3 hash, for python 2.7.x or 
python 3.x.

[![Build Status](https://travis-ci.org/ascv/HyperLogLog.png?branch=master)]
(https://travis-ci.org/ascv/HyperLogLog)

v0.96

Setup
=====

You will need the python development package. On Ubuntu/Mint
you can install this package using:

    sudo apt-get install python-dev

Now install using pip:
    
    sudo pip install HLL

Alternatively, install using setup.py:

    sudo python setup.py install

Quick start
===========

    from HLL import HyperLogLog
    
    hll = HyperLogLog(5) # use 2^5 registers
    hll.add('some data')
    estimate = hll.cardinality()
  
Documentation
=============

    add(data)

Adds *data* to the estimator where data is a string, buffer, or bytes
type.

    HyperLogLog(k, seed=314)

Create a new HyperLogLog using 2^*k* registers, *k* must be in the 
range [2, 16]. Set *seed* to determine the seed value for the Murmur3 
hash. The default value was chosen arbitrarily.

    merge(hll)

Merges another HyperLogLog into the current one. Merging compares individual
registers and takes the maximum value for each one. The registers of the other
HyperLogLog are unaffected.

    murmur3_hash(data, seed=314)

Gets a signed integer from a Murmur3 hash of *data* where *data* is a 
string, buffer, or bytes (python 3.x). Set *seed* to determine the seed
value for the Murmur3 hash. The default value was chosen arbitrarily.

    registers()

Gets a bytearray of the registers.

    seed()

Gets the seed value used in the Murmur3 hash.

    set_register(index, value)

Sets the register at *index* to *value*. Indexing is zero-based.

    size()

Gets the number of registers.

License
=======

This software is released under the [MIT License](https://gist.github.com/ascv/5123769).

References
==========

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
