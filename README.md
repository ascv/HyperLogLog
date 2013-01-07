## hll overview

hll is a demo implementation of the HyperLogLog algorithm for cardinality
estimation given in [1]. The demo returns the number of distinct words, 
reading up to 100 characters in a single word.

## Setup

    make

## Options

* -f the file to estimate on, if this is not set, then read from stdin
* -k use 2^k registers
* -s seed value for Murmur3

## Example usage

Using a file:

    ./hll -k 12 -s 44 -f somefile

Or reading from stdin:

    cat somefile | ./hll -k 12 -s 44

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
