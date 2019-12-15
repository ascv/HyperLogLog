from HLL import HyperLogLog
from random import randint
import pickle
import unittest
import sys

class TestAdd(unittest.TestCase):

    def setUp(self):
        self.hll = HyperLogLog(5)

    def test_add_string(self):
        try:
            self.hll.add('asdf')
        except Exception as ex:
            self.fail('failed to add string: %s' % ex)

    @unittest.skipIf(sys.version_info[0] > 2, 'buffer is deprecated in python 3.x')
    def test_add_buffer(self):
        try:
            self.hll.add(buffer('asdf'))
        except Exception as ex:
            self.fail('failed to add buffer: %s' % ex)

    def test_add_bytes(self):
        try:
            self.hll.add(b'asdf')
        except Exception as ex:
            self.fail('failed to add bytes: %s' % ex)

class TestHyperLogLogConstructor(unittest.TestCase):

    def test_one_is_invalid_size(self):
        with self.assertRaises(ValueError):
            HyperLogLog(0)

    def test_negative_size_is_invalid(self):
        with self.assertRaises(ValueError):
            HyperLogLog(-1)

    def test_minimum_size_is_valid(self):
        try:
            HyperLogLog(2)
        except Exception:
            self.fail()

    def test_maximum_size_is_valid(self):
        try:
            HyperLogLog(16)
        except Exception:
            self.fail()

    @unittest.skip('fix')
    def test_greater_than_the_maximum_size_is_invalid(self):
        with self.assertRaises(ValueError):
            HyperLogLog(17)

    def test_all_registers_initialized_to_zero(self):
        hll = HyperLogLog(5)
        #registers = hll.registers()
        #for register in registers:
        #    self.assertEqual(register, 0)

    def test_k_param_determines_the_number_of_registers(self):
        hll = HyperLogLog(5)
        #self.assertEqual(len(hll.registers()), 32)
        #self.assertEqual(hll.size(), 32)

    def test_seed_parameter_sets_seed(self):
        hll = HyperLogLog(5, seed=4)
        self.assertEqual(hll.seed(), 4)

        hll2 = HyperLogLog(5, seed=20000)
        self.assertNotEqual(hll.hash('test'), hll2.hash('test'))

class TestMerging(unittest.TestCase):

    def test_only_same_size_HyperLogLogs_can_be_merged(self):
        hll = HyperLogLog(4)
        hll2 = HyperLogLog(5)
        with self.assertRaises(Exception):
            hll.merge(hll2)

    def test_merge(self):
        #expected = bytearray(4)
        #expected[0] = 1
        #expected[3] = 1

        hll = HyperLogLog(2)
        hll2 = HyperLogLog(2)

        #hll.set_register(0, 1)
        #hll2.set_register(3, 1)

        hll.merge(hll2)
        #self.assertEqual(hll.registers(), expected)

class TestPickling(unittest.TestCase):

    def setUp(self):
        hlls = [HyperLogLog(x, randint(1, 1000)) for x in range(4, 16)]
        cardinalities = [x**5 for x in range(1, 16)]

        for hll, n in zip(hlls, cardinalities):
            for i in range(1, n):
                hll.add(str(i))
        self.hlls = hlls

    def test_pickled_cardinality(self):
        for hll in self.hlls:
            expected = hll.cardinality()
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(expected, hll2.cardinality())

    def test_pickled_seed(self):
        for hll in self.hlls:
            expected = hll.seed()
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(expected, hll2.seed())

    def test_pickled_register_histogram(self):
        for hll in self.hlls:
            expected = hll._histogram()
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(expected, hll2._histogram())

    def test_pickled_size(self):
         for hll in self.hlls:
            expected = hll.size()
            hll2 = pickle.loads(pickle.dumps(hll))
            self.assertEqual(expected, hll2.size())

if __name__ == '__main__':
    unittest.main()
