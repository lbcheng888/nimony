# src/cheng_py/core/primitives.py
from .ast import ChengExpr, Pair, Nil, Symbol, Integer, Float, Boolean, String # Import necessary AST nodes
from typing import List, Union # Import Union

# --- List Primitives ---

def prim_cons(args: List[ChengExpr]) -> ChengExpr:
    """(cons item list) -> new_pair"""
    if len(args) != 2:
        raise TypeError(f"cons: expects 2 arguments, got {len(args)}")
    # No type checking on arguments for now, assumes correct usage
    return Pair(args[0], args[1])

def prim_car(args: List[ChengExpr]) -> ChengExpr:
    """(car pair) -> first_element"""
    if len(args) != 1:
        raise TypeError(f"car: expects 1 argument, got {len(args)}")
    target = args[0]
    if not isinstance(target, Pair):
        # Following Scheme behavior: car of non-pair is an error
        raise TypeError(f"car: expects a pair, got {type(target).__name__} ({target!r})")
    return target.car

def prim_cdr(args: List[ChengExpr]) -> ChengExpr:
    """(cdr pair) -> rest_of_list"""
    if len(args) != 1:
        raise TypeError(f"cdr: expects 1 argument, got {len(args)}")
    target = args[0]
    if not isinstance(target, Pair):
        # Following Scheme behavior: cdr of non-pair is an error
        raise TypeError(f"cdr: expects a pair, got {type(target).__name__} ({target!r})")
    return target.cdr

# --- Type Predicates (Example) ---

def prim_is_pair(args: List[ChengExpr]) -> Boolean:
    """(pair? obj) -> #t or #f"""
    if len(args) != 1:
        raise TypeError(f"pair?: expects 1 argument, got {len(args)}")
    return Boolean(isinstance(args[0], Pair))

def prim_is_null(args: List[ChengExpr]) -> Boolean:
    """(null? obj) -> #t or #f"""
    if len(args) != 1:
        raise TypeError(f"null?: expects 1 argument, got {len(args)}")
    return Boolean(args[0] is Nil)

def prim_list(args: List[ChengExpr]) -> ChengExpr:
    """(list item ...) -> new_list"""
    # Constructs a proper list from the arguments
    res: ChengExpr = Nil
    for item in reversed(args):
        res = Pair(item, res)
    return res

def prim_append(args: List[ChengExpr]) -> ChengExpr:
    """(append list ...) -> new_list"""
    if not args:
        return Nil
    # Append requires all but the last argument to be proper lists
    # Start with the last argument
    result_tail = args[-1]

    # Iterate through the lists to be prepended (from right to left)
    for i in range(len(args) - 2, -1, -1):
        lst = args[i]

        # Validate that lst is a proper list
        if not isinstance(lst, (Pair, Nil)):
             raise TypeError(f"append: expects list arguments (except possibly the last), got {type(lst).__name__}")
        temp_check = lst
        while isinstance(temp_check, Pair):
            temp_check = temp_check.cdr
        if temp_check is not Nil:
             raise TypeError(f"append: expects proper list arguments (except possibly the last), got dotted list {lst!r}")

        # Prepend elements of lst one by one to the front of result_tail
        current_list_elems_reversed: List[ChengExpr] = []
        temp = lst
        while isinstance(temp, Pair):
            current_list_elems_reversed.append(temp.car)
            temp = temp.cdr

        # Prepend elements in original order
        for item in reversed(current_list_elems_reversed):
             # print(f"DEBUG: Prepending {item!r} to {result_tail!r}") # Optional debug
             result_tail = Pair(item, result_tail)
             # print(f"DEBUG: result_tail is now {result_tail!r}") # Optional debug

    return result_tail


# --- Arithmetic Primitives ---

