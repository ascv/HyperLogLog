The HyperLogLog algorithm [1] provides a space efficient means to estimate the
cardinality of extraordinarily large data sets. This module provides an
implementation, written in C, for python 2.7.3 or python 3.x. See 
<a href="#theory">Theory</a> for an introduction to the algorithm.

v0.6

## Setup

    python setup build

## Quick start

    from HLL import HyperLogLog
    
    hll = HyperLogLog(5) # use 2^5 registers
    hll.add('some data')
    estimate = hll.cardinality()
  
## Documentation

##### HyperLogLog(<i>k [,seed])

Create a new HyperLogLog using 2^<i>k</i> registers, <i>k</i> must be in the 
range [2, 16]. Set <i>seed</i> to determine the seed value for the Murmur3 
hash. The default value is 314.

* * *

##### add(<i>data</i>)

Adds <i>data</i> to the estimator where <i>data</i> is a string, buffer, or 
memoryview.

##### merge(<i>HyperLogLog</i>)

Merges another HyperLogLog object with the current object. Merging compares the 
registers of each object, setting the register of the current object to the 
maximum value. Only the registers of the current object are affected, the 
registers of the merging object are unaffected.

##### murmur3_hash(<i>data [,seed]</i>)

Gets a signed integer from a Murmur3 hash of <i>data</i> where <i>data</i> is a 
string, buffer, or memoryview. Set <i>seed</i> to determine the seed
value for the Murmur3 hash. The default value is objects default value.

##### registers()

Gets a bytearray of the registers.

##### seed()

Gets the seed value used in the Murmur3 hash.

##### set_register(<i>index, value</i>)

Sets the register at <i>index</i> to <i>value</i>. Indexing is zero-based.

##### size()

Gets the number of registers.

## Algorithm

```
Let h: D --> [0, 1] = {0, 1}^32; // hash data from domain D to 32-bit words
Let p(s) be the position of the leftmost 1-bit of s; // e.g. p(001...) = 3, p(0^k) = k + 1
Define a_16 = .673, a_32 = .697, a_64 = .709; a_m = .7213/(1 + 1.079/m) for m >= 128;

Algorithm HYPERLOGLOG(input M: a multiset of items from domain D)
    assume m = 2^b with b in [4 .. 16];
    initialize a collection of m registers, M[1], ..., M[m], to 0;
	
	for v in M do
	    set x := h(v);
		set j := 1 + <x_1 x_2 ... x_b}_2; // the binary address determined by the first b bits of x
		set w := <x_b+1 x_b+2 ... >; // the remaining bits of x
		set M[j] := max(M[j], p[w]);
		
	Z := 0;
	for j := 1 to m do
	     Z := Z + 2^-M[j];
	
	Z := 1/Z;
	E := a_m * m^2 * Z; // raw HLL estimate
	
	if E <= 5/2 * m then
		let V be the number of registers equal to 0;
		if V != 0 then set E* := m*log(m/V) else set E* := E // small range correction
			
	if E < 1/30 * 2^32 then
		set E* := E // intermediate range - no correction
	
	if E > 1/30 * 2^32 then
		set E* := -2^32 * log(1 - E/2^32) // large range correction
	
	return cardinality estimate E* with typical relative error +/- 1.04/m^(1/2)
```
    
## License

This software is released under the [MIT License](https://gist.github.com/ascv/5123769).

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
