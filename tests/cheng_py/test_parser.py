# tests/cheng_py/test_parser.py

import unittest
import re # Import re for cleaning multiline strings
from cheng_py.parser.parser import parse, tokenize, Token # Remove SyntaxError import
from cheng_py.core.ast import (
    Symbol, Integer, Float, Boolean, String, Nil, ChengExpr, ChengAstNode,
    Param, LambdaExpr, DefineMacroExpr, AssignmentExpr, ApplyExpr, QuoteExpr,
    PrefixExpr, BinaryExpr, IndexExpr, SliceExpr, ListLiteralExpr, SequenceExpr,
    IfExpr, QuasiquoteExpr, UnquoteExpr, UnquoteSplicingExpr
)
# Import examples from the consolidated file
from .all_test_examples import all_examples

class TestNewParserFromExamples(unittest.TestCase):

    # Keep assertParseError for potential future use with specific error cases
    def assertParseError(self, text: str, expected_message_part: str = None):
        """Helper to assert parsing raises SyntaxError"""
        with self.assertRaises(SyntaxError) as cm:
            # Tokenization errors should also be caught here if they occur
            parse(text)
        if expected_message_part:
            self.assertIn(expected_message_part, str(cm.exception),
                          f"Input: {text}\nExpected error containing: '{expected_message_part}'\nGot error: '{cm.exception}'")

    def test_parse_valid_examples(self):
        """
        Tests that all valid examples from all_test_examples.py parse without raising SyntaxError.
        Does not check the resulting AST structure, only that parsing succeeds.
        """
        for i, example in enumerate(all_examples): # Iterate over the list of dicts
            code_to_parse = example['code'] # Access the 'code' key
            reason = example.get('reason', '') # Access reason from dict
            # Clean up potential multiline strings.
            # Rely on the tokenizer to handle comments correctly.
            # Just strip leading/trailing whitespace.
            code_to_parse = code_to_parse.strip()

            # Skip empty strings after cleaning (e.g., if original was just comments/whitespace)
            if not code_to_parse:
                continue

            # Skip known error cases from the old parser tests for now
            # These might need specific assertParseError tests later if they are still errors
            known_error_patterns = [
                r'^\($', # Unmatched open paren
                r'^\(1 2 3$', # Unmatched open paren
                r'^\)$', # Unmatched close paren
                r'^\(1 2\) 3$', # Extra token
                r'^1 2$', # Extra token
                # r'^`=$', # Invalid prefix = (removed)
                # r'^&\)$', # Invalid use of & (removed)
                # r'^& 1 2\)$', # Invalid use of & (removed)
                r'^1 \+$', # Incomplete binary
                # r'^\+ 2$', # Invalid prefix + (removed)
                r'^x =$', # Incomplete assignment
            ]
            is_known_error = any(re.match(pattern, code_to_parse) for pattern in known_error_patterns)
            if is_known_error:
                 print(f"Skipping known (old) error case: {code_to_parse!r}")
                 continue

            # Skip tests marked explicitly in all_test_examples.py
            # These might indicate unimplemented parsing features or complex setups
            if example.get('skip_eval', False):
                 # Use print instead of self.skipTest within the loop for simplicity
                 print(f"Skipping test {i} (marked skip_eval in examples): {reason} - Code: {code_to_parse!r}")
                 continue

            with self.subTest(i=i, code=code_to_parse, reason=reason): # Use code_to_parse and reason in subtest
                try:
                    # Attempt to parse the code
                    parsed_ast = parse(code_to_parse)
                    # Basic check: ensure it returns an AST node (or Nil for empty/comment-only)
                    self.assertTrue(isinstance(parsed_ast, ChengAstNode) or parsed_ast is Nil,
                                    f"Parsing did not return a valid AST node or Nil. Got: {type(parsed_ast)}")
                except SyntaxError as e:
                    self.fail(f"Example {i} failed to parse:\n--- CODE ---\n{code_to_parse}\n--- ERROR ---\n{e}")
                except Exception as e: # Catch other unexpected errors during parsing
                    self.fail(f"Example {i} raised unexpected exception during parsing:\n--- CODE ---\n{code_to_parse}\n--- ERROR ---\n{type(e).__name__}: {e}")

    # TODO: Add specific tests for known error cases using assertParseError if needed

# Keep the old TestNewParser class commented out or remove it if no longer needed
# class TestNewParser(unittest.TestCase):
#     ... (old test methods) ...

if __name__ == '__main__':
    unittest.main()
