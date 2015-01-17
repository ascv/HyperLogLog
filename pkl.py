from HLL import HyperLogLog
import cPickle as pickle

n = 10  # however, set n to 100000 and this code prints that pickle works
#n = 100000
hll1 = HyperLogLog(5, seed=123)
for i in range(n): hll1.add(str(i*i))
hll2 = pickle.loads(pickle.dumps(hll1))

print "__reduce__()"
print hll1.__reduce__()
print hll2.__reduce__()
r1 = hll1.__reduce__()[2]
r2 = hll2.__reduce__()[2]
print r1 == r2

print ""
print "registers:"
r1 = hll1.registers()
r2 = hll2.registers()
print map(int, r1)
print map(int, r2)
print r1 == r2

print ""
print "cardinalities:"
cardinality1 = hll1.cardinality()
cardinality2 = hll2.cardinality()

print cardinality1
print cardinality2
print cardinality1 == cardinality2
