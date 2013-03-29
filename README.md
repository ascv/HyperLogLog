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

## Theory <a name='theory'></a>

This section is intended to provide a description of the HyperLogLog algorithm,
denoted HLL, and the intuition behind why it works. Before considering HLL, it 
is important to motivate the purpose of the algorithm by considering the problem 
it is designed to solve. Suppose we have some multi-set (a set that contains 
duplicate elements) whose data size is on the order of TB or PB. Furthermore,
suppose we wanted to estimate the cardinality of this set. Using a naive approach
we could scan the elements and hash them, storing each hash in memory or on disk.
The number of unique hashes is then the cardinality of the set. If the size of 
each hash is 8 bytes and the cardinality of set is 100 billion, then there
are 100 billion hashes occupying 8 * 10^11 bytes or approximately 754 GB. For 
practical purposes this space requirement is prohibitively inefficient. 

A more sophisticated approach might utilize linear counting, where each hash 
instead determines the location of some bit in a vector of zero bits. The bit
at this location is set to 1. After all the elements have been hashed and their 
associated bits set to 1 the cardinality can then be computed by simply counting 
the number of 1 bits in the vector. The space requirement is then the number of 
bits in the bit vector. Using the previous example, this would require 100 
billion bits or approximately 12 GB. This space requirement is still prohibitive 
for many applications. 

HLL relies on making observations in the underlying bit-patterns of the elements 
in the dataset whose cardinality we wish to estimate. As an explanatory example, 
we will consider an 8-bit case. Suppose h(x) is a hash function that randomly 
distributes the bits of x with equal probability. Then a hashed element might 
have the following distribution of bits:
  
|  0  | 0  | 0  | 0  | 1  | 1  | 0  | 1  |
| --- |:--:|:--:|:--:|:--:|:--:|:--:| --:|

Note the position of the first 1 bit, from left to right. This is known as the
rank. In the example above, the first 1 bit is in the fifth position, indexed 
from the left, so the rank is 5. Out of all the numbers that can be formed on 
8 bits, what is the probability of a number having rank 5? If we interpret the 
sequence of bits as a sequence of coin flips where 0 is a tails, 1 is a heads 
and k is the rank (the number of independent flips required to observe a heads) 
then the rank is geometrically distributed according to:

    P(X=k) = p^(k-1) * (1 - p)

Since each bit has equal probability p=1/2 so:

    P(X=k) = (1/2)^(k-1) * (1/2)
	
	P(X=k) = 1/2^k
	
Then for the case of rank 5: 

    P(X=5) = 1/2^5

Out of all of the possible numbers on 8 bits, how many might we expect to have 
rank 5? Note P(X=5) is equivalent to:

    P(X=5) = (# of rank 5 numbers) / (# of possible numbers on 8 bits)
    
Then the number of elements of rank 5 is:

    1/2^5 = (# of rank 5 numbers) / (2^8)

    (# rank 5 numbers) = 2^8 / 2^5  = 2^3 = 8

So we might expect 8 elements to have rank 5 and indeed this is the case. All of
the rank 5 numbers are given below:

    00001000
    00001001
    00001010
    00001011
    00001100
    00001101
    00001111

More generally, if there are n distinct elements and r is the number of elements 
with rank k, then we would expect about n/2^k of the hashed elements to have 
rank k. Moreover,

    P(X=k) ~ r / n	
	
    n ~  r / P(X=k)
	
    n ~  r * 2^k
	
    log_2(n) ~ k + log2(r)
	
In other words, if M contains the hashed elements of a multi-set of unknown 
cardinality, n is the true cardinality of M, and R is the maximum rank amongst the 
elements of M, then R provides a rough estimation of log_2(n) with some additive bias. 
Notice that the expectation of 2^R is infinite so 2^R cannot used to estimate n. 

Furthermore using only a single observable can be misleading. For example, suppose 
all the elements of M have the same hash. This implies that these elements are
probably not distinct. However the rank of these elements may be very large so
using the expression for log_2(n) would produce wildly inaccurate results.

Rather than take the maximum rank amongst all the elements of M, HLL divides M 
into m buckets, takes the maximum rank of each bucket. It follows that for each 
bucket, we have an estimate of log_2(n/m). These results are averaged using 
a harmonic mean and then multiplied by a constant to reduce bias (see [1] 
for discussion of the constant). The HLL algorithm is given by the 
following pseudocode:

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