def _check_numeric_args(op_name: str, args: List[ChengExpr]) -> List[Union[Integer, Float]]: # Type hint already uses Union
    """Helper to check if all args are numbers and return them."""
    numeric_args = []
    if not args: # Handle empty arguments for operations like + and *
        if op_name in ['+', 'add']:
            return [Integer(0)] # (+) -> 0
        elif op_name in ['*', 'mul']:
            return [Integer(1)] # (*) -> 1
        else:
             raise TypeError(f"{op_name}: requires at least one argument")

    for arg in args:
        if isinstance(arg, (Integer, Float)):
            numeric_args.append(arg)
        else:
            raise TypeError(f"{op_name}: expects numeric arguments, got {type(arg).__name__} ({arg!r})")
    return numeric_args

def prim_add(args: List[ChengExpr]) -> Union[Integer, Float]: # Type hint already uses Union
    """(+) or (+ num1 num2 ...) -> sum"""
    nums = _check_numeric_args('+', args)
    # Determine result type (float if any arg is float)
    result_type = Float if any(isinstance(n, Float) for n in nums) else Integer
    total = sum(n.value for n in nums)
    return result_type(total)

def prim_sub(args: List[ChengExpr]) -> Union[Integer, Float]: # Type hint already uses Union
    """(-) or (- num1) or (- num1 num2 ...) -> difference or negation"""
    nums = _check_numeric_args('-', args)
    if len(nums) == 1: # Negation
        result_type = Float if isinstance(nums[0], Float) else Integer
        return result_type(-nums[0].value)
    else: # Subtraction
        result_type = Float if any(isinstance(n, Float) for n in nums) else Integer
        result = nums[0].value
        for num in nums[1:]:
            result -= num.value
        return result_type(result)

def prim_mul(args: List[ChengExpr]) -> Union[Integer, Float]: # Type hint already uses Union
    """(*) or (* num1 num2 ...) -> product"""
    nums = _check_numeric_args('*', args)
    result_type = Float if any(isinstance(n, Float) for n in nums) else Integer
    product = 1
    for num in nums:
        product *= num.value
    return result_type(product)

def prim_div(args: List[ChengExpr]) -> Union[Integer, Float]: # Type hint already uses Union
    """(/ num1) or (/ num1 num2 ...) -> division result (always float for division?)"""
    # Scheme division usually results in exact rationals or floats.
    # Python's / always produces floats. Let's stick to float results for simplicity.
    nums = _check_numeric_args('/', args)
    if len(nums) == 1: # Reciprocal
        if nums[0].value == 0:
            raise ZeroDivisionError("division by zero")
        return Float(1.0 / nums[0].value)
    else:
        result = float(nums[0].value) # Start with float
        for num in nums[1:]:
            if num.value == 0:
                raise ZeroDivisionError("division by zero")
            result /= num.value

        # Determine return type: Float if any original operand was Float,
        # otherwise return Integer if result is exact, Float otherwise.
        has_float_operand = any(isinstance(n, Float) for n in nums)
        if has_float_operand:
            return Float(result)
        else: # All operands were Integers
            if result.is_integer():
                 return Integer(int(result)) # Return Integer only if exact and all inputs were Int
            else:
                 return Float(result) # Return Float if division resulted in non-integer

# --- Comparison Primitives ---

def prim_eq(args: List[ChengExpr]) -> Boolean:
    """(= obj1 obj2) -> #t or #f. Checks for numeric equality or object identity."""
    if len(args) != 2:
        raise TypeError(f"=: expects 2 arguments, got {len(args)}")
    a, b = args[0], args[1]

    # Numeric comparison (handle Integer and Float)
    if isinstance(a, (Integer, Float)) and isinstance(b, (Integer, Float)):
        return Boolean(a.value == b.value)
    # Boolean comparison
    elif isinstance(a, Boolean) and isinstance(b, Boolean):
        return Boolean(a.value == b.value)
    # String comparison
    elif isinstance(a, String) and isinstance(b, String):
        return Boolean(a.value == b.value)
    # Symbol comparison (by value)
    elif isinstance(a, Symbol) and isinstance(b, Symbol):
        return Boolean(a.value == b.value)
    # Nil comparison
    elif a is Nil and b is Nil:
        return Boolean(True)
    # Default to object identity for other types (like Pairs, Closures)
    # Note: This means (cons 1 2) != (cons 1 2) unless they are the same object.
    # Scheme's `equal?` provides deep comparison, `eq?` provides identity.
    # This `=` acts more like `eqv?` for numbers/chars/bools and `eq?` otherwise.
    else:
        return Boolean(a is b)

