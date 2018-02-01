
import unittest

from ..intervals import IntervalTree, Interval

class IntervalTests(unittest.TestCase):

    def setUp(self):
        self.tree = IntervalTree()

    def test_find_containing(self):
        i1_10 = self.tree.add_interval(1, 10)
        i1_5 = self.tree.add_interval(1, 5)
        i1_2 = self.tree.add_interval(1, 2)
        i3_4 = self.tree.add_interval(3, 4)
        i7_8 = self.tree.add_interval(7, 8)

        self.assertIs(self.tree.find_interval(1), i1_2)
        self.assertIs(self.tree.find_interval(2), i1_2)
        self.assertIs(self.tree.find_interval(3), i3_4)
        self.assertIs(self.tree.find_interval(4), i3_4)
        self.assertIs(self.tree.find_interval(5), i1_5)
        self.assertIs(self.tree.find_interval(6), i1_10)
        self.assertIs(self.tree.find_interval(7), i7_8)
        self.assertIs(self.tree.find_interval(8), i7_8)
        self.assertIs(self.tree.find_interval(9), i1_10)
        self.assertIs(self.tree.find_interval(10), i1_10)
