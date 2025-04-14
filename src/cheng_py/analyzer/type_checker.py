# src/cheng_py/analyzer/type_checker.py

"""
实现 Cheng 语言的类型检查器。
"""

from typing import Dict, Optional, List, Tuple

from cheng_py.core.ast import (
    ChengExpr, Symbol, Integer, Float, Boolean, String, Pair, Nil, Param, # Added Param
    LambdaExpr, IfExpr, ApplyExpr, QuoteExpr, DefineMacroExpr, # Removed LetExpr again
    AssignmentExpr, PrefixExpr, BinaryExpr, ListLiteralExpr, SequenceExpr, # Added new AST nodes
    IndexExpr, SliceExpr, TernaryExpr # Added for list access and ternary
)
from cheng_py.analyzer.types import (
    ChengType, IntType, FloatType, BoolType, StringType, NilType, SymbolType, AnyType,
    PairType, ListType, FuncType, RefType, # Removed MutRefType again
    T_INT, T_FLOAT, T_BOOL, T_STRING, T_NIL, T_SYMBOL, T_ANY,
    make_list_type, make_func_type, is_copy_type # Import is_copy_type
)
# --- Task 6.1: Import Runtime Simulation ---
from ..runtime.runtime import RuntimeEnvironment, RuntimeValue, MemoryState

class TypeError(Exception):
    """类型错误异常。"""
    pass

# --- Task 4.8: Borrow Checking Structures ---
from enum import Enum, auto
from dataclasses import dataclass

class BorrowKind(Enum):
    # Since '&' is the only reference and it's mutable per spec/decision
    MUTABLE = auto()
    # IMMUTABLE = auto() # Removed

@dataclass
class BorrowInfo:
    """Information about an active borrow."""
    kind: BorrowKind
    # TODO: Add lifetime information here later?
    # lifetime: LifetimeId
    # TODO: Add source location of the borrow?

class BorrowState:
    """Represents the borrowing status and ownership state of a variable."""
    def __init__(self):
        # Store active borrows. For immutable, could be multiple. For mutable, only one.
        self.active_borrows: List[BorrowInfo] = []
        self.moved: bool = False # Task 4.3: Track if the value has been moved

    def check_add_borrow(self, kind: BorrowKind):
        """Checks if adding a borrow of the given kind is allowed."""
        if self.moved:
            raise TypeError("Cannot borrow: variable has been moved") # Task 4.3 Check

        # Simplified check: only MUTABLE borrows exist
        if kind == BorrowKind.MUTABLE:
            if self.active_borrows: # Any existing borrow prevents a new mutable borrow
                # TODO: Distinguish between existing mutable/immutable if IMMUTABLE is reintroduced
                raise TypeError("Cannot take mutable reference: variable is already borrowed")
        # else: # No other kinds exist
        #     raise ValueError(f"Unknown BorrowKind: {kind}") # Should be unreachable
        # If checks pass, do nothing in this method (borrow is added externally)

    def add_borrow(self, kind: BorrowKind): # kind will always be MUTABLE now
        """Adds a borrow *after* checks pass."""
        # self.check_add_borrow(kind) # Assume check was done externally
        self.active_borrows.append(BorrowInfo(kind=kind))

    def remove_borrow(self, kind: BorrowKind):
        """Removes a borrow (simplistic removal for now)."""
        # Proper removal requires identifying *which* borrow ends (lifetimes needed).
        # For now, just remove one matching borrow if it exists.
        for i, borrow in enumerate(self.active_borrows):
            if borrow.kind == kind:
                del self.active_borrows[i]
                return
        # It might be okay if the borrow wasn't found (e.g., scope exit)
        # print(f"Warning: Attempted to remove {kind} borrow, but none found.")

    def mark_as_moved(self):
        """Marks the variable as moved. Checks for existing borrows first."""
        if self.is_borrowed():
             # Cannot move while borrowed (This check is also done in _check_argument_types)
             raise TypeError("Cannot move: variable is currently borrowed")
        self.moved = True

    def is_valid(self) -> bool:
        """Checks if the variable's value is currently valid (not moved)."""
        return not self.moved

    def is_mutably_borrowed(self) -> bool: # This is now equivalent to is_borrowed
        return any(b.kind == BorrowKind.MUTABLE for b in self.active_borrows)

    # def is_immutably_borrowed(self) -> bool: # Removed
    #     return any(b.kind == BorrowKind.IMMUTABLE for b in self.active_borrows)

    def is_borrowed(self) -> bool: # Checks if any borrow (which must be mutable) exists
        return bool(self.active_borrows)


class BorrowEnvironment:
    """Tracks the borrow state of variables in scope."""
    def __init__(self, outer: Optional['BorrowEnvironment'] = None):
        self.states: Dict[Symbol, BorrowState] = {}
        self.outer = outer

    def get_state(self, name: Symbol) -> Optional[BorrowState]:
        """Gets the borrow state for a variable, looking up scopes."""
        if name in self.states:
            return self.states[name]
        elif self.outer:
            return self.outer.get_state(name)
        else:
            return None

    def ensure_state_in_current_scope(self, name: Symbol) -> BorrowState:
        """Gets or creates the borrow state for a variable *only* in the current scope."""
        if name not in self.states:
            self.states[name] = BorrowState()
        return self.states[name]

    def extend(self) -> 'BorrowEnvironment':
        """Creates a new nested borrow environment."""
        return BorrowEnvironment(outer=self)

# --- End Task 4.8 Structures ---


class TypeEnvironment:
    """类型环境，存储变量名到其 (类型, 可变性) 的映射。"""
    def __init__(self, outer: Optional['TypeEnvironment'] = None):
        # Store type and mutability flag
        self.bindings: Dict[Symbol, Tuple[ChengType, bool]] = {}
        self.outer = outer
        # TODO: Add ownership/borrow state tracking here? Or pass runtime env?

    def define(self, name: Symbol, type: ChengType, mutable: bool = False): # Add mutable flag
        """在当前环境中定义变量类型和可变性。"""
        if name in self.bindings:
            # TODO: Handle shadowing or redefinition rules
            print(f"Warning: Redefining symbol '{name.value}' in type environment.")
        self.bindings[name] = (type, mutable)

    def lookup(self, name: Symbol) -> Optional[Tuple[ChengType, bool]]: # Return tuple
        """查找变量类型和可变性。"""
        if name in self.bindings:
            return self.bindings[name]
        elif self.outer:
            return self.outer.lookup(name) # Recursively lookup in outer scope
        else:
            return None # Not found

    def extend(self) -> 'TypeEnvironment':
        """创建一个新的嵌套环境。"""
        return TypeEnvironment(outer=self)


