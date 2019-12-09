import random
import time
import HLL

def print_registers(h):
    regs = '|'
    for i in range(h.size()):
        regs += '{}|'.format(h._get_register(i))
    print(regs)

K = [24,]
N = [5, 50, 500, 5000, 50000, 500000,]

msg = "{: <3}{: <16}{: <24}{: <12}{: <12}{: >8}"
print(msg.format('k', 'n', 'card', 'RE', '% err', 'Time'))
print(msg.format('=', '=', '====', '==', '=====', '===='))

for k in K:
    for n in N:
        hll = HLL.HyperLogLog(k)
        start = time.time()
        #for i in xrange(n):
        for i in range(n):
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

h1 = HLL.HyperLogLog(3)
h2 = HLL.HyperLogLog(3)

print('H1:')
h1.add('one')
h1.add('two')
h1.add('one million')
print_registers(h1)
print(h1.cardinality())
print('H2:')
h2.add('one million')
print_registers(h2)
print(h2.cardinality())
print('MERGE:')
h1.merge(h2)
print_registers(h1)
print(h1.cardinality())

print(h1._histogram())
