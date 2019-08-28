#!/usr/bin/env python
#coding:utf-8
# Author:  mozman
# Purpose: test binary trees
# Created: 28.04.2010
# Copyright (c) 2010-2013 by Manfred Moitzi
# License: MIT License


import sys
PYPY = hasattr(sys, 'pypy_version_info')

import unittest
from random import randint, shuffle

if not PYPY:
    from bintrees.cython_trees import FastRBTree

@unittest.skipIf(PYPY, "Cython implementation not supported for pypy.")
class TestTree(unittest.TestCase):
    values = [(2, 12), (4, 34), (8, 45), (1, 16), (9, 35), (3, 57)]
    keys = [2, 4, 8, 1, 9, 3]

    def test_create_tree(self):
        tree = FastRBTree()
        self.assertEqual(tree.count, 0)
        tree.update(self.values)
        self.assertEqual(tree.count, 6)

    def test_properties(self):
        tree = FastRBTree(self.values)
        self.assertEqual(tree.count, 6)

    def test_clear_tree(self):
        tree = FastRBTree(self.values)
        tree.clear()
        self.assertEqual(tree.count, 0)

    def test_insert(self):
        tree = FastRBTree()
        for key in self.keys:
            tree.insert(key, key)
            value = tree.get_value(key)
            self.assertEqual(value, key)
        self.assertEqual(tree.count, 6)

    def test_remove(self):
        tree = FastRBTree(self.values)
        for key in self.keys:
            tree.remove(key)
            self.assertRaises(KeyError, tree.get_value, key)
        self.assertEqual(tree.count, 0)

    def test_remove_random_numbers(self):
        keys = list(set([randint(0, 10000) for _ in range(500)]))
        shuffle(keys)
        tree = FastRBTree(zip(keys, keys))
        self.assertEqual(tree.count, len(keys))
        for key in keys:
            tree.remove(key)
        self.assertEqual(tree.count, 0)


if __name__ == '__main__':
    unittest.main()
