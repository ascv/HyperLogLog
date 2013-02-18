This is a demonstration of the HyperLogLog algorithm using a Murmur3 hash function. 
While HLL operates independently of the encoding (e.g. it is irrelevant whether elements in the 
input data set are text files or images) this program operates only on strings and reads up to 100 
characters for any individual string from the input data set. In other words, it returns the number
of distinct words in the input data set. 

## Setup

    make

## Options

* -f the file to estimate on, if this is not set, then read from stdin
* -k use 2^k registers to store the rank (number of leading zeros)
* -s seed value for Murmur3 (defaults to 42)

## Example usage

Using a file:

    ./hll -k 12 -f sometextfile

Or reading from stdin:

    cat somefile | ./hll -k 12 -s 44

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
