# tests/cheng_py/test_type_checker.py

import pytest

from cheng_py.core.ast import Symbol, Integer, Float, Boolean, String, Pair, Nil as AstNil, LambdaExpr, IfExpr, ApplyExpr, QuoteExpr
from cheng_py.parser.parser import parse
from cheng_py.analyzer.types import (
    ChengType, IntType, FloatType, BoolType, StringType, NilType, AnyType, FuncType,
    RefType, ListType, # Added ListType
    T_INT, T_FLOAT, T_BOOL, T_STRING, T_NIL, T_ANY, T_SYMBOL, # Added T_SYMBOL
    make_func_type, make_list_type # Added make_list_type
)
# Explicitly import TypeError from the checker module
from cheng_py.analyzer.type_checker import TypeChecker, TypeEnvironment, BorrowEnvironment
from cheng_py.analyzer.type_checker import TypeError as CheckerTypeError
from .all_test_examples import all_examples # Import the examples

# No longer need to pre-filter or create IDs/batches here

# Single test function to process all examples sequentially with one checker instance
def test_type_check_all_examples_sequentially():
    """
    Runs the type checker sequentially on all code examples from all_test_examples.py,
    maintaining the checker's state between examples. Verifies results against
    expected_type or type_raises for examples not marked with 'skip_typecheck'.
    """
    # Create ONE checker instance to maintain state across examples
    checker = TypeChecker()

    for i, example in enumerate(all_examples):
        code = example['code']
        should_skip = example.get('skip_typecheck', False)
        expected_type = example.get('expected_type')
        type_raises = example.get('type_raises')
        type_error_msg = example.get('type_error_msg')
        reason = example.get('reason', '') # Get reason for context

        # Use the explicitly imported TypeError if specified
        type_raises_class = example.get('type_raises')
        if type_raises_class and type_raises_class.__name__ == 'TypeError':
            type_raises = CheckerTypeError
        else:
            type_raises = type_raises_class

        print(f"\n--- Running Test Example {i+1} {'(Skipping Assertions)' if should_skip else ''} ---")
        print(f"Code: {code.strip()}")
        if reason: print(f"Reason: {reason}")

        try:
            ast = parse(code)
        except Exception as e:
            # If parsing fails, it should only fail the test if we weren't expecting
            # a type error (as parsing is a prerequisite for type checking).
            # However, for simplicity, let's just fail the test run here.
            pytest.fail(f"Example {i+1}: Parsing failed unexpectedly for code:\n{code}\nError: {e}")
            continue # Should not be reached due to pytest.fail

        # Always run the checker to update state, even if skipping assertions
        current_exception = None
        actual_type = None
        try:
            actual_type = checker.check(ast) # Use the persistent checker instance
        except Exception as e:
            current_exception = e

        # --- Assertion Logic (only if not skipping) ---
        if not should_skip:
            if type_raises:
                # Path 1: Expecting an exception
                assert current_exception is not None, \
                       f"Example {i+1}: Code was expected to raise {type_raises.__name__} but did not.\nCode: {code}"
                assert isinstance(current_exception, type_raises), \
                       f"Example {i+1}: Code raised {type(current_exception).__name__} but expected {type_raises.__name__}.\nCode: {code}\nError: {current_exception}"
                # Optionally check the exception message
                if type_error_msg:
                    assert type_error_msg in str(current_exception), \
                           f"Example {i+1}: Code: {code}\nExpected error message part: '{type_error_msg}'\nActual error: '{str(current_exception)}'"
                print(f"Expected exception {type_raises.__name__} caught successfully.")

            elif expected_type is not None:
                # Path 2: Expecting a specific type, no exception
                assert current_exception is None, \
                       f"Example {i+1}: Type checking unexpectedly failed (expected type {expected_type!r}).\nCode: {code}\nError: {current_exception}"
                assert actual_type == expected_type, \
                       f"Example {i+1}: Code: {code}\nExpected type: {expected_type!r}\nActual type:   {actual_type!r}"
                print(f"Type check successful. Expected: {expected_type!r}, Got: {actual_type!r}")

            else:
                # Path 3: Expecting successful type check, but no specific type or exception defined
                assert current_exception is None, \
                       f"Example {i+1}: Type checking unexpectedly failed (no specific expectation).\nCode: {code}\nError: {current_exception}"
                print(f"Type check successful (no specific expectation). Got type: {actual_type!r}")
        else:
             # If skipping assertions, just print the outcome
             if current_exception:
                 print(f"Checker raised (ignored due to skip): {type(current_exception).__name__}: {current_exception}")
             else:
                 print(f"Checker returned type (ignored due to skip): {actual_type!r}")