def _check_comparison_args(op_name: str, args: List[ChengExpr]) -> tuple[Union[Integer, Float], Union[Integer, Float]]:
    """Helper to check for exactly two numeric arguments for comparison."""
    if len(args) != 2:
        raise TypeError(f"{op_name}: expects exactly 2 arguments, got {len(args)}")
    a, b = args[0], args[1]
    if not isinstance(a, (Integer, Float)):
        raise TypeError(f"{op_name}: first argument must be numeric, got {type(a).__name__} ({a!r})")
    if not isinstance(b, (Integer, Float)):
        raise TypeError(f"{op_name}: second argument must be numeric, got {type(b).__name__} ({b!r})")
    return a, b

def prim_gt(args: List[ChengExpr]) -> Boolean:
    """>( num1 num2) -> #t or #f"""
    a, b = _check_comparison_args('>', args)
    return Boolean(a.value > b.value)

def prim_lt(args: List[ChengExpr]) -> Boolean:
    """(< num1 num2) -> #t or #f"""
    a, b = _check_comparison_args('<', args)
    return Boolean(a.value < b.value)

def prim_ge(args: List[ChengExpr]) -> Boolean:
    """>(>= num1 num2) -> #t or #f"""
    a, b = _check_comparison_args('>=', args)
    return Boolean(a.value >= b.value)

def prim_le(args: List[ChengExpr]) -> Boolean:
    """(<= num1 num2) -> #t or #f"""
    a, b = _check_comparison_args('<=', args)
    return Boolean(a.value <= b.value)

# --- Equality Primitives ---
# Note: prim_eq handles numeric equality. We need eq? and equal? for Scheme-like behavior.

def prim_eq_q(args: List[ChengExpr]) -> Boolean:
    """(eq? obj1 obj2) -> #t or #f. Checks for object identity (like Python's 'is')."""
    if len(args) != 2:
        raise TypeError(f"eq?: expects 2 arguments, got {len(args)}")
    # For immutable atoms (numbers, booleans, Nil, maybe symbols/strings depending on interning),
    # 'is' might work, but direct comparison is safer. For mutable objects (Pairs), 'is' is correct.
    a, b = args[0], args[1]
    if isinstance(a, (Integer, Float, Boolean, String, Symbol)) or a is Nil:
        # For atoms, eq? often behaves like = or eqv? in Scheme
        return prim_eq(args) # Reuse '=' logic for atoms
    else:
        # For Pairs and other potential mutable types, check identity
        return Boolean(a is b)

def prim_equal_q(args: List[ChengExpr]) -> Boolean:
    """(equal? obj1 obj2) -> #t or #f. Checks for structural equality (deep comparison)."""
    if len(args) != 2:
        raise TypeError(f"equal?: expects 2 arguments, got {len(args)}")
    a, b = args[0], args[1]

    # Use Python's rich comparison (__eq__) which we've defined on AST nodes
    # This handles atoms and performs recursive comparison for Pairs.
    # Need to handle potential cycles if Pair.__eq__ becomes complex.
    try:
        return Boolean(a == b)
    except RecursionError:
        raise RuntimeError("equal?: Maximum recursion depth exceeded, possibly due to cyclic structure")


# Dictionary mapping primitive names (as strings) to their functions
BUILTIN_PRIMITIVES = {
    "cons": prim_cons,
    "car": prim_car,
    "cdr": prim_cdr,
    "pair?": prim_is_pair,
    "null?": prim_is_null,
    "list": prim_list,      # Added list
    "append": prim_append,  # Added append
    "+": prim_add,
    "-": prim_sub,
    "*": prim_mul,
    "/": prim_div,
    # Comparison
    "=": prim_eq,       # Numeric equality primarily
    ">": prim_gt,
    "<": prim_lt,
    ">=": prim_ge,
    "<=": prim_le,
    # Equality
    "eq?": prim_eq_q,     # Identity check (mostly)
    "equal?": prim_equal_q, # Structural check
    # Add more primitives here
}
