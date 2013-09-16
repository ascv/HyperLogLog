The HyperLogLog algorithm [1] is a space efficient method to estimate the
cardinality of extraordinarily large data sets. This module provides an
implementation, written in C using a Murmur3 hash, for python 2.7.x or python 3.x. 

v0.72

## Setup ##

You will need the python development package. On Ubuntu/Mint
you can install this package using:

    sudo apt-get install python-dev

Now install using setup.py:

    sudo python setup.py install

## Quick start ##

    from HLL import HyperLogLog
    
    hll = HyperLogLog(5) # use 2^5 registers
    hll.add('some data')
    estimate = hll.cardinality()
  
## Documentation ##

##### HyperLogLog(<i>k [,seed]) #####

Create a new HyperLogLog using 2^<i>k</i> registers, <i>k</i> must be in the 
range [2, 16]. Set <i>seed</i> to determine the seed value for the Murmur3 
hash. The default value is 314.

* * *

##### add(<i>data</i>)

Adds <i>data</i> to the estimator where <i>data</i> is a string or buffer. 


##### merge(<i>HyperLogLog</i>)

Merges another HyperLogLog object with the current object. Merging compares the 
registers of each object, setting the register of the current object to the 
maximum value. Only the registers of the current object are affected, the 
registers of the merging object are unaffected.

##### murmur3_hash(<i>data [,seed]</i>)

Gets a signed integer from a Murmur3 hash of <i>data</i> where <i>data</i> is a 
string, buffer, or memoryview. Set <i>seed</i> to determine the seed
value for the Murmur3 hash. The default seed is HyperLogLog's default seed.

##### registers()

Gets a bytearray of the registers.

##### seed()

Gets the seed value used in the Murmur3 hash.

##### set_register(<i>index, value</i>)

Sets the register at <i>index</i> to <i>value</i>. Indexing is zero-based.

##### size()

Gets the number of registers.

## License

This software is released under the [MIT License](https://gist.github.com/ascv/5123769).

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
