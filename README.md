This is a demonstration of the HyperLogLog algorithm implemented in C as a command line program. 
While HLL operates independently of the encoding, e.g. it is irrelevant whether elements in the 
input data set aretext files or images, this program operates only on strings and reads up to 100 
characters for any individual string from the input data set. In other words, it returns the number
of distinct words in the input data set.

## Setup

    make

## Options

* -f the file to estimate on, if this is not set, then read from stdin
* -k use 2^k registers to store the rank (maximum 
* -s seed value for Murmur3

## Example usage

Using a file:

    ./hll -k 12 -s 44 -f somefile

Or reading from stdin:

    cat somefile | ./hll -k 12 -s 44

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
