# tests/cheng_py/test_ast.py

import unittest
# 使用相对于项目根目录的导入路径
from cheng_py.core.ast import (
    Symbol, Integer, Float, Boolean, String, Pair, Nil
)

class TestAstNodes(unittest.TestCase):

    def test_symbol(self):
        s1 = Symbol("abc")
        s2 = Symbol("abc")
        s3 = Symbol("def")
        self.assertEqual(repr(s1), "Symbol('abc')")
        self.assertEqual(s1, s2)
        self.assertNotEqual(s1, s3)
        self.assertEqual(hash(s1), hash(s2))
        self.assertNotEqual(hash(s1), hash(s3))

    def test_integer(self):
        i1 = Integer(123)
        i2 = Integer(123)
        i3 = Integer(456)
        self.assertEqual(repr(i1), "Integer(123)")
        self.assertEqual(i1, i2)
        self.assertNotEqual(i1, i3)
        self.assertEqual(hash(i1), hash(i2))
        self.assertNotEqual(hash(i1), hash(i3))

    def test_float(self):
        f1 = Float(1.23)
        f2 = Float(1.23)
        f3 = Float(4.56)
        self.assertEqual(repr(f1), "Float(1.23)")
        self.assertEqual(f1, f2)
        self.assertNotEqual(f1, f3)
        self.assertEqual(hash(f1), hash(f2))
        self.assertNotEqual(hash(f1), hash(f3))

    def test_boolean(self):
        b_true1 = Boolean(True)
        b_true2 = Boolean(True)
        b_false = Boolean(False)
        self.assertEqual(repr(b_true1), "Boolean(True)")
        self.assertEqual(repr(b_false), "Boolean(False)")
        self.assertEqual(b_true1, b_true2)
        self.assertNotEqual(b_true1, b_false)
        self.assertEqual(hash(b_true1), hash(b_true2))
        self.assertNotEqual(hash(b_true1), hash(b_false))

    def test_string(self):
        str1 = String("hello")
        str2 = String("hello")
        str3 = String("world")
        self.assertEqual(repr(str1), "String('hello')")
        self.assertEqual(str1, str2)
        self.assertNotEqual(str1, str3)
        self.assertEqual(hash(str1), hash(str2))
        self.assertNotEqual(hash(str1), hash(str3))

    def test_nil(self):
        n1 = Nil
        n2 = Nil
        self.assertEqual(repr(n1), "Nil")
        self.assertEqual(n1, n2) # Singleton
        self.assertEqual(hash(n1), hash(n2))
        self.assertNotEqual(n1, Symbol("Nil"))

    def test_pair_list(self):
        # (1 2 3)
        p1 = Pair(Integer(1), Pair(Integer(2), Pair(Integer(3), Nil)))
        p2 = Pair(Integer(1), Pair(Integer(2), Pair(Integer(3), Nil)))
        # (1 2 . 3) - Dotted pair
        p3 = Pair(Integer(1), Pair(Integer(2), Integer(3)))
        # (1)
        p4 = Pair(Integer(1), Nil)

        self.assertEqual(repr(p1), "(Integer(1) Integer(2) Integer(3))")
        self.assertEqual(repr(p3), "(Integer(1) Integer(2) . Integer(3))")
        self.assertEqual(repr(p4), "(Integer(1))")
        self.assertEqual(p1, p2)
        self.assertNotEqual(p1, p3)
        self.assertNotEqual(p1, p4)

    def test_nested_list(self):
        # ((1 2) 3)
        inner = Pair(Integer(1), Pair(Integer(2), Nil))
        outer = Pair(inner, Pair(Integer(3), Nil))
        self.assertEqual(repr(outer), "((Integer(1) Integer(2)) Integer(3))")

if __name__ == '__main__':
    unittest.main()
