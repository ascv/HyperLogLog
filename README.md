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