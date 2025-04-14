# src/cheng_py/core/evaluator.py

# 更改: 导入更多 AST 节点和 Nil 实例
from .ast import (
    ChengExpr, ChengAtom, Symbol, Integer, Float, Boolean, String, Pair, Nil, Closure,
    DefineMacroExpr, LambdaExpr, Param, # Removed DefineExpr, LetExpr, LetRecExpr, SetExpr, LetBinding
    AssignmentExpr, ApplyExpr, IfExpr, TernaryExpr, PrefixExpr, BinaryExpr, IndexExpr, SliceExpr,
    ListLiteralExpr, SequenceExpr, QuoteExpr, QuasiquoteExpr, UnquoteExpr, UnquoteSplicingExpr,
    Reference # Added Reference
)
# 导入 Environment 类 和 Macro
from .environment import Environment, EnvironmentError # Import EnvironmentError
from .macros import Macro # Task 6.2: Import Macro
# Task 6.5: Import expand_quasiquote
from .macro_expander import expand_quasiquote
# 更改: 导入 List 类型注解 和 Optional
from typing import List, Tuple, Optional # Import Optional

def evaluate(expr: ChengExpr, env: Environment) -> ChengExpr:
    """
    计算一个表达式。
    当前只处理原子值的自评估。
    """
    # --- Task 6.2: Handle DefineMacroExpr ---
    # Macro definitions are handled specially; they define a macro but don't evaluate to a runtime value in the same way.
    # This check should ideally happen *before* standard evaluation, perhaps in a separate macro expansion phase,
    # but for now, we handle it within evaluate.
    if isinstance(expr, DefineMacroExpr):
        # Create a Macro object and define it in the current environment
        macro = Macro(expr.params, expr.body, env) # Capture current env
        env.define_macro(expr.name, macro)
        return Nil # Define-macro returns an unspecified value (Nil)

    # --- Standard Evaluation ---
    # Check for self-evaluating atoms first
    elif isinstance(expr, (Integer, Float, Boolean, String)):
        return expr
    elif expr is Nil: # Check for the Nil instance
        return expr
    elif isinstance(expr, Symbol):
        # Lookup symbol in environment
        try:
            return env.lookup(expr.value) # Use string key
        except EnvironmentError as e:
             raise e # Re-raise lookup errors

    # --- Handle Specific AST Nodes ---
    # Removed DefineExpr handling - 'define' is likely handled as a primitive or macro now
    elif isinstance(expr, LambdaExpr):
        # Create a closure, capturing the current environment
        param_symbols: List[Symbol] = []
        # Check if expr.params is a Python list (new parser output) or Pair (old style)
        if isinstance(expr.params, list):
            # Handle Python list of Param objects
            for param_node in expr.params:
                if isinstance(param_node, Param):
                    param_symbols.append(param_node.name)
                else:
                    # Should not happen if parser produces list of Param
                    raise SyntaxError(f"Expected Param object in lambda parameter list, got {param_node!r}")
        else:
            # Handle legacy Pair-based list
            curr = expr.params
            while isinstance(curr, Pair):
                param_node = curr.car
                if isinstance(param_node, Param):
                    param_symbols.append(param_node.name)
                elif isinstance(param_node, Symbol): # Handle case where parser didn't make Param
                    param_symbols.append(param_node)
                else:
                    raise SyntaxError(f"Invalid item in lambda parameter list: {param_node!r}")
                curr = curr.cdr
            # Check if the Pair list was proper (ended with Nil)
            if curr is not Nil:
                raise SyntaxError(f"Improper parameter list (Pair structure) in lambda: {expr.params!r}")

        # Create the closure with the Python list of Symbols
        return Closure(param_symbols, expr.body, env)

    # Removed LetExpr, LetRecExpr, SetExpr handling.
    # These are likely handled by AssignmentExpr, SequenceExpr, ApplyExpr, or primitives now.

    # --- Handle Quote and If (using dedicated AST nodes) ---
    elif isinstance(expr, QuoteExpr):
        # Quote prevents evaluation, return the quoted datum directly.
        # The parser ensures expr.datum holds the structure to be quoted.
        return expr.datum # Correct: return the datum itself

    elif isinstance(expr, IfExpr):
        # Evaluate condition
        cond_result = evaluate(expr.condition, env)
        # Choose branch based on condition (#f is false, everything else is true)
        if isinstance(cond_result, Boolean) and cond_result.value is False:
            return evaluate(expr.else_branch, env)
        else:
            return evaluate(expr.then_branch, env)

    # --- Handle Ternary Operator ---
    elif isinstance(expr, TernaryExpr):
        # Evaluate condition
        cond_result = evaluate(expr.condition, env)
        # Choose branch based on condition (#f is false, everything else is true)
        if isinstance(cond_result, Boolean) and cond_result.value is False:
            return evaluate(expr.false_expr, env)
        else:
            return evaluate(expr.true_expr, env)

    # --- Handle Assignment ---
    elif isinstance(expr, AssignmentExpr):
        # Evaluate the value first
        value_to_assign = evaluate(expr.value, env)

        # Check if the target is a dereference expression (like *ptr = ...)
        # We need to evaluate the target *partially* to see if it's a reference
        # This is tricky. Let's assume for now the parser gives us a Symbol for direct assignment
        # and a PrefixExpr('*', ...) for dereference assignment.
        # TODO: Revisit this if the parser structure for assignment targets changes.

        if isinstance(expr.target, PrefixExpr) and expr.target.operator == '*':
            # Handle assignment through dereference: *target_ref = value
            target_ref_expr = expr.target.right
            evaluated_target = evaluate(target_ref_expr, env)

            if not isinstance(evaluated_target, Reference):
                raise TypeError(f"Cannot assign through dereference of non-reference type: {evaluated_target!r}")

            # Perform the assignment in the referenced environment
            ref: Reference = evaluated_target
            try:
                ref.environment.set(ref.variable_name, value_to_assign)
            except EnvironmentError as e:
                 # This shouldn't happen if the reference was valid, but handle defensively
                 raise EnvironmentError(f"Cannot set referenced variable '{ref.variable_name}' which does not exist in its environment: {e}") from e
            return Nil # Assignment returns Nil

        elif isinstance(expr.target, Symbol):
            # Handle direct assignment: target_symbol = value
            target_symbol: Symbol = expr.target
            # Define or set the variable in the *current* environment
            try:
                # Try setting first (like set!)
                env.set(target_symbol.value, value_to_assign)
            except EnvironmentError:
                # If set fails (not found), define it (like let/define)
                env.define(target_symbol.value, value_to_assign)
            return Nil # Assignment/Definition returns Nil
        else:
            # Invalid assignment target
            raise SyntaxError(f"Invalid target for assignment: {expr.target!r}")

    # --- Handle Sequences ---
    elif isinstance(expr, SequenceExpr):
        last_result: ChengExpr = Nil
        for sub_expr in expr.expressions:
            last_result = evaluate(sub_expr, env)
        return last_result # Return the value of the last expression

    # --- Handle Prefix/Binary/Apply ---
    elif isinstance(expr, PrefixExpr):
        # Handle '&' (Reference)
        if expr.operator == '&':
            operand_expr = expr.right
            # The operand of '&' must be a symbol (variable name)
            if not isinstance(operand_expr, Symbol):
                 # We could evaluate it first, but '&' usually operates on names directly
                 # Let's enforce it must be a symbol syntactically for now.
                 # TODO: Revisit if we allow referencing complex expressions' results (needs memory management)
                 raise TypeError(f"Operator '&' can only take a variable name (Symbol), got {operand_expr!r}")

            # Check if the variable exists before creating a reference
            var_name = operand_expr.value
            try:
                 # We don't actually need the value, just check existence
                 env.lookup(var_name)
            except EnvironmentError as e:
                 raise EnvironmentError(f"Cannot take reference of undefined variable '{var_name}'") from e

            # Create and return the Reference object
            return Reference(var_name, env)

        # Handle '*' (Dereference)
        elif expr.operator == '*':
            operand_val = evaluate(expr.right, env)
            if not isinstance(operand_val, Reference):
                raise TypeError(f"Cannot dereference non-reference type: {operand_val!r}")
            # Lookup the variable in the reference's environment
            ref: Reference = operand_val
            try:
                return ref.environment.lookup(ref.variable_name)
            except EnvironmentError as e:
                 # This might happen if the environment changed unexpectedly, though unlikely with lexical scope
                 raise EnvironmentError(f"Failed to lookup referenced variable '{ref.variable_name}' in its environment: {e}") from e

        # Handle other prefix operators (like unary minus, logical not if added)
        else:
            # Evaluate operand
            operand_val = evaluate(expr.right, env)
            # Lookup primitive function for the operator
            # Assume operators like '-' are defined as functions/primitives
            try:
                 op_func = env.lookup(expr.operator)
            except EnvironmentError:
                 raise NameError(f"Unknown prefix operator or function: '{expr.operator}'")

            if not callable(op_func):
                raise TypeError(f"Prefix operator '{expr.operator}' is not callable")
            # Apply the primitive
            try:
                # Pass args as list, even for unary
                return op_func([operand_val])
            except Exception as e:
                raise RuntimeError(f"Error applying prefix operator '{expr.operator}' to {operand_val!r}: {e}") from e

    elif isinstance(expr, BinaryExpr):
        # Evaluate operands
        left_val = evaluate(expr.left, env)
        right_val = evaluate(expr.right, env)
        # Lookup primitive function for the operator
        op_func = env.lookup(expr.operator) # Assume operators like '+' are defined as functions
        if not callable(op_func):
            raise TypeError(f"Operator '{expr.operator}' is not callable")
        # Apply the primitive
        try:
            return op_func([left_val, right_val]) # Pass args as list
        except Exception as e:
            raise RuntimeError(f"Error applying binary operator '{expr.operator}' to {left_val!r} and {right_val!r}: {e}") from e

    elif isinstance(expr, ApplyExpr):
        # Evaluate the operator/procedure
        proc = evaluate(expr.operator, env)
        # Evaluate arguments
        evaluated_args = [evaluate(arg, env) for arg in expr.operands]

        # Check if it's callable (primitive or closure)
        is_primitive = callable(proc)
        is_closure = isinstance(proc, Closure)

        if not (is_primitive or is_closure):
            raise TypeError(f"Object is not callable: {proc!r} derived from {expr.operator!r}")

        # Apply the procedure
        if is_primitive:
            try:
                return proc(evaluated_args)
            except Exception as e:
                func_name = repr(expr.operator)
                arg_reprs = ', '.join(repr(arg) for arg in evaluated_args)
                raise RuntimeError(f"Error in primitive '{func_name}' with args ({arg_reprs}): {e}") from e
        elif is_closure:
            closure: Closure = proc
            # Check arity
            num_params = len(closure.params)
            num_args = len(evaluated_args)
            if num_params != num_args:
                raise TypeError(f"Closure expected {num_params} arguments, got {num_args}")

            # Create call environment
            call_env = Environment(outer=closure.definition_env)
            # Bind arguments
            for param_node, arg_value in zip(closure.params, evaluated_args):
                 if not isinstance(param_node, Param):
                      raise TypeError(f"Internal error: Closure parameter is not Param: {param_node}")
                 call_env.define(param_node.name.value, arg_value) # Use string key

            # Evaluate the closure body
            try:
                # Assuming single body expression for now
                # TODO: Handle multiple body expressions if LambdaExpr body becomes a list
                result = evaluate(closure.body, call_env)
                return result
            except Exception as e:
                param_names = ' '.join(p.name.value for p in closure.params)
                arg_reprs = ', '.join(repr(arg) for arg in evaluated_args)
                raise RuntimeError(f"Error during call to closure ({param_names}) with args ({arg_reprs}): {e}") from e

    # --- Handle Quasiquote ---
    elif isinstance(expr, Pair):
        # Check for empty list (should not happen if parser works)
        if expr is Nil:
             raise ValueError("Cannot evaluate empty list '()'")

        first = expr.car
        rest = expr.cdr

        if isinstance(first, Symbol):
            # Handle 'quote'
            if first.value == 'quote':
                # (quote <expression>)
                if not isinstance(rest, Pair) or not (rest.cdr is Nil): # Check structure
                     raise SyntaxError(f"Malformed quote: expected (quote <expr>), got {expr}")
                return rest.car # Return the argument unevaluated

            # Handle 'if'
            elif first.value == 'if':
                # (if <condition> <consequent> <alternative>)
                # Basic structure check
                if not isinstance(rest, Pair) or not isinstance(rest.cdr, Pair):
                     raise SyntaxError(f"Malformed if: requires condition and consequent, got {expr}")

                condition = rest.car
                consequent = rest.cdr.car
                alternative = Nil # Default alternative

                # Check for alternative and ensure no extra args
                if isinstance(rest.cdr.cdr, Pair):
                    if not (rest.cdr.cdr.cdr is Nil): # Check for extra args
                         raise SyntaxError(f"Malformed if: too many arguments {expr}")
                    alternative = rest.cdr.cdr.car
                elif not (rest.cdr.cdr is Nil): # Check if third element is not Nil but also not Pair
                     raise SyntaxError(f"Malformed if: invalid structure after consequent {expr}")

                # Evaluate condition
                cond_result = evaluate(condition, env)

                # Choose branch based on condition (#f is false, everything else is true)
                if isinstance(cond_result, Boolean) and cond_result.value is False:
                    return evaluate(alternative, env)
                else:
                    return evaluate(consequent, env)

            # Handle 'quasiquote'
    elif isinstance(expr, QuasiquoteExpr):
        return expand_quasiquote(expr.expression, env)

    # --- Handle List Literal ---
    elif isinstance(expr, ListLiteralExpr):
        # Evaluate each element and construct a runtime list (Pair structure)
        # This assumes the runtime uses Pair/Nil for lists.
        result_list = Nil
        for elem_expr in reversed(expr.elements):
            evaluated_elem = evaluate(elem_expr, env)
            result_list = Pair(evaluated_elem, result_list)
        return result_list

    # --- Handle Runtime Pair (should only happen if passed directly, not from parser) ---
    elif isinstance(expr, Pair):
        # Pairs passed directly are treated as data, not evaluated further
        # unless part of an application handled above.
        # This case handles pairs appearing as standalone data.
        return expr # Return the pair itself

    # --- Handle Indexing ---
    elif isinstance(expr, IndexExpr): # Correct indentation
        # Evaluate the sequence and the index
        sequence_val = evaluate(expr.sequence, env)
        index_val = evaluate(expr.index, env)

        # Check if sequence is a Pair (list) and index is an Integer
        if not isinstance(sequence_val, Pair) and sequence_val is not Nil:
            raise TypeError(f"Cannot index non-list type: {type(sequence_val)}")
        if not isinstance(index_val, Integer):
            raise TypeError(f"List index must be an integer, got: {type(index_val)}")

        # Perform indexing on the Pair structure
        try:
            return get_pair_element(sequence_val, index_val.value)
        except IndexError as e:
            raise IndexError(f"Index error accessing list {sequence_val!r}: {e}") from e
        except Exception as e: # Catch other potential errors during traversal
             raise RuntimeError(f"Error indexing list {sequence_val!r} with index {index_val!r}: {e}") from e

    # --- Handle Slicing ---
    elif isinstance(expr, SliceExpr): # Correct indentation
        # Evaluate the sequence and slice bounds
        sequence_val = evaluate(expr.sequence, env)
        start_val = evaluate(expr.start, env) if expr.start else None
        end_val = evaluate(expr.end, env) if expr.end else None

        # Check sequence type
        if not isinstance(sequence_val, Pair) and sequence_val is not Nil:
            raise TypeError(f"Cannot slice non-list type: {type(sequence_val)}")

        # Validate and convert bounds to integers or None
        start_index: Optional[int] = None
        if start_val is not None:
            if not isinstance(start_val, Integer):
                raise TypeError(f"Slice start index must be an integer, got: {type(start_val)}")
            start_index = start_val.value

        end_index: Optional[int] = None
        if end_val is not None:
            if not isinstance(end_val, Integer):
                raise TypeError(f"Slice end index must be an integer, got: {type(end_val)}")
            end_index = end_val.value

        # Perform slicing on the Pair structure
        try:
            return get_pair_slice(sequence_val, start_index, end_index)
        except Exception as e: # Catch potential errors during traversal/construction
             start_repr = repr(start_val) if start_val else 'None'
             end_repr = repr(end_val) if end_val else 'None'
             raise RuntimeError(f"Error slicing list {sequence_val!r} with start={start_repr}, end={end_repr}: {e}") from e

    else: # Correct indentation
        # Should not be reached if parsing is correct and all AST nodes are handled
        raise TypeError(f"Unknown or unhandled expression type for evaluation: {type(expr)}")