class TypeChecker:
    """
    执行类型检查的类。
    遍历 AST 并推断/验证每个节点的类型，包括借用规则。
    Uses a RuntimeEnvironment simulation to track ownership/borrow state.
    """
    def __init__(self):
        # Initialize global environments
        self.global_type_env = self._create_global_type_env()
        self.global_borrow_env = BorrowEnvironment() # Static borrow tracking
        self.global_runtime_env = RuntimeEnvironment() # Dynamic state simulation

    def _create_global_type_env(self) -> TypeEnvironment:
        """创建包含内建原语类型的全局类型环境。"""
        # TODO: Define global runtime values as well?
        env = TypeEnvironment()

        # --- Type Predicates ---
        env.define(Symbol("int?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("float?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("bool?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("string?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("symbol?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("pair?"), make_func_type([T_ANY], T_BOOL))
        env.define(Symbol("list?"), make_func_type([T_ANY], T_BOOL)) # Checks for proper list
        env.define(Symbol("nil?"), make_func_type([T_ANY], T_BOOL)) # Same as null?
        env.define(Symbol("null?"), make_func_type([T_ANY], T_BOOL))

        # --- Arithmetic ---
        # For simplicity, allow Int/Float mix, returning Float. Stricter checks possible.
        # A more advanced system might use type variables or union types.
        T_NUMERIC = AnyType() # Placeholder for Int | Float concept
        env.define(Symbol("+"), make_func_type([T_NUMERIC, T_NUMERIC], T_NUMERIC))
        env.define(Symbol("-"), make_func_type([T_NUMERIC, T_NUMERIC], T_NUMERIC))
        env.define(Symbol("*"), make_func_type([T_NUMERIC, T_NUMERIC], T_NUMERIC))
        env.define(Symbol("/"), make_func_type([T_NUMERIC, T_NUMERIC], T_FLOAT)) # Division usually results in float

        # --- Comparison ---
        # Allow comparing compatible types. T_ANY for now is simplest.
        env.define(Symbol("="), make_func_type([T_ANY, T_ANY], T_BOOL)) # General equality (potentially structural)
        env.define(Symbol("=="), make_func_type([T_ANY, T_ANY], T_BOOL)) # Add '==' for primitive/reference equality? Assume same as '=' for now.
        env.define(Symbol("<"), make_func_type([T_NUMERIC, T_NUMERIC], T_BOOL))
        env.define(Symbol(">"), make_func_type([T_NUMERIC, T_NUMERIC], T_BOOL))
        env.define(Symbol("<="), make_func_type([T_NUMERIC, T_NUMERIC], T_BOOL))
        env.define(Symbol(">="), make_func_type([T_NUMERIC, T_NUMERIC], T_BOOL))

        # --- List/Pair Operations ---
        # Using T_ANY for elements initially. Generics/Type Variables needed for full accuracy.
        # cons: T -> List(T) -> List(T)  (or T -> Any -> Pair(T, Any))
        env.define(Symbol("cons"), make_func_type([T_ANY, T_ANY], PairType(T_ANY, T_ANY))) # Returns a Pair
        # car: Pair(T, U) -> T
        env.define(Symbol("car"), make_func_type([PairType(T_ANY, T_ANY)], T_ANY)) # Expects Pair, returns car type
        # cdr: Pair(T, U) -> U
        env.define(Symbol("cdr"), make_func_type([PairType(T_ANY, T_ANY)], T_ANY)) # Expects Pair, returns cdr type

        # TODO: Add more primitives: list, append, map, filter, etc.
        # TODO: Define types for I/O, mutation, etc.

        return env # Indent this return statement correctly

    # Removed _resolve_type_syntax as TypeSyntax AST node is removed.
    # Type annotations need to be re-implemented based on the new parser/AST structure if needed.

    # Updated check method signature to include runtime_env
    def check(self, expr: ChengExpr, type_env: Optional[TypeEnvironment] = None, borrow_env: Optional[BorrowEnvironment] = None, runtime_env: Optional[RuntimeEnvironment] = None) -> ChengType:
        """
        Checks the type of the given AST node, considering borrow rules and runtime state.
        Returns the inferred type of the node.
        """
        # Initialize environments if not provided (use globals)
        type_env = type_env if type_env is not None else self.global_type_env
        borrow_env = borrow_env if borrow_env is not None else self.global_borrow_env
        runtime_env = runtime_env if runtime_env is not None else self.global_runtime_env

        node = expr # Use 'node' consistently internally for clarity
        node_type = type(node)

        # --- Atoms ---
        if node_type is Integer:
            return T_INT
        elif node_type is Float:
            return T_FLOAT
        elif node_type is Boolean:
            return T_BOOL
        elif node_type is String:
            return T_STRING
        # Handle Nil instance (produced by parser for '())
        elif node is Nil: # Check for the singleton instance Nil, not the class NilType
            return T_NIL

        # --- Symbol Lookup ---
        elif node_type is Symbol:
            print(f"[Symbol Check] Looking up '{node.value}' in env {id(type_env)}") # DEBUG
            # Handle special form symbols that might not be functions (like 'if', 'lambda')
            # These are typically handled by their specific AST node types (IfExpr, LambdaExpr)
            # But macros might appear as symbols in an ApplyExpr operator position.
            # We handle macros explicitly in ApplyExpr check.

            lookup_result = type_env.lookup(node) # Use type_env
            print(f"[Symbol Check] Lookup result for '{node.value}' in env {id(type_env)}: {lookup_result!r}") # DEBUG
            if lookup_result is None:
                # Check if it's a known macro symbol that wasn't caught by ApplyExpr handling
                # (This path shouldn't ideally be hit for macros if ApplyExpr handles them)
                if node.value in ["when", "unless", "swap"]:
                     # Macros themselves don't have a standalone type when used as a symbol value
                     return T_ANY # Or a specific MacroType?
                raise TypeError(f"Undefined symbol: {node.value}")
            symbol_type, is_mutable = lookup_result # Unpack type and mutability

            # --- Task 4.3 & 4.8 & 6.1: Check for move/borrow violations on use ---
            # Check runtime state first for moved status
            try:
                rt_val = runtime_env.get_runtime_value(node.value) # Check if exists in runtime
                if not rt_val.is_valid():
                    raise TypeError(f"Use of moved variable '{node.value}'")
                # Check runtime borrow state (though static borrow_env check is primary)
                # if rt_val.state == MemoryState.BORROWED:
                #     print(f"Info: Accessing '{node.value}' while runtime state is BORROWED.")
            except NameError:
                 # If not in runtime_env, it might be a global function/primitive
                 # or an error caught later by type_env lookup. Assume ok for now.
                 pass
            except ValueError as e: # Catch runtime's access-moved error
                 raise TypeError(str(e))

            # Static borrow check (still useful for preventing certain patterns)
            borrow_state = borrow_env.get_state(node)
            if borrow_state:
                 # Check if mutably borrowed (Task 4.8 - static check)
                 if borrow_state.is_mutably_borrowed():
                     # This check might need refinement. Reading while mutably borrowed
                     # is often okay if the read doesn't conflict with the borrow's purpose.
                     # For now, we allow it, but a stricter checker might disallow it.
                     # print(f"Info: Accessing '{node.value}' while it is mutably borrowed.")
                     pass

            # If no state found, it implies a global or something not tracked? Should be rare.

            return symbol_type

        # --- Special Forms / Expressions ---
        elif node_type is QuoteExpr:
            # The type of (quote x) is complex. It represents the structure quoted.
            # For now, let's return a generic 'quoted' type or Any.
            # A more sophisticated checker might return a type representing the quoted structure.
            return T_ANY # Simplification

        elif node_type is IfExpr:
            # Check condition type (pass all envs)
            cond_type = self.check(node.condition, type_env, borrow_env, runtime_env)
            # TODO: Handle conditional moves/borrows requires more complex env management
            # (e.g., cloning envs, merging state). For now, just check branches.
            # Create copies or manage state carefully if branches modify envs.
            # Simplification: Assume branches don't modify outer runtime state for now.
            then_type = self.check(node.then_branch, type_env, borrow_env, runtime_env)
            if node.else_branch:
                else_type = self.check(node.else_branch, type_env, borrow_env, runtime_env)
                # Check type compatibility
                if then_type != else_type and then_type != T_ANY and else_type != T_ANY:
                     # Allow Int/Float compatibility
                     if not ((isinstance(then_type, IntType) and isinstance(else_type, FloatType)) or \
                             (isinstance(then_type, FloatType) and isinstance(else_type, IntType))):
                         print(f"Warning: 'if' branches have different incompatible types: {then_type!r} and {else_type!r}")
                         return T_ANY # Or find common supertype
                     else:
                         return T_FLOAT # Promote to Float if Int/Float mix
                elif isinstance(then_type, FloatType) or isinstance(else_type, FloatType):
                     return T_FLOAT # Promote if one branch is Float
                else:
                     return then_type # Return common type (or Any)
            else:
                # If no else branch, result type depends on then_branch and potentially Nil
                # Let's return the then_branch type for now.
                return then_type

        # Removed DefineExpr handling - 'define' is likely handled as a primitive or macro now
        elif node_type is LambdaExpr:
            # Create new environments for checking the lambda body
            lambda_type_env = type_env.extend()
            lambda_borrow_env = borrow_env.extend()
            lambda_runtime_env = runtime_env.extend() # Extend runtime env for body scope
            param_types: List[ChengType] = []
            print(f"[Lambda Check] Created lambda env {id(lambda_type_env)} (outer {id(type_env)}) for {node!r}") # DEBUG

            # Define parameters in the lambda's environments
            print(f"[Lambda Check] Defining params in env {id(lambda_type_env)}") # DEBUG
            for param_node in node.params:
                param_name = param_node.name
                print(f"[Lambda Check]   Defining param '{param_name.value}' in env {id(lambda_type_env)}") # DEBUG
                # TODO: Add support for type annotations on parameters later
                param_type = T_ANY # Default to Any if no annotation
                lambda_type_env.define(param_name, param_type, mutable=False) # Params are immutable bindings
                lambda_borrow_env.ensure_state_in_current_scope(param_name) # Initialize borrow state
                # Simulate parameter binding in runtime env (assume copy/move handled at call site)
                # For checking the body, we just need the name defined.
                # We can define it with a placeholder value like Nil or a special marker.
                lambda_runtime_env.define(param_name.value, Nil) # Define in runtime scope for lookup
                param_types.append(param_type)

            # Check the body using the new environments
            # Handle single expression body vs. sequence
            if isinstance(node.body, SequenceExpr):
                 body_exprs = node.body.expressions
            elif node.body is Nil: # Empty body
                 body_exprs = []
            else: # Single expression body
                 body_exprs = [node.body]

            if not body_exprs:
                 inferred_return_type = T_NIL
            else:
                 last_return_type = T_NIL
                 print(f"[Lambda Check] Checking body in env {id(lambda_type_env)}") # DEBUG
                 for expr in body_exprs:
                     # Pass the extended environments to check each expression in the body
                     last_return_type = self.check(expr, lambda_type_env, lambda_borrow_env, lambda_runtime_env)
                 inferred_return_type = last_return_type

            # Exit the runtime scope simulation for the lambda body
            lambda_runtime_env.exit_scope()

            # The lambda expression itself evaluates to a function type
            return make_func_type(param_types, inferred_return_type)

        elif node_type is ApplyExpr:
            # --- Handle Macros and Special Forms First ---
            if isinstance(node.operator, Symbol):
                op_name = node.operator.value
                # Handle known macros/special forms directly
                if op_name == "when":
                    return self._check_when(node, type_env, borrow_env, runtime_env)
                elif op_name == "unless":
                    return self._check_unless(node, type_env, borrow_env, runtime_env)
                elif op_name == "swap": # Assuming swap! is handled by parser as 'swap' symbol
                    return self._check_swap(node, type_env, borrow_env, runtime_env)
                # Removed 'let' handling as it's not in the grammar
                # Add other special forms here if needed

            # --- Default: Handle as Standard Function Application ---
            # Check the operator type (pass all envs)
            op_type = self.check(node.operator, type_env, borrow_env, runtime_env)

            # --- Handle Application based on Operator Type ---
            # Only allow applying FuncType
            if not isinstance(op_type, FuncType):
                 # Check if it was Nil due to empty list application like (())
                 if op_type == T_NIL and isinstance(node.operator, Pair) and node.operator.car is Nil:
                     raise TypeError("Cannot apply empty list '()'")
                 # Applying AnyType or some other non-function type
                 raise TypeError(f"Cannot apply non-function type: {op_type!r} to arguments {node.operands!r}")

            # --- Operator is FuncType, proceed with checks ---
            # Check argument types (pass all envs)
            arg_types = [self.check(arg, type_env, borrow_env, runtime_env) for arg in node.operands]

            # --- Check Arity (Parameter Count) ---
            if len(arg_types) != len(op_type.param_types):
                op_repr = node.operator.value if isinstance(node.operator, Symbol) else repr(node.operator)
                raise TypeError(f"Function '{op_repr}' expected {len(op_type.param_types)} arguments, got {len(arg_types)}")

            # Check argument type compatibility (and potentially borrow rules for args)
            # Pass runtime_env to the helper
            self._check_argument_types(op_type.param_types, arg_types, node.operator, node.operands, type_env, borrow_env, runtime_env)

            # --- Determine Return Type (with refined numeric handling) ---
            if isinstance(node.operator, Symbol):
                op_name = node.operator.value
                # Special handling for numeric ops
                if op_name in ["+", "-", "*", "/"]:
                    valid_numeric = all(isinstance(t, (IntType, FloatType, AnyType)) for t in arg_types)
                    if not valid_numeric:
                         raise TypeError(f"Operator '{op_name}' expects numeric arguments, got {arg_types!r}")
                    if op_name == "/": return T_FLOAT
                    elif any(isinstance(t, FloatType) for t in arg_types): return T_FLOAT
                    elif all(isinstance(t, IntType) for t in arg_types): return T_INT
                    else: return T_FLOAT # Default to Float for Int/Any mix
                # Special handling for comparison ops
                elif op_name in ["=", "==", "<", ">", "<=", ">="]: # Added ==
                     return T_BOOL
                # TODO: Add specific return type logic for car, cdr, cons etc.
                # elif op_name == "cons": return PairType(arg_types[0], arg_types[1])

            # Default: Return the declared return type of the function
            return op_type.return_type

        # Removed LetExpr handling. Let might be represented by AssignmentExpr within SequenceExpr.
        # Removed SetExpr handling. Set! might be represented by AssignmentExpr.

        elif node_type is DefineMacroExpr:
            # Macros themselves don't have a runtime type in this sense.
            # Type checking happens *after* macro expansion.
            return T_NIL # Or a special MacroType?

        # --- Handle New AST Nodes ---
        elif node_type is AssignmentExpr:
            # Check the value type (pass all envs)
            value_type = self.check(node.value, type_env, borrow_env, runtime_env)
            target_symbol = node.target

            # Determine if the source value is copyable using the helper function
            is_copyable = is_copy_type(value_type)

            # Get the runtime value of the source *before* potential move in assign_value
            source_rt_val: Optional[RuntimeValue] = None
            if isinstance(node.value, Symbol):
                 try:
                      source_rt_val = runtime_env.get_runtime_value(node.value.value)
                 except NameError:
                      raise TypeError(f"Source variable '{node.value.value}' not found for assignment.")
                 except ValueError as e: # Catch access-moved error from runtime
                      raise TypeError(str(e))
            else:
                 # If source is not a symbol, create a temporary RuntimeValue for assign_value
                 # This temporary value is implicitly moved from if not copyable.
                 # Need a dummy ChengExpr if node.value isn't one? No, check returns ChengExpr.
                 source_rt_val = RuntimeValue(node.value) # State is VALID

            # Check if target exists and handle mutability/borrowing for reassignment (set!)
            lookup_result = type_env.lookup(target_symbol)
            if lookup_result:
                # Existing variable (set! semantics)
                var_type, _ = lookup_result

                # Static borrow check (prevents assigning while borrowed)
                borrow_state = borrow_env.get_state(target_symbol)
                if borrow_state and borrow_state.is_borrowed():
                    raise TypeError(f"Cannot assign to '{target_symbol.value}' while it is mutably borrowed")

                # Check type compatibility
                if value_type != var_type and var_type != T_ANY:
                    if not (var_type == T_FLOAT and value_type == T_INT):
                        raise TypeError(f"Type mismatch in assignment to '{target_symbol.value}': expected {var_type!r}, got {value_type!r}")

                # Perform assignment in runtime (handles drop, move/copy, state checks)
                try:
                    runtime_env.assign_value(target_symbol.value, source_rt_val, is_copyable)
                except (RuntimeError, ValueError) as e:
                    raise TypeError(f"Runtime error during assignment to '{target_symbol.value}': {e}")

                # Reset static borrow state's moved flag if it existed
                if borrow_state:
                     borrow_state.moved = False # Assignment makes it valid again

            else:
                # New variable (define/let semantics)
                type_env.define(target_symbol, value_type, mutable=True) # Define type as mutable
                borrow_env.ensure_state_in_current_scope(target_symbol) # Init static borrow state

                # Perform assignment in runtime (defines in current scope)
                try:
                    # Need to handle case where source_rt_val is temporary
                    if isinstance(node.value, Symbol):
                         runtime_env.assign_value(target_symbol.value, source_rt_val, is_copyable)
                    else:
                         # Define directly if source is not a variable
                         runtime_env.define(target_symbol.value, node.value) # is_copyable not needed for define

                except (RuntimeError, ValueError, NameError) as e:
                    raise TypeError(f"Runtime error during definition of '{target_symbol.value}': {e}")


            return T_NIL # Assignment returns Nil

        elif node_type is SequenceExpr:
            # Check each expression in sequence, return type of the last one
            # Create a new scope for the sequence? Only if it represents a block like `begin`.
            # Assuming SequenceExpr itself doesn't create a scope unless told otherwise.
            # Pass envs through.
            last_type: ChengType = T_NIL
            # TODO: Handle scope exit for runtime_env if Sequence represents a block
            # seq_runtime_env = runtime_env.extend() # If it's a block
            seq_runtime_env = runtime_env # If not a block
            for sub_expr in node.expressions:
                last_type = self.check(sub_expr, type_env, borrow_env, seq_runtime_env) # Pass potentially extended env
            # if seq_runtime_env is not runtime_env: seq_runtime_env.exit_scope() # Drop locals if block
            return last_type

        elif node_type is PrefixExpr:
            # Check operand type (pass all envs)
            operand_type = self.check(node.right, type_env, borrow_env, runtime_env)
            op_name = node.operator # e.g., '-', '&', '*'

            # Handle specific prefix operators
            if op_name == '-': # Negation
                if operand_type not in [T_INT, T_FLOAT, T_ANY]:
                    raise TypeError(f"Operator '-' cannot be applied to type {operand_type!r}")
                return operand_type if operand_type != T_ANY else T_FLOAT # Default to Float if Any
            elif op_name == '&': # Reference (Mutable according to spec)
                # Check if operand is an l-value
                if not self._is_lvalue(node.right):
                    raise TypeError("Cannot take reference to temporary value")
                # Check if the l-value binding is mutable
                if not self._is_mutable_binding(node.right, type_env):
                     target_name = node.right.value if isinstance(node.right, Symbol) else 'expression'
                     raise TypeError(f"Cannot take reference to immutable variable '{target_name}'")

                # Borrow checking logic (static and runtime)
                if isinstance(node.right, Symbol):
                    target_name_str = node.right.value
                    # Static check
                    borrow_state = borrow_env.get_state(node.right)
                    if not borrow_state: # Should exist if mutable binding check passed
                         borrow_state = borrow_env.ensure_state_in_current_scope(node.right) # Ensure exists

                    try:
                        borrow_state.check_add_borrow(BorrowKind.MUTABLE)
                        # Runtime check (redundant if static check is correct, but good validation)
                        rt_val = runtime_env.get_runtime_value(target_name_str)
                        if rt_val.state == MemoryState.BORROWED:
                             raise TypeError(f"Runtime state conflict: variable '{target_name_str}' already borrowed")
                        if not rt_val.is_valid():
                             raise TypeError(f"Runtime state conflict: variable '{target_name_str}' is moved")

                        # Update static borrow state
                        borrow_state.add_borrow(BorrowKind.MUTABLE)
                        # Update runtime state simulation
                        runtime_env.borrow_value(target_name_str)

                    except (TypeError, RuntimeError, ValueError, NameError) as e:
                        raise TypeError(f"Cannot take reference to '{target_name_str}': {e}")

                # Return the single RefType
                return RefType(operand_type)
            elif op_name == '*': # Dereference
                # Check only against the single RefType
                if isinstance(operand_type, RefType):
                    # TODO: Check borrow state/lifetime validity for dereference
                    # This requires knowing which reference is being dereferenced
                    # and checking its corresponding borrow state in borrow_env/runtime_env.
                    # Simplification: Allow dereference for now.
                    return operand_type.pointee_type
                else:
                    raise TypeError(f"Cannot dereference non-reference type: {operand_type!r}")
            else:
                raise TypeError(f"Unknown prefix operator: {op_name}")

        elif node_type is BinaryExpr:
            # Check operand types (pass all envs)
            left_type = self.check(node.left, type_env, borrow_env, runtime_env)
            right_type = self.check(node.right, type_env, borrow_env, runtime_env)
            op_name = node.operator # e.g., '+', '=='

            # Lookup the function type for the operator
            op_lookup = type_env.lookup(Symbol(op_name)) # Assume defined in global env
            if not op_lookup or not isinstance(op_lookup[0], FuncType):
                 raise TypeError(f"Unknown or non-function binary operator: {op_name}")
            op_func_type: FuncType = op_lookup[0]

            # Check arity (should be 2)
            if len(op_func_type.param_types) != 2:
                 raise TypeError(f"Binary operator '{op_name}' function type expects {len(op_func_type.param_types)} args, not 2")

            # Check argument compatibility (using the helper, pass runtime_env)
            self._check_argument_types(op_func_type.param_types, [left_type, right_type], Symbol(op_name), [node.left, node.right], type_env, borrow_env, runtime_env)

            # Determine return type (reuse logic from ApplyExpr, refined)
            if op_name in ["+", "-", "*", "/"]:
                 valid_numeric = all(isinstance(t, (IntType, FloatType, AnyType)) for t in [left_type, right_type])
                 if not valid_numeric:
                      raise TypeError(f"Operator '{op_name}' expects numeric arguments, got {[left_type, right_type]!r}")
                 if op_name == "/":
                     return T_FLOAT
                 elif any(isinstance(t, FloatType) for t in [left_type, right_type]):
                     return T_FLOAT
                 elif all(isinstance(t, IntType) for t in [left_type, right_type]):
                     return T_INT
                 else:
                     return T_FLOAT # Default to Float for Int/Any mix
            elif op_name in ["==", "!=", "<", ">", "<=", ">="]: # Use '==' from parser? Assume '=' is equality.
                 # Compatibility checked by _check_argument_types
                 return T_BOOL
            # Note: '==' is already handled by the line above. This specific elif is redundant.
            # else: # Other binary operators? # This else should cover cases not explicitly handled above
            #      return op_func_type.return_type
            # Let's simplify: if it's not numeric or comparison, return the FuncType's return type.
            else:
                 return op_func_type.return_type


        elif node_type is ListLiteralExpr:
            # Check types of elements (pass all envs)
            element_types = [self.check(elem, type_env, borrow_env, runtime_env) for elem in node.elements]
            # Determine the list type.
            if not element_types:
                list_element_type = T_ANY
            else:
                first_type = element_types[0]
                if all(t == first_type for t in element_types):
                    list_element_type = first_type
                else:
                    print(f"Warning: List literal has mixed element types: {[repr(t) for t in element_types]}")
                    list_element_type = T_ANY
            return ListType(list_element_type) # Return the specific ListType

        elif node_type is IndexExpr:
            # Check the object being indexed
            obj_type = self.check(node.obj, type_env, borrow_env, runtime_env)
            if not isinstance(obj_type, ListType):
                # TODO: Allow indexing on other types like String?
                raise TypeError(f"Cannot index non-list type: {obj_type!r}")

            # Check the index type
            index_type = self.check(node.index, type_env, borrow_env, runtime_env)
            if index_type != T_INT:
                raise TypeError(f"List index must be an Integer, got {index_type!r}")

            # Result type is the element type of the list
            return obj_type.element_type

        elif node_type is SliceExpr:
            # Check the object being sliced
            obj_type = self.check(node.obj, type_env, borrow_env, runtime_env)
            if not isinstance(obj_type, ListType):
                 # TODO: Allow slicing on other types like String?
                 raise TypeError(f"Cannot slice non-list type: {obj_type!r}")

            # Check start index type (if present)
            if node.start is not None:
                start_type = self.check(node.start, type_env, borrow_env, runtime_env)
                if start_type != T_INT:
                    raise TypeError(f"Slice start index must be an Integer, got {start_type!r}")

            # Check stop index type (if present)
            if node.stop is not None:
                stop_type = self.check(node.stop, type_env, borrow_env, runtime_env)
                if stop_type != T_INT:
                    raise TypeError(f"Slice stop index must be an Integer, got {stop_type!r}")

            # Slicing returns a new list of the same type
            return obj_type

        # --- Fallback: Handle Pair nodes directly ---
        # This might be hit if the parser produces Pairs for lists in some contexts
        elif node_type is Pair:
            # Could try to infer PairType more specifically, but Any is safer for now
            return PairType(T_ANY, T_ANY) # Or just T_ANY?

        # --- Handle TernaryExpr ---
        elif node_type is TernaryExpr:
            # Check condition type (must be Bool)
            cond_type = self.check(node.condition, type_env, borrow_env, runtime_env)
            # --- Stricter Check: Condition MUST be Bool ---
            if cond_type != T_BOOL:
                raise TypeError(f"Ternary condition must be Boolean, got {cond_type!r}")

            # Check branch types
            # TODO: Handle conditional moves/borrows like in IfExpr? (Simplification for now)
            true_type = self.check(node.true_expr, type_env, borrow_env, runtime_env)
            false_type = self.check(node.false_expr, type_env, borrow_env, runtime_env)

            # Determine result type based on branch compatibility
            if true_type == false_type:
                return true_type
            elif true_type == T_ANY:
                return false_type # If one is Any, return the other specific type
            elif false_type == T_ANY:
                return true_type
            # Allow Int/Float compatibility -> Float
            elif (isinstance(true_type, IntType) and isinstance(false_type, FloatType)) or \
                 (isinstance(true_type, FloatType) and isinstance(false_type, IntType)):
                return T_FLOAT
            # Allow Nil compatibility with List/Pair?
            elif isinstance(true_type, (ListType, PairType)) and false_type == T_NIL:
                 return true_type
            elif isinstance(false_type, (ListType, PairType)) and true_type == T_NIL:
                 return false_type
            else:
                # Incompatible types
                raise TypeError(f"Ternary branches have incompatible types: {true_type!r} and {false_type!r}")

        else:
            raise NotImplementedError(f"Type checking not implemented for AST node: {node_type.__name__}")


    # Updated helper signature - Rewriting with explicit 4-space indentation
    # This helper seems redundant now that ApplyExpr is handled directly. Remove?
    # Keep for now if parser might still produce Pair for application.
    def _check_application(self, node: Pair, type_env: TypeEnvironment, borrow_env: BorrowEnvironment, runtime_env: RuntimeEnvironment) -> ChengType:
        """Helper to check function application represented by a Pair."""
        # ... (Implementation needs updating similar to ApplyExpr) ...
        # This method is likely deprecated by direct ApplyExpr handling.
        # print("Warning: _check_application called, potentially deprecated.")
        # Re-implement or remove based on parser behavior. For now, raise error.
        # Let's comment it out or remove it to avoid confusion.
        pass # Keep the method signature for now, but make it do nothing or raise clearly.
        # raise NotImplementedError("_check_application is deprecated; use ApplyExpr handling.")


    # --- Macro Type Checking Helpers ---
    def _check_when(self, node: ApplyExpr, type_env: TypeEnvironment, borrow_env: BorrowEnvironment, runtime_env: RuntimeEnvironment) -> ChengType:
        """Type checks the 'when' macro: (when condition body...)."""
        if len(node.operands) < 1:
            raise TypeError("'when' requires at least a condition expression")

        condition_expr = node.operands[0]
        body_exprs = node.operands[1:]

        # Check condition type
        cond_type = self.check(condition_expr, type_env, borrow_env, runtime_env)
        if cond_type != T_BOOL and cond_type != T_ANY:
            raise TypeError(f"'when' condition must be Boolean, got {cond_type!r}")

        # Check body expressions
        last_body_type: ChengType = T_NIL
        # TODO: Handle conditional borrow/move state changes if needed (complex)
        for expr in body_exprs:
            last_body_type = self.check(expr, type_env, borrow_env, runtime_env)

        # The result type is the type of the last body expression, or Nil if no body.
        return last_body_type

    def _check_unless(self, node: ApplyExpr, type_env: TypeEnvironment, borrow_env: BorrowEnvironment, runtime_env: RuntimeEnvironment) -> ChengType:
        """Type checks the 'unless' macro: (unless condition body...)."""
        # Logic is identical to 'when' for type checking purposes
        return self._check_when(node, type_env, borrow_env, runtime_env)

    def _check_swap(self, node: ApplyExpr, type_env: TypeEnvironment, borrow_env: BorrowEnvironment, runtime_env: RuntimeEnvironment) -> ChengType:
        """Type checks the 'swap' macro: (swap! place1 place2)."""
        # Renamed to swap! based on common Lisp convention for mutation
        if len(node.operands) != 2:
            raise TypeError("'swap!' requires exactly two place expressions")

        place1 = node.operands[0]
        place2 = node.operands[1]

        # Check if both places are valid l-values and mutable
        if not self._is_lvalue(place1) or not self._is_mutable_binding(place1, type_env):
             raise TypeError(f"'swap!' first argument must be a mutable place (variable), got {place1!r}")
        if not self._is_lvalue(place2) or not self._is_mutable_binding(place2, type_env):
             raise TypeError(f"'swap!' second argument must be a mutable place (variable), got {place2!r}")

        # Check types (must be compatible for assignment)
        type1 = self.check(place1, type_env, borrow_env, runtime_env)
        type2 = self.check(place2, type_env, borrow_env, runtime_env)

        # Check compatibility (simplistic: allow Any or same type, or Int/Float mix)
        compatible = False
        if type1 == type2 or type1 == T_ANY or type2 == T_ANY:
            compatible = True
        elif (isinstance(type1, IntType) and isinstance(type2, FloatType)) or \
             (isinstance(type1, FloatType) and isinstance(type2, IntType)):
            compatible = True # Allow swapping Int and Float

        if not compatible:
            raise TypeError(f"'swap!' requires type-compatible places, got {type1!r} and {type2!r}")

        # Check borrow state (cannot swap if either is borrowed)
        if isinstance(place1, Symbol):
             borrow_state1 = borrow_env.get_state(place1)
             if borrow_state1 and borrow_state1.is_borrowed():
                 raise TypeError(f"Cannot 'swap!' place '{place1.value}' while it is borrowed")
             # Check runtime state too
             try:
                 rt_val1 = runtime_env.get_runtime_value(place1.value)
                 if not rt_val1.is_valid(): raise TypeError(f"Cannot 'swap!' moved variable '{place1.value}'")
                 if rt_val1.state == MemoryState.BORROWED: raise TypeError(f"Runtime: Cannot 'swap!' borrowed variable '{place1.value}'")
             except (NameError, ValueError) as e: raise TypeError(f"Runtime check failed for swap place1 '{place1.value}': {e}")

        if isinstance(place2, Symbol):
             borrow_state2 = borrow_env.get_state(place2)
             if borrow_state2 and borrow_state2.is_borrowed():
                 raise TypeError(f"Cannot 'swap!' place '{place2.value}' while it is borrowed")
             # Check runtime state too
             try:
                 rt_val2 = runtime_env.get_runtime_value(place2.value)
                 if not rt_val2.is_valid(): raise TypeError(f"Cannot 'swap!' moved variable '{place2.value}'")
                 if rt_val2.state == MemoryState.BORROWED: raise TypeError(f"Runtime: Cannot 'swap!' borrowed variable '{place2.value}'")
             except (NameError, ValueError) as e: raise TypeError(f"Runtime check failed for swap place2 '{place2.value}': {e}")


        # Swap doesn't change types, but runtime state simulation would perform the swap.
        # The type checker just ensures it's valid.
        # Swap returns Nil.
        return T_NIL

    # Removed _check_let method as 'let' is not in the grammar

    # Ensure correct signature and indentation for this method
    # Updated signature to include runtime_env
    def _check_argument_types(self, param_types: Tuple[ChengType, ...], arg_types: List[ChengType], operator_node: ChengExpr, operand_nodes: List[ChengExpr], type_env: TypeEnvironment, borrow_env: BorrowEnvironment, runtime_env: RuntimeEnvironment):
        """Checks if provided argument types are compatible with parameter types and borrow/move rules."""
        for i, (param_t, arg_t) in enumerate(zip(param_types, arg_types)):
            operand_node = operand_nodes[i] # Get the specific argument expression node

            # --- Borrow Checking Logic (Task 4.8) ---
            # Check borrow rules *before* type compatibility for references
            if isinstance(param_t, RefType): # Function expects &T
                # Static check: Argument must be an l-value and mutable binding
                if not self._is_lvalue(operand_node):
                     raise TypeError(f"Argument {i+1}: Cannot pass temporary value as reference (&T)")
                if not self._is_mutable_binding(operand_node, type_env):
                     target_name = operand_node.value if isinstance(operand_node, Symbol) else 'expression'
                     raise TypeError(f"Argument {i+1}: Cannot pass immutable variable '{target_name}' as reference (&T)")

                # Static and Runtime borrow state checks
                if isinstance(operand_node, Symbol):
                    target_name_str = operand_node.value
                    borrow_state = borrow_env.get_state(operand_node)
                    if not borrow_state: borrow_state = borrow_env.ensure_state_in_current_scope(operand_node)

                    try:
                        # Static check: Can we add a mutable borrow now?
                        borrow_state.check_add_borrow(BorrowKind.MUTABLE)
                        # Runtime check: Is it currently borrowed or moved?
                        rt_val = runtime_env.get_runtime_value(target_name_str)
                        if not rt_val.is_valid():
                             raise TypeError(f"Runtime state conflict: variable '{target_name_str}' is moved")
                        if rt_val.state == MemoryState.BORROWED:
                             raise TypeError(f"Runtime state conflict: variable '{target_name_str}' already borrowed")
                        # If checks pass, the call itself represents the borrow.
                        # --- Task 4.8: Update state *immediately* to catch aliasing in the same call ---
                        borrow_state.add_borrow(BorrowKind.MUTABLE)
                        runtime_env.borrow_value(target_name_str)
                        # -----------------------------------------------------------------------------
                    except (TypeError, RuntimeError, ValueError, NameError) as e:
                        raise TypeError(f"Argument {i+1}: Cannot pass '{target_name_str}' as reference (&T): {e}")

            # --- Type Compatibility Logic ---
            # Check if the *underlying* types match for references
            if isinstance(param_t, RefType) and isinstance(arg_t, RefType):
                 pointee_param_t = param_t.pointee_type
                 pointee_arg_t = arg_t.pointee_type
                 # Allow Int/Float compatibility within references? No, keep strict for now.
                 if pointee_param_t == pointee_arg_t or pointee_param_t == T_ANY or pointee_arg_t == T_ANY:
                     continue # &T compatible with &T or &Any
                 else:
                     raise TypeError(f"Argument {i+1} reference type mismatch for '{operator_node!r}': expected &{pointee_param_t!r}, got &{pointee_arg_t!r}")
            # Check if passing a reference where a value is expected (implicitly dereference?)
            # This is generally not allowed in systems like Rust. Let's forbid it.
            elif not isinstance(param_t, RefType) and isinstance(arg_t, RefType):
                 raise TypeError(f"Argument {i+1} type mismatch for '{operator_node!r}': expected value type {param_t!r}, got reference type {arg_t!r}")
            # Check if passing a value where a reference is expected (already handled above)

            # Original type compatibility checks for non-reference types
            elif param_t == arg_t or param_t == T_ANY or arg_t == T_ANY:
                 # --- Move Semantics Check (Task 4.3 / 6.1) ---
                 # If the function expects a value (not a ref), we pass a variable (Symbol),
                 # and the variable's type is NOT copyable, it's a move.
                 is_copyable = is_copy_type(arg_t) # Use the helper function
                 # Only perform move check if it's a variable (Symbol) being passed
                 if not isinstance(param_t, RefType) and isinstance(operand_node, Symbol) and not is_copyable:
                     target_name_str = operand_node.value
                     borrow_state = borrow_env.get_state(operand_node) # Static state
                     # Ensure state exists for static tracking if needed later
                     if not borrow_state: borrow_state = borrow_env.ensure_state_in_current_scope(operand_node)

                     try:
                         # Runtime check: Is it valid and not borrowed?
                         # Need to ensure the variable actually exists in runtime before checking state
                         rt_val = runtime_env.get_runtime_value(target_name_str) # This raises NameError if not found
                         if not rt_val.is_valid():
                             raise TypeError(f"Argument {i+1}: Use of moved variable '{target_name_str}'")
                         if rt_val.state == MemoryState.BORROWED:
                             raise TypeError(f"Argument {i+1}: Cannot move '{target_name_str}' because it is borrowed")

                         # Perform the move in runtime simulation
                         # The actual move happens conceptually; runtime tracks the state change.
                         runtime_env.move_value(target_name_str) # Use a dedicated move method if available
                         # Mark static state as moved
                         if borrow_state: # Check if state was actually found/created
                              borrow_state.mark_as_moved()

                     except NameError:
                          # Variable exists in type env but not runtime? Could be a global primitive/func.
                          # Or an error in env setup. Assume okay if not found in runtime for now?
                          # Or raise error? Let's raise for clarity.
                          raise TypeError(f"Argument {i+1}: Variable '{target_name_str}' not found in runtime environment for move check.")
                     except (TypeError, RuntimeError, ValueError) as e:
                          # Catch specific runtime errors or TypeErrors raised during checks
                          raise TypeError(f"Argument {i+1}: Cannot move '{target_name_str}': {e}")

                 # If we reached here, the types matched based on the outer elif condition.
                 # No need for explicit continue, control flow will skip subsequent elifs.
                 pass # Correctly indented under the outer elif

            # --- Additional Compatibility Checks (Correctly aligned with the outer elif) ---
            elif param_t == T_FLOAT and arg_t == T_INT:
                continue # Allow Int -> Float conversion
            elif isinstance(param_t, (PairType, ListType)) and arg_t == T_NIL:
                continue # Allow Nil -> List/Pair
            else:
                # If none of the above, types are incompatible (Correctly aligned else)
                raise TypeError(f"Argument {i+1} type mismatch for '{operator_node!r}': expected {param_t!r}, got {arg_t!r}")

    # Updated to use type_env
    def _is_mutable_binding(self, node: ChengExpr, type_env: TypeEnvironment) -> bool:
        """Checks if the node refers to a symbol bound to a mutable variable."""
        if isinstance(node, Symbol):
            lookup_result = type_env.lookup(node) # Use type_env
            # If the variable exists in the environment, assume it's mutable
            # as there's no explicit 'let mut' vs 'let' distinction seen.
            return lookup_result is not None
        # Only symbols can be mutable bindings in this simple model.
        return False

    def _is_lvalue(self, node: ChengExpr) -> bool:
        """Checks if an expression node represents an l-value (can be borrowed/assigned to)."""
        # Simple version: only symbols are lvalues for now.
        # TODO: Extend to handle dereferences like (* (&mut x))
        return isinstance(node, Symbol)


# --- Helper Function (needs to be defined) --- # Corrected indentation at module level
def list_from_pair(pair_node: ChengExpr) -> List[ChengExpr]:
    """Converts a proper list represented by Pairs/Nil into a Python list."""
    if pair_node is Nil:
        return []
    if not isinstance(pair_node, Pair):
        raise TypeError("Invalid list structure: expected Pair or Nil")

    elements = []
    curr = pair_node
    while isinstance(curr, Pair):
        elements.append(curr.car)
        curr = curr.cdr
    if curr is not Nil:
        raise TypeError("Invalid list structure: not a proper list (dotted pair)")
    return elements


# --- Main entry point --- # Corrected indentation at module level
def check_types(program: ChengExpr) -> ChengType:
    """
    Performs type checking on the entire program AST.
    Returns the type of the program's result.
    """
    checker = TypeChecker()
    # TODO: Handle programs with multiple top-level expressions
    # Call check with default global environments
    return checker.check(program)
