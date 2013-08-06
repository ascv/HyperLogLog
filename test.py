from HLL import HyperLogLog
import unittest

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
        """ """

    def test_set_register_with_greater_than_max_rank_fails(self):
        """ """

    def test_set_register_with_index_greater_than_size_fails(self):
        """ """

    def test_set_register_with_negative_index_fails(self):
        """ """

    def test_bytesarray_returned_from_registers_contains_correct_values(self):
        """ """

    def test_registers_returns_bytesarray(self):
        self.assertTrue(type(self.hll.registers()) is bytearray)

    def test_registesr_returns_correct_length_bytearray(self):
        self.assertTrue(len(self.hll.registers()) == pow(2, self.k))

class TestCardinalityEstimation(unittest.TestCase):

    def setUp(self):
        """ """

    def test_add_adds_an_element_to_a_random_register(self):
        """ """

    def test_small_range_correction(self):
        """ """

    def test_medium_range_no_correction(self):
        """ """

    def test_large_range_correction(self):
        """ """

    def test_the_larger_rank_is_used_when_comparing_elements(self):
        """ """

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

if __name__ == '__main__':
    unittest.main()

