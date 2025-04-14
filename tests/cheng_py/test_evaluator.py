# tests/cheng_py/test_evaluator.py

import unittest
from cheng_py.core.ast import (
    Symbol, Integer, Float, Boolean, String, Pair, Nil, ChengExpr, DefineMacroExpr, Closure # Added DefineMacroExpr, Closure
)
from cheng_py.core.environment import create_global_env, Environment, EnvironmentError
from cheng_py.core.macros import Macro # Task 6.5: Import Macro
from cheng_py.core.macro_expander import expand_macros # Task 6.5: Import expand_macros
from cheng_py.core.evaluator import evaluate
from cheng_py.parser.parser import parse # For parsing test inputs
# Import examples
from .all_test_examples import all_examples

# Helper to create lists easily for comparison (from test_parser)
def make_list(*elements):
    res: ChengExpr = Nil
    for el in reversed(elements):
        assert isinstance(el, ChengExpr), f"Element {el} is not a ChengExpr"
        res = Pair(el, res)
    return res

class TestEvaluator(unittest.TestCase):

    def setUp(self):
        """创建每个测试使用的全局环境"""
        self.global_env = create_global_env()

    def eval_string(self, text: str, env: Environment = None) -> ChengExpr:
        """Helper to parse and evaluate a string"""
        if env is None:
            env = self.global_env
        ast = parse(text)
        # Task 6.5: Add macro expansion step
        expanded_ast = expand_macros(ast, env)
        # DEBUG: Print the expanded AST before evaluation (Removed)
        print(f"DEBUG eval_string: Input='{text}', Expanded AST={expanded_ast!r}") # DEBUG ADDED
        evaluated_result = evaluate(expanded_ast, env)
        print(f"DEBUG eval_string: Input='{text}', Evaluated Result={evaluated_result!r}") # DEBUG ADDED
        return evaluated_result

    def assertEvalTo(self, text: str, expected_result: ChengExpr, env: Environment = None):
        """Asserts that evaluating text results in expected_result"""
        actual_result = self.eval_string(text, env)
        print(f"DEBUG assertEvalTo: Input='{text}', Expected={expected_result!r}, Actual={actual_result!r}") # DEBUG ADDED
        # Removed DEBUG prints
        self.assertEqual(actual_result, expected_result,
                         f"Input: {text}\nExpected: {expected_result!r}\nActual:   {actual_result!r}")

    def assertEvalRaises(self, text: str, expected_exception: type, expected_message_part: str = None, env: Environment = None):
        """Asserts that evaluating text raises the expected exception"""
        with self.assertRaises(expected_exception) as cm:
            self.eval_string(text, env)
        if expected_message_part:
            self.assertIn(expected_message_part, str(cm.exception), f"Input: {text}")

    # NOTE: Most specific test methods below are removed as examples are now in all_test_examples.py
    # Keep the class structure and helper methods for potential future use.

    # TODO: Add tests that load examples from all_test_examples.py and evaluate them,
    # comparing results against expected values.

    def test_evaluate_examples(self):
        """Runs all examples defined in all_test_examples.py"""
        for i, example in enumerate(all_examples):
            code = example['code']
            reason = example.get('reason', '')
            # Use subtest for better error reporting
            with self.subTest(i=i, code=code, reason=reason):
                # Reset environment for each independent test case
                # Note: This prevents testing sequences that build state across examples.
                current_env = create_global_env()

                # Skip tests marked explicitly or requiring sequential evaluation for now
                if example.get('skip_eval', False):
                    self.skipTest(f"Skipped (explicitly): {reason}")
                    continue
                if "Requires sequential eval" in reason:
                     self.skipTest(f"Skipped (requires sequential eval): {reason}")
                     continue
                # Skip tests that depend on prior definitions for now
                # (e.g., using 'x' after 'x = 10' in a separate example dict)
                # A simple heuristic: check if the reason mentions a prior definition.
                # This is imperfect but covers many cases.
                if "Requires prior" in reason:
                     self.skipTest(f"Skipped (requires prior definition): {reason}")
                     continue


                expected_exception = example.get('raises')
                expected_result = example.get('expected_eval')

                # Removed DEBUG prints

                if expected_exception:
                    # print("Action: Calling assertEvalRaises") # DEBUG REMOVED
                    self.assertEvalRaises(code, expected_exception, env=current_env)
                else:
                    # We must have an expected result if no exception is expected
                    # (unless skipped which is handled above)
                    self.assertIsNotNone(expected_result, f"Test case must have 'expected_eval' or 'raises' or be skipped: {code}")
                    # print("Action: Calling assertEvalTo") # DEBUG REMOVED
                    self.assertEvalTo(code, expected_result, env=current_env)


if __name__ == '__main__':
    unittest.main()
