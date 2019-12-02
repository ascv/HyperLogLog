import random
import hyperloglog
import time
import HLL

#K = [8, 12, 24, 32]
#N = [100, 1000, 100000, 1000000, ]
K = [5,]
N = [10,]

print("HLL")
print("===")
for k in K:
    for n in N:
        print('')
        print('k: {} n: {}'.format(k, n))
        print('')

        hll = HLL.HyperLogLog(k)
        start = time.time()
        for i in xrange(n):
            hll.add(str(i))

        elapsed = time.time() - start
        relative_err = (hll.cardinality()-n)/n
        pct_err = relative_err*100

        print('')
        print('')
        print('Relative error:\t{}'.format(round(relative_err, 6)))
        print('Percent error\t{}%'.format(round(pct_err, 4)))
        print('cardinality:\t{}'.format(round(hll.cardinality())))
        print('time: {}s'.format(round(elapsed, 6)))

print('')
