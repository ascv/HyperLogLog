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

    def test_registers_returns_bytesarray(self):
        self.assertTrue(type(self.hll.registers()) is bytearray)

    def test_registesr_returns_correct_length_bytes_array(self):
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

    def setUp(self):
        """ """

    def test_non_power_of_2_size_is_invalid(self):
        """ """

    def test_negative_size_is_invalid(self):
        """ """

    def test_size_2_is_valid(self):
        """ """

    def test_all_registers_initialized_to_zero(self):
        """ """

    def test_size_param_correctly_determines_the_number_of_registers(self):
        """ """

    def test_seed_can_be_set(self):
        """ """

class TestMerging(unittest.TestCase):

    def setUp(self):
        """ """

    def test_only_same_size_HyperLogLogs_can_be_merged(self):
        """ """

    def test_merge(self):
        """ """

if __name__ == '__main__':
    unittest.main()
