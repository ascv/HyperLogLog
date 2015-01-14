from HLL import HyperLogLog
from random import randint
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


class TestCardinalityEstimation(unittest.TestCase):

    def setUp(self):
        self.hll = HyperLogLog(5)

    def test_small_range_correction_all_registers_set_to_zero(self):
        self.assertEqual(self.hll.cardinality(), 0.0)

    def test_small_range_correction_not_all_registers_set_to_zero(self):
        self.hll.set_register(0, 1)
        c = self.hll.cardinality()
        used_correction= 1.46571806761 <= c and c <= 1.46571806762
        self.assertTrue(used_correction)

    def test_medium_range_no_correction(self):
        for i in range(32):
            self.hll.set_register(i, 2)

        c = self.hll.cardinality()
        no_correction = 89.216 <= c and c <= 89.217
        self.assertTrue(no_correction)
 
    def test_large_range_correction(self):
        hll = HyperLogLog(16)
        for i in range(hll.size()):
            hll.set_register(i, 16)

        c = hll.cardinality()
        used_correction = 7916284520 <= c and c <= 7916284521
        self.assertTrue(used_correction)
 
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
    
    def test_greater_than_the_maximum_size_is_invalid(self):
        with self.assertRaises(ValueError):
            HyperLogLog(17)           
                 
    def test_all_registers_initialized_to_zero(self):
        hll = HyperLogLog(5)
        registers = hll.registers()
        for register in registers:
            self.assertEqual(register, 0)

    def test_k_param_correctly_determines_the_number_of_registers(self):
        hll = HyperLogLog(5)
        self.assertEqual(len(hll.registers()), 32)
        self.assertEqual(hll.size(), 32)

    def test_seed_parameter_sets_seed(self):
        hll = HyperLogLog(5, seed=4)
        self.assertEqual(hll.seed(), 4)

        hll2 = HyperLogLog(5, seed=2)
        self.assertNotEqual(hll.murmur3_hash('test'), hll2.murmur3_hash('test'))

class TestMerging(unittest.TestCase):

    def test_only_same_size_HyperLogLogs_can_be_merged(self):
        hll = HyperLogLog(4)
        hll2 = HyperLogLog(5)
        with self.assertRaises(ValueError):
            hll.merge(hll2)
             
    def test_merge(self):
        expected = bytearray(4)
        expected[0] = 1
        expected[3] = 1

        hll = HyperLogLog(2)
        hll2 = HyperLogLog(2)

        hll.set_register(0, 1)
        hll2.set_register(3, 1)

        hll.merge(hll2)
        self.assertEqual(hll.registers(), expected)

class TestRegisterFunctions(unittest.TestCase):

    def setUp(self):
        self.k = 5
        self.hll = HyperLogLog(5)

    def test_set_last_register(self):
        self.hll.set_register(self.k - 1, 1)
        self.assertTrue(self.hll.registers()[self.k - 1] == 1)

    def test_set_first_register(self):
        self.hll.set_register(0, 1)
        self.assertTrue(self.hll.registers()[0] == 1)

    def test_set_register_with_negative_value_fails(self):
        with self.assertRaises(ValueError):
            self.hll.set_register(0, -1)

    def test_set_register_with_greater_than_max_rank_fails(self):
        with self.assertRaises(ValueError):
            self.hll.set_register(0, 33)

    def test_set_register_with_index_out_of_bounds(self):
        with self.assertRaises(IndexError):
            self.hll.set_register(33, 1)

    def test_set_register_with_negative_index_fails(self):
        with self.assertRaises(ValueError):
            self.hll.set_register(0, -1)

    def test_bytesarray_returned_from_registers_contains_correct_values(self):
        expected = bytearray(32)
        for i in range(32):
            expected[i] = randint(0, 16)

        for i in range(32):
            self.hll.set_register(i, expected[i])

        registers = self.hll.registers()
        for i in range(32):
            self.assertEqual(expected[i], registers[i])

    def test_registers_returns_bytesarray(self):
        self.assertTrue(type(self.hll.registers()) is bytearray)

    def test_registers_returns_correct_length_bytearray(self):
        self.assertTrue(len(self.hll.registers()) == pow(2, self.k))

if __name__ == '__main__':
    unittest.main()
