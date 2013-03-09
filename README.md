This is a python module, written in C, implementing the HyperLogLog algorithm using a Murmur3 hash function.

## Setup

    python setup build

## Usage

    from HLL import HyperLogLog
    
    hll = HyperLogLog(5) # create a counter with 2^5 registers
    hll.add('some data')
    print hll.cardinality()
    
## Documentation

todo
    
## License

This software is released under the [MIT License](https://gist.github.com/ascv/5123769)

## References

[1] http://algo.inria.fr/flajolet/Publications/FlFuGaMe07.pdf
