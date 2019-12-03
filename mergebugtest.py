import random
import hyperloglog
import time
import HLL

#K = [8, 12, 24, 32]
#N = [100, 1000, 100000, 1000000, ]
#K = [5,]
#N = [7,]
K = [8, 12, 24, 32]
N = [100, 1000, 100000, 1000000,]

for k in K:
    for n in N:
        hll = HLL.HyperLogLog(k)
        start = time.time()
        for i in xrange(n):
            hll.add(str(i))

        elapsed = time.time() - start
        relative_err = (hll.cardinality()-n)/n
        pct_err = relative_err*100

        msg = "k: {} n: {} RE: {}\tPctErr: {}\tCard: {}\tt: {}s".format(
            k,
            n,
            round(relative_err, 6),
            round(pct_err, 4),
            round(hll.cardinality()),
            round(elapsed, 6)
        )
        print(msg)
