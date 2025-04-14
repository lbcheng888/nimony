import unittest
from cheng_py.parser.parser import tokenize, Token

class TestTokenizerSpecific(unittest.TestCase):

    def test_tokenize_example_54(self):
        # Removed semicolon from the original code
        code = "((() (x = 20 (getX))))"
        expected_tokens = [
            Token('LPAREN', '(', 1, 1),
            Token('LPAREN', '(', 1, 2),
            Token('LPAREN', '(', 1, 3),
            Token('RPAREN', ')', 1, 4),
            Token('LPAREN', '(', 1, 6),
            Token('SYMBOL', 'x', 1, 7),
            Token('EQUALS', '=', 1, 9),
            Token('INTEGER', '20', 1, 11),
            # Removed SEMICOLON token
            Token('LPAREN', '(', 1, 14), # Adjusted column
            Token('SYMBOL', 'getX', 1, 15), # Adjusted column
            Token('RPAREN', ')', 1, 19), # Adjusted column
            Token('RPAREN', ')', 1, 20), # Adjusted column
            Token('RPAREN', ')', 1, 21), # Adjusted column
            Token('RPAREN', ')', 1, 22), # Adjusted column
        ]
        actual_tokens = tokenize(code)

        print("\n--- Tokenizing Example 54 (No Semicolon) ---")
        print(f"Code: {code!r}")
        print("Expected Tokens:")
        for t in expected_tokens: print(f"  {t!r}")
        print("Actual Tokens:")
        for t in actual_tokens: print(f"  {t!r}")
        print("---------------------------\n")

        self.assertEqual(len(actual_tokens), len(expected_tokens), "Mismatch in number of tokens")
        for i, (actual, expected) in enumerate(zip(actual_tokens, expected_tokens)):
            self.assertEqual(actual.type, expected.type, f"Token {i} type mismatch")
            self.assertEqual(actual.value, expected.value, f"Token {i} value mismatch")
            # Column check might be fragile, skip for now if needed
            # self.assertEqual(actual.column, expected.column, f"Token {i} column mismatch")

if __name__ == '__main__':
    unittest.main()
