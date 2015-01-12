from HLL import HyperLogLog
import binascii
import pickle

hll = HyperLogLog(5)
hll.add('hello')
hll.add('world')

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
