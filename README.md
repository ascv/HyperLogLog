This is a python module, written in C, that defines the type HyperLogLog
which implements the HyperLogLog algorithm using a Murmur3 hash function.

v0.3

## Setup

    python setup build

## Quick start

    from HLL import HyperLogLog
    
    hll = HyperLogLog(5) # use 2^5 registers
    hll.add('some data')
    print hll.cardinality()
    
## Documentation

##### add(<i>data</i>)

Adds <i>data</i> to the estimator where <i>data</i> is a string, buffer, or memoryview.

##### HyperLogLog(<i>k [,seed])

Create a new HyperLogLog using 2^<i>k</i> registers. Set <i>seed</i> to determine the seed
value for the Murmur3 hash. The default value is 314.

##### merge(<i>HyperLogLog</i>)

Merges another HyperLogLog object with the current object. Merging compares the registers
of each object, setting the register of the current object to the maximum value. Only
the registers of the current object are affected, the registers of the merging object
are unaffected.

##### murmur3_hash(<i>data [,seed]</i>)

Gets a signed integer from a Murmur3 hash of <i>data</i> where <i>data</i> is a 
string, buffer, or memoryview. Set <i>seed</i> to determine the seed
value for the Murmur3 hash. The default value is objects default value.

##### registers()

Gets a bytearray of the registers.

##### seed()

Gets the seed value used in the Murmur3 hash.

##### size()

Gets the number of registers.

##### set_register(<i>index, value</i>)

Sets the register at <i>index</i> to <i>value</i>. Indexing is zero-based.
    
## License

This software is released under the [MIT License](https://gist.github.com/ascv/5123769).

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
