# src/cheng_py/core/macro_expander.py

from .ast import (
    ChengExpr, Symbol, Pair, Nil, DefineMacroExpr, LambdaExpr, ChengAstNode, QuasiquoteExpr, # Added QuasiquoteExpr
    Integer, Float, Boolean, String
)
from .environment import Environment
from .macros import Macro
from typing import List, Tuple, Union

# --- Helper functions to build AST for list construction ---
# Import necessary AST nodes
from .ast import QuoteExpr, ApplyExpr

def make_quote(expr: ChengExpr) -> QuoteExpr:
    """Creates QuoteExpr(<expr>) AST node."""
    return QuoteExpr(expr)

def make_cons(car: ChengExpr, cdr: ChengExpr) -> ApplyExpr:
    """Creates ApplyExpr for (cons <car> <cdr>) AST node."""
    return ApplyExpr(Symbol("cons"), [car, cdr]) # Pass args as list

def make_list(*elements: ChengExpr) -> ApplyExpr:
    """Creates ApplyExpr for (list <element>...) AST node."""
    # Directly create the ApplyExpr node
    return ApplyExpr(Symbol("list"), list(elements)) # Pass args as list

def make_append(*lists: ChengExpr) -> ChengExpr: # Return type can be QuoteExpr or ApplyExpr
    """Creates ApplyExpr for (append <list>...) AST node, or QuoteExpr(Nil)."""
    if not lists:
        # (append) -> '() -> QuoteExpr(Nil)
        return QuoteExpr(Nil)

    # Filter out empty quoted lists '() which are identity for append
    # Note: make_quote now returns QuoteExpr
    filtered_lists = [lst for lst in lists if not (isinstance(lst, QuoteExpr) and lst.datum is Nil)]

    if not filtered_lists:
        # If all lists were empty, return '()
        return QuoteExpr(Nil)
    if len(filtered_lists) == 1:
        # (append x) -> x (return the single list expression directly)
        return filtered_lists[0]

    # Create the ApplyExpr node for (append ...)
    return ApplyExpr(Symbol("append"), filtered_lists) # Pass args as list


# --- Quasiquote Expansion Logic (AST -> AST) ---

def expand_quasiquote(datum: ChengExpr, level: int) -> ChengExpr:
    """
    Recursively transforms a quasiquoted datum into an AST expression
    that will construct the datum when evaluated. Always returns AST.
    Uses cons/append/list/quote for construction.
    - level: Tracks the nesting depth of quasiquotes.
    - Unquoting happens only when level is 1.
    """
    if isinstance(datum, Pair):
        # Handle Pairs (potential lists, unquotes, etc.)
        if datum is Nil:
            return make_quote(Nil) # `() -> '()

        car = datum.car
        cdr = datum.cdr

        # --- Check for special forms within the Pair ---
        if isinstance(car, Symbol):
            if car.value == 'unquote':
                if not isinstance(cdr, Pair) or cdr.cdr is not Nil:
                    raise SyntaxError(f"Malformed unquote: expected (, <datum>), got {datum}")
                if level == 1:
                    return cdr.car # Unquote at level 1: return expression directly
                else:
                    # Nested unquote: build (list 'unquote <expanded_arg>)
                    processed_arg = expand_quasiquote(cdr.car, level - 1)
                    return make_list(make_quote(Symbol('unquote')), processed_arg)
            elif car.value == 'unquote-splicing':
                 # Handled below when processing list elements recursively
                 pass # Fall through to list processing logic
            elif car.value == 'quasiquote':
                if not isinstance(cdr, Pair) or cdr.cdr is not Nil:
                    raise SyntaxError(f"Malformed nested quasiquote: expected (` <datum>), got {datum}")
                # Nested quasiquote: build (list 'quasiquote <expanded_arg>)
                processed_arg = expand_quasiquote(cdr.car, level + 1)
                return make_list(make_quote(Symbol('quasiquote')), processed_arg)

        # --- Recursive List Construction ---
        # Check if car is unquote-splicing
        if isinstance(car, Pair) and isinstance(car.car, Symbol) and car.car.value == 'unquote-splicing':
            # Handle `(,@expr ...)`
            if not isinstance(car.cdr, Pair) or car.cdr.cdr is not Nil:
                raise SyntaxError(f"Malformed unquote-splicing: expected (,@ <datum>), got {car}")
            if level == 1:
                splice_expr = car.cdr.car
                expanded_cdr = expand_quasiquote(cdr, level) # Expand the rest of the list
                # Result is (append splice_expr expanded_cdr)
                return make_append(splice_expr, expanded_cdr)
            else:
                # Nested splice: `(cons (list 'unquote-splicing <expanded_arg>) <expanded_cdr>)`
                processed_splice_arg = expand_quasiquote(car.cdr.car, level - 1)
                expanded_cdr = expand_quasiquote(cdr, level)
                splice_part = make_list(make_quote(Symbol('unquote-splicing')), processed_splice_arg)
                return make_cons(splice_part, expanded_cdr)
        else:
            # Regular element in car
            expanded_car = expand_quasiquote(car, level)
            expanded_cdr = expand_quasiquote(cdr, level)
            # Result is (cons expanded_car expanded_cdr)
            return make_cons(expanded_car, expanded_cdr)

    else:
        # --- Handle Atoms ---
        # Atoms at level 1 should be quoted: `atom -> (quote atom)
        # Atoms at deeper levels are doubly quoted: ``atom -> (quote (quote atom))
        # The make_quote helper handles the quoting.
        return make_quote(datum)


