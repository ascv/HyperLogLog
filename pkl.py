from HLL import HyperLogLog
import binascii
import pickle

hll = HyperLogLog(2)
hll.set_register(0, 1)
hll.set_register(1, 1)
hll.set_register(2,1)
hll.set_register(3,1)

print hll.__reduce__()

print "pickling..."
print hll.cardinality()
print binascii.hexlify(hll.registers())
print hll.size()
print "="*20
pickle.dump(hll, open( "save.p", "wb"))

print "unpickling..."
hll_new = pickle.load(open( "save.p", "rb"))
print hll_new.cardinality()
print binascii.hexlify(hll_new.registers())
print hll_new.size()