# --- Helper functions for Pair-based list operations ---

def get_pair_element(p: ChengExpr, index: int) -> ChengExpr:
    """Gets the element at the specified index from a Pair-based list."""
    if index < 0:
        # Basic negative indexing support (from the end) - might be complex
        # For simplicity, let's disallow negative indices for now.
        raise IndexError(f"Negative indices are not currently supported for Pair lists.")

    current = p
    i = 0
    while isinstance(current, Pair):
        if i == index:
            return current.car
        current = current.cdr
        i += 1

    # If we exit the loop, either index is out of bounds or it wasn't a proper list ending in Nil
    if current is not Nil:
         raise TypeError(f"Cannot index improper list: {p!r}")
    raise IndexError(f"Index {index} out of bounds for list length {i}")


def get_pair_slice(p: ChengExpr, start: Optional[int], end: Optional[int]) -> ChengExpr:
    """Gets a slice from a Pair-based list."""
    elements = []
    current = p
    i = 0

    # Determine effective start and end
    actual_start = start if start is not None else 0
    if actual_start < 0:
         raise IndexError("Slice start index cannot be negative.")
    # end is exclusive

    while isinstance(current, Pair):
        # Stop if we reach or exceed the end index (if specified)
        if end is not None and i >= end:
            break
        # Add element if we are at or past the start index
        if i >= actual_start:
            elements.append(current.car)
        current = current.cdr
        i += 1

    # Check if the original was a proper list if we didn't break early due to 'end'
    if current is not Nil and (end is None or i < end):
         raise TypeError(f"Cannot slice improper list: {p!r}")

    # Check if end index was valid (if specified)
    if end is not None and end < actual_start:
         # Slice results in empty list if end < start
         return Nil
    if end is not None and end < 0:
         raise IndexError("Slice end index cannot be negative.")


    # Reconstruct the resulting list (Pair structure)
    result_list: ChengExpr = Nil
    for elem in reversed(elements):
        result_list = Pair(elem, result_list)
    return result_list
