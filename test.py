import pickle
import sys
import unittest

from HLL import HyperLogLog
from random import randint

class TestAdd(unittest.TestCase):

    def setUp(self):
        self.hll = HyperLogLog(5)

    def test_add_string(self):
        try:
            self.hll.add('my string')
        except Exception as ex:
            self.fail('failed to add string: %s' % ex)

    @unittest.skipIf(sys.version_info[0] > 2, 'buffer is deprecated in python 3.x')
    def test_add_buffer(self):
        try:
            self.hll.add(buffer('some characters'))
        except Exception as ex:
            self.fail('failed to add buffer: %s' % ex)

    def test_add_bytes(self):
        try:
            self.hll.add(b'some other characters')
        except Exception as ex:
            self.fail('failed to add bytes: %s' % ex)

    def test_return_value_indicates_register_update(self):
        changed = self.hll.add('asdf')
        self.assertTrue(changed)
        changed = self.hll.add('asdf')
        self.assertFalse(changed)
        changed = self.hll.add('otherdata')
        self.assertTrue(changed)


class TestHyperLogLogConstructor(unittest.TestCase):

    def test_size_lower_bound(self):
        for i in range(-1, 2):
            with self.assertRaises(ValueError):
                HyperLogLog(i)

    def test_size_upper_bound(self):
        with self.assertRaises(ValueError):
            HyperLogLog(64)

    def test_registers_initialized_to_zero(self):
        hll = HyperLogLog(randint(2, 16))
        for i in range(hll.size()):
            self.assertEqual(hll.get_register(i), 0)

    def test_histogram_initialized_with_correct_counts(self):
        k = randint(2, 10)
        hll = HyperLogLog(k)
        hist = hll._histogram()
        self.assertEqual(sum(hist), 2**k)

    def test_p_sets_size(self):
        for i in range(2, 6):
            hll = HyperLogLog(i)
            self.assertEqual(hll.size(), 2**i)

    def test_setting_a_seed(self):
        hll = HyperLogLog(5, seed=4)
        self.assertEqual(hll.seed(), 4)

        hll2 = HyperLogLog(5, seed=20000)
        self.assertNotEqual(hll.hash('test'), hll2.hash('test'))

class TestMerging(unittest.TestCase):

    def test_only_same_size_can_be_merged(self):
        with self.assertRaises(Exception):
            hll = HyperLogLog(4)
            hll.merge(HyperLogLog(5))

    def test_merging(self):
        k = randint(2, 8)

        hll_a = HyperLogLog(k)
        hll_b = HyperLogLog(k)

        for i in range(randint(10, 100)):
            hll_a.add(str(randint(0, 1024)))
            hll_b.add(str(randint(0, 1024)))

        hll_c = HyperLogLog(k)
        hll_c.merge(hll_a)
        hll_c.merge(hll_b)

        for i in range(2**k):
            max_fsb = max(hll_a.get_register(i), hll_b.get_register(i))
            self.assertEqual(max_fsb, hll_c.get_register(i))


class TestPickling(unittest.TestCase):

    def setUp(self):
        hlls = [HyperLogLog(x, randint(1, 10**6), sparse=False) for x in range(4, 16)]
        cardinalities = [x**5 for x in range(1, 16)]

        for hll, n in zip(hlls, cardinalities):
            for i in range(1, n):
                hll.add(str(i))
        self.dense_hlls = hlls

        hlls = [HyperLogLog(x, randint(1, 10**6), sparse=True) for x in range(4, 16)]
        cardinalities = [x**5 for x in range(1, 16)]

        for hll, n in zip(hlls, cardinalities):
            for i in range(1, n):
                hll.add(str(i))

        self.sparse_hlls = hlls

    def test_dense_pickled_cardinality(self):
        for hll in self.dense_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.cardinality(), hll2.cardinality())

    def test_dense_pickled_seed(self):
        for hll in self.dense_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.seed(), hll2.seed())

    def test_dense_pickled_register_histogram(self):
        for hll in self.dense_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll._histogram(), hll2._histogram())

    def test_dense_pickled_size(self):
         for hll in self.dense_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.size(), hll2.size())

    def test_sparse_pickled_cardinality(self):
        for hll in self.sparse_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.cardinality(), hll2.cardinality())

    def test_sparse_pickled_seed(self):
        for hll in self.sparse_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.seed(), hll2.seed())

    def test_sparse_pickled_register_histogram(self):
        for hll in self.sparse_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll._histogram(), hll2._histogram())

    def test_sparse_pickled_size(self):
         for hll in self.sparse_hlls:
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(hll.size(), hll2.size())

if __name__ == '__main__':
    unittest.main()