# --- Main Macro Expansion Function ---

def expand_macros(expr: ChengExpr, env: Environment) -> ChengExpr:
    """
    Recursively expands macros in the given expression AST.
    This should be called before evaluation.
    """
    # Keep track of expansion cycles to detect infinite loops (optional but recommended)
    # expansion_depth = 0
    # MAX_EXPANSION_DEPTH = 100

    if not isinstance(expr, Pair):
        # Atoms and non-Pair compound nodes that might contain expandable sub-expressions
        if isinstance(expr, QuasiquoteExpr):
             # Handle `expr syntax node
             return expand_quasiquote(expr.expression, 1)
        elif isinstance(expr, LambdaExpr):
            expanded_body = expand_macros(expr.body, env)
            return LambdaExpr(expr.params, expanded_body) # Removed return_type_syntax if not present
        # Removed LetExpr handling - let is no longer a special AST node
        elif isinstance(expr, DefineMacroExpr):
             # Expand the macro body itself
             expanded_body = expand_macros(expr.body, env)
             return DefineMacroExpr(expr.name, expr.params, expanded_body)
        # Other atoms or nodes without sub-expressions are returned directly
        return expr

    # --- Handle Lists (Pair) ---
    if expr is Nil:
        return Nil

    first = expr.car
    rest = expr.cdr

    # --- Handle Quasiquote ---
    if isinstance(first, Symbol) and first.value == 'quasiquote':
        if not isinstance(rest, Pair) or rest.cdr is not Nil:
            raise SyntaxError(f"Malformed quasiquote: expected (` <datum>), got {expr}")
        datum = rest.car
        # Start quasiquote expansion at level 1 - returns AST to build the structure
        return expand_quasiquote(datum, 1) # Use the new expand_quasiquote

    # --- Check if it's a macro call ---
    elif isinstance(first, Symbol):
        macro = env.lookup_macro(first)
        if macro:
            # --- Macro Expansion ---
            # 1. Extract arguments (unevaluated AST nodes)
            args: List[ChengExpr] = []
            curr = rest
            while isinstance(curr, Pair):
                args.append(curr.car)
                curr = curr.cdr
            if curr is not Nil:
                # Handle dotted lists for varargs macros if supported later
                raise SyntaxError(f"Improper list for macro call arguments: {rest}")

            # 2. Check arity (Simple check, needs update for varargs)
            # TODO: Handle varargs (dotted parameter list in macro definition)
            if isinstance(macro.params, List): # Check if params is a list of Symbols
                 if len(macro.params) != len(args):
                      raise TypeError(f"Macro '{first.value}' expected {len(macro.params)} arguments, got {len(args)}")
                 param_symbols = macro.params
            elif isinstance(macro.params, Pair): # Handle raw Pair list from AST
                 # Convert Pair list to Python list for easier handling
                 param_symbols_list: List[Symbol] = []
                 p_curr = macro.params
                 while isinstance(p_curr, Pair):
                      if not isinstance(p_curr.car, Symbol):
                           raise TypeError(f"Macro parameter names must be symbols, got {p_curr.car}")
                      param_symbols_list.append(p_curr.car)
                      p_curr = p_curr.cdr
                 if p_curr is not Nil: # Dotted list for varargs
                      # TODO: Implement varargs handling
                      raise NotImplementedError("Varargs macros not yet fully supported in arity check")
                 param_symbols = param_symbols_list
                 if len(param_symbols) != len(args):
                      raise TypeError(f"Macro '{first.value}' expected {len(param_symbols)} arguments, got {len(args)}")
            else:
                 raise TypeError(f"Invalid macro parameter definition for '{first.value}'")


            # 3. Create expansion environment
            expansion_env = Environment(outer=macro.definition_env)

            # 4. Bind macro parameters to argument ASTs
            for param_symbol, arg_expr in zip(param_symbols, args):
                expansion_env.define(param_symbol, arg_expr)

            # 5. Evaluate the macro body in the expansion environment
            # Re-import evaluate locally for this specific use
            from .evaluator import evaluate
            try:
                expanded_code = evaluate(macro.body, expansion_env)
            except Exception as e:
                 # Improve error reporting
                 import traceback
                 tb_str = traceback.format_exc()
                 raise RuntimeError(f"Error during expansion of macro '{first.value}': {e}\nTraceback:\n{tb_str}") from e


            # 6. Recursively expand the result of the expansion
            return expand_macros(expanded_code, env)
        else:
            # Not a macro call, fall through to expand elements
            pass

    # --- Expand elements of a regular list ---
    expanded_car = expand_macros(expr.car, env)
    expanded_cdr = expand_macros(expr.cdr, env)

    # Optimization: if nothing changed, return original pair
    if expanded_car is expr.car and expanded_cdr is expr.cdr:
        return expr
    else:
        return Pair(expanded_car, expanded_cdr)
