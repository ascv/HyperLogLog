import random
import time
import HLL

K = [24,]
N = [500000,] 


msg = "{: <3}{: <16}{: <24}{: <12}{: <12}{: >8}"
print(msg.format('k', 'n', 'card', 'RE', '% err', 'Time'))
#print(msg.format('-', '-', '----', '--', '-----', '----'))
print(msg.format('=', '=', '====', '==', '=====', '===='))
for k in K:
    for n in N:
        hll = HLL.HyperLogLog(k)
        start = time.time()
        for i in xrange(n):
        #for i in range(n):
            hll.add(str(i))

        elapsed = time.time() - start
        relative_err = abs((hll.cardinality()-n)/n)
        pct_err = abs(relative_err*100)

        out = msg.format(
            k,
            n,
            round(hll.cardinality()),
            round(relative_err, 4),
            round(pct_err, 4),
            round(elapsed, 4)
        )

        print(out)
