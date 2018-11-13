from HLL import HyperLogLog
import random

hll1 = HyperLogLog(16)
hll2 = HyperLogLog(16)
for i in range(100000):
    print("\n{}".format(i))
    hll1.add(str(i))
print(hll1.cardinality())
#for i in range(100000):
#    hll2.add(str(random.random()))
#for i in range(100000):
#    r = str(random.random())
#    hll1.add(r)
#    hll2.add(r)
#
#print(hll1.cardinality())
#print(hll2.cardinality())
#
#hll1.merge(hll2)
#print(hll1.cardinality())
