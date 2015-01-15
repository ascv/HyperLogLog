from HLL import HyperLogLog
import cPickle as pickle1
import pickle as pickle2

hll = HyperLogLog(12,123)
for i in range(1000000): hll.add(str(i))

# result is ~1 million
print hll.cardinality()

hll2 = pickle1.loads(pickle1.dumps(hll))
print hll2.cardinality()

hll2 = pickle2.loads(pickle2.dumps(hll))
print hll2.cardinality()

hll2 = HyperLogLog(12,hll.seed())
for i,r in enumerate(hll.registers()): hll2.set_register(i,r)
print hll2.cardinality()

print "=" * 20
hll = HyperLogLog(2)
hll.set_register(0, 1)
hll.set_register(1, 1)
hll.set_register(2,1)
hll.set_register(3,1)

print "__reduce__"
print hll.__reduce__()

print "pickling..."
print hll.cardinality()
print "=" * 20
pickle2.dump(hll,open("save.p", "wb"))

print "unpickling..."
hll_new = pickle2.load(open("save.p", "rb"))
print hll_new.cardinality()
