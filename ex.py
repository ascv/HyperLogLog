from HLL import HyperLogLog
import random
import pickle

SPARSE = True
P = 8

def test_merge_precision(should_pickle=False):
    random.seed(0)
    n_tests = 1000

    potential_values = [str(i) for i in range(100000)]
    chosen_values = set()
    aggregate_hll = HyperLogLog(p=P, seed=0, sparse=SPARSE)

    for test in n_tests:
        hll = HyperLogLog(p=P, seed=0, sparse=SPARSE)
        values = random.sample(potential_values, k=random.randint(0, 100))
        chosen_values.update(values)

        for value in values:
            hll.add(values)

def test_union_precision(pickleit=False):
    random.seed(0)
    union_count = 1
    candidate_values = [str(i) for i in range(100000)]
    picked_values = set()
    agg_hll = HyperLogLog(p=P, seed = 0, sparse=SPARSE)

    print('-'*80)
    s = 'PICKLING' if pickleit else 'NO PICKLING'
    print(s)
    print('-'*80)

    for i in range(union_count):
        hll = HyperLogLog(p=P, seed=0, sparse=SPARSE)
        values = random.sample(candidate_values, k=random.randint(0, 100))
        picked_values.update(values)

        for v in values:
            hll.add(v)

        # Do it anyway just to debug
        if not pickleit:
            a = pickle.dumps(hll)
            b = pickle.loads(a)

        if(pickleit):
            hll = pickle.loads(pickle.dumps(hll))

        agg_hll.merge(hll)

    n_picked = len(picked_values)

    cardinality = agg_hll.cardinality()
    print("> pickling {} merged cardinality: {}".format(pickleit, cardinality))
    print("> pickling {} true cardinality: {}".format(pickleit, cardinality))
    deviation = agg_hll.cardinality()/len(picked_values)
    return deviation

print("> pickling {} merged accuracy: {}".format(False, round(test_union_precision(pickleit=False), 4)))
print("> pickling {} merged accuracy: {}".format(True, round(test_union_precision(pickleit=True), 4)))
