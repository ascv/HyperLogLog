This is a demonstration of the HyperLogLog algorithm implemented in C as a command line program. 
HLL can space efficiently estimate the cardinality of very large data set. While HLL operates
independently of the encoding, e.g. it is irrelevant whether elements in the input data set are
text files or images, this program operates only on strings reading up to 100 characters for any
individual string in the input data set.

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
