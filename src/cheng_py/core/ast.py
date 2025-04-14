# src/cheng_py/core/ast.py

from typing import Any, Optional, Union, List

# Forward declare ChengExpr for type hints
ChengExpr = Any # Placeholder

class ChengAstNode:
    """基础 AST 节点类"""
    # 可以添加行号/列号等元数据
    # line: int
    # col: int
    pass

# --- Atoms ---

class Symbol(ChengAstNode):
    def __init__(self, value: str):
        self.value = value
    def __repr__(self):
        # Represent symbols directly without the class name for cleaner output
        # return self.value
        return f"Symbol({self.value!r})" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, Symbol) and self.value == other.value
    def __hash__(self):
        return hash((Symbol, self.value))

class Integer(ChengAstNode):
    def __init__(self, value: int):
        self.value = value
    def __repr__(self):
        # return str(self.value)
        return f"Integer({self.value})" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, Integer) and self.value == other.value
    def __hash__(self):
        return hash((Integer, self.value))

class Float(ChengAstNode):
    def __init__(self, value: float):
        self.value = value
    def __repr__(self):
        # return str(self.value)
        return f"Float({self.value})" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, Float) and self.value == other.value
    def __hash__(self):
        return hash((Float, self.value))

class Boolean(ChengAstNode):
    def __init__(self, value: bool):
        self.value = value
    def __repr__(self):
        # return "true" if self.value else "false"
        return f"Boolean({self.value})" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, Boolean) and self.value == other.value
    def __hash__(self):
        return hash((Boolean, self.value))

class String(ChengAstNode):
    def __init__(self, value: str):
        self.value = value
    def __repr__(self):
        # Represent strings with double quotes, escaping internal quotes
        # return f'"{self.value.replace("\"", "\\\"")}"'
        return f"String({self.value!r})" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, String) and self.value == other.value
    def __hash__(self):
        return hash((String, self.value))

class NilType(ChengAstNode):
    """表示空列表 '() 的单例类型"""
    _instance = None
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(NilType, cls).__new__(cls)
        return cls._instance
    def __repr__(self):
        # return "()" # Represent Nil as ()
        return "Nil" # Restore representation for tests
    def __eq__(self, other):
        return isinstance(other, NilType)
    def __hash__(self):
        return hash(NilType)

Nil = NilType() # 空列表的单例实例

# --- Parameter ---
class Param(ChengAstNode):
    """Represents a function parameter (just a name)."""
    def __init__(self, name: Symbol):
        self.name = name
    def __repr__(self):
        return repr(self.name) # Represent param just by its name symbol
        # return f"Param({self.name!r})" # Old representation
    def __eq__(self, other):
        return isinstance(other, Param) and self.name == other.name

# --- Expressions ---

class LambdaExpr(ChengAstNode):
    """Represents a lambda expression: ((params...) body)"""
    def __init__(self, params: List[Param], body: ChengExpr):
        self.params = params # List of Param nodes
        self.body = body
    def __repr__(self):
        params_repr = ' '.join(map(repr, self.params))
        body_repr = repr(self.body)
        # Represent lambda using the new syntax
        return f"(({params_repr}) {body_repr})"
    def __eq__(self, other):
        return (isinstance(other, LambdaExpr) and
                self.params == other.params and
                self.body == other.body)

class DefineMacroExpr(ChengAstNode):
    """Represents a macro definition: name = `(params... [. variadic_param]) body"""
    def __init__(self, name: Symbol, params: List[Param], body: ChengExpr, variadic_param: Optional[Param] = None):
        self.name = name
        self.params = params # List of regular Param nodes
        self.variadic_param = variadic_param # Optional Param node for the rest parameter
        self.body = body
    def __repr__(self):
        params_repr_list = [repr(p) for p in self.params]
        if self.variadic_param:
            params_repr_list.append('.')
            params_repr_list.append(repr(self.variadic_param))
        params_repr = ' '.join(params_repr_list)
        body_repr = repr(self.body)
        # Represent using the new syntax
        return f"({self.name!r} = `({params_repr}) {body_repr})"
    def __eq__(self, other):
        return (isinstance(other, DefineMacroExpr) and
                self.name == other.name and
                self.params == other.params and
                self.variadic_param == other.variadic_param and
                self.body == other.body)

class AssignmentExpr(ChengAstNode):
    """Represents variable assignment/redefinition: target = value"""
    def __init__(self, target: Symbol, value: ChengExpr):
        self.target = target # Renamed from 'name'
        self.value = value
    def __repr__(self):
        # Represent using infix syntax
        return f"({self.target!r} = {self.value!r})"
    def __eq__(self, other):
        return (isinstance(other, AssignmentExpr) and
                self.target == other.target and
                self.value == other.value)

class ApplyExpr(ChengAstNode):
    """Represents function application: operator(operand ...)"""
    def __init__(self, operator: ChengExpr, operands: List[ChengExpr]):
        self.operator = operator
        self.operands = operands
    def __repr__(self):
        # Represent using call syntax
        operands_repr = ' '.join(map(repr, self.operands)) # Space separated args
        return f"({self.operator!r} {operands_repr})" # Keep outer parens for now? Or operator(args)? Let's stick to S-expr like for now.
        # Alternative: return f"{self.operator!r}({operands_repr})"
    def __eq__(self, other):
        return (isinstance(other, ApplyExpr) and
                self.operator == other.operator and
                self.operands == other.operands)

class IfExpr(ChengAstNode):
    """Represents: if condition then_branch else else_branch"""
    def __init__(self, condition: ChengExpr, then_branch: ChengExpr, else_branch: ChengExpr): # else_branch is now mandatory Nil if absent
        self.condition = condition
        self.then_branch = then_branch
        self.else_branch = else_branch
    def __repr__(self):
        # Represent using keyword syntax
        if self.else_branch is Nil:
            return f"(if {self.condition!r} {self.then_branch!r})"
        else:
            return f"(if {self.condition!r} {self.then_branch!r} else {self.else_branch!r})"
    def __eq__(self, other):
        return (isinstance(other, IfExpr) and
                self.condition == other.condition and
                self.then_branch == other.then_branch and
                self.else_branch == other.else_branch)

class TernaryExpr(ChengAstNode):
    """Represents the ternary conditional operator: condition ? true_expr : false_expr"""
    def __init__(self, condition: ChengExpr, true_expr: ChengExpr, false_expr: ChengExpr):
        self.condition = condition
        self.true_expr = true_expr
        self.false_expr = false_expr
    def __repr__(self):
        # Represent using infix syntax
        return f"({self.condition!r} ? {self.true_expr!r} : {self.false_expr!r})"
    def __eq__(self, other):
        return (isinstance(other, TernaryExpr) and
                self.condition == other.condition and
                self.true_expr == other.true_expr and
                self.false_expr == other.false_expr)

class PrefixExpr(ChengAstNode):
    """Represents prefix operations: op operand (e.g., -x, &x, *x)"""
    def __init__(self, operator: str, right: ChengExpr):
        self.operator = operator # Store operator as string ('-', '&', '*')
        self.right = right
    def __repr__(self):
        return f"({self.operator}{self.right!r})" # e.g., (- 5)
    def __eq__(self, other):
        return (isinstance(other, PrefixExpr) and
                self.operator == other.operator and
                self.right == other.right)

class BinaryExpr(ChengAstNode):
    """Represents binary infix operations: left op right"""
    def __init__(self, left: ChengExpr, operator: str, right: ChengExpr):
        self.left = left
        self.operator = operator # Store operator as string ('+', '==', etc.)
        self.right = right
    def __repr__(self):
        # Represent using infix notation within parentheses for clarity
        return f"({self.left!r} {self.operator} {self.right!r})"
    def __eq__(self, other):
        return (isinstance(other, BinaryExpr) and
                self.left == other.left and
                self.operator == other.operator and
                self.right == other.right)

class IndexExpr(ChengAstNode):
    """Represents sequence indexing: sequence[index]"""
    def __init__(self, sequence: ChengExpr, index: ChengExpr):
        self.sequence = sequence
        self.index = index
    def __repr__(self):
        # Removed outer parentheses
        return f"{self.sequence!r}[{self.index!r}]"
    def __eq__(self, other):
        return (isinstance(other, IndexExpr) and
                self.sequence == other.sequence and
                self.index == other.index)

class SliceExpr(ChengAstNode):
    """Represents sequence slicing: sequence[start:end]"""
    def __init__(self, sequence: ChengExpr, start: Optional[ChengExpr], end: Optional[ChengExpr]):
        self.sequence = sequence
        self.start = start # Can be None
        self.end = end   # Can be None
    def __repr__(self):
        start_repr = repr(self.start) if self.start is not None else ""
        end_repr = repr(self.end) if self.end is not None else ""
        # Removed outer parentheses
        return f"{self.sequence!r}[{start_repr}:{end_repr}]"
    def __eq__(self, other):
        return (isinstance(other, SliceExpr) and
                self.sequence == other.sequence and
                self.start == other.start and
                self.end == other.end)

class ListLiteralExpr(ChengAstNode):
    """Represents list literals: (elem1 elem2 ...)"""
    def __init__(self, elements: List[ChengExpr]):
        self.elements = elements
    def __repr__(self):
        elements_repr = ' '.join(map(repr, self.elements))
        return f"({elements_repr})" # Represent using square brackets
    def __eq__(self, other):
        return (isinstance(other, ListLiteralExpr) and
                self.elements == other.elements)

class SequenceExpr(ChengAstNode):
    """Represents sequences of expressions: expr1; expr2; ..."""
    def __init__(self, expressions: List[ChengExpr]):
        self.expressions = expressions
    def __repr__(self):
        # Represent with semicolons, maybe within a 'begin' block?
        expr_repr = '; '.join(map(repr, self.expressions))
        return f"(begin {expr_repr})" # Use 'begin' for clarity
    def __eq__(self, other):
        return (isinstance(other, SequenceExpr) and
                self.expressions == other.expressions)

class QuoteExpr(ChengAstNode):
    """Represents (quote datum) or 'datum"""
    def __init__(self, datum: ChengExpr):
        self.datum = datum
    def __repr__(self):
        # Use ' syntax? Or keep (quote ...)? Let's keep (quote ...) for now.
        return f"(quote {self.datum!r})"
    def __eq__(self, other):
        return isinstance(other, QuoteExpr) and self.datum == other.datum

class QuasiquoteExpr(ChengAstNode):
    """Represents `datum"""
    def __init__(self, expression: ChengExpr):
        self.expression = expression
    def __repr__(self):
        return f"`{self.expression!r}"
    def __eq__(self, other):
        return isinstance(other, QuasiquoteExpr) and self.expression == other.expression

class UnquoteExpr(ChengAstNode):
    """Represents ,datum"""
    def __init__(self, expression: ChengExpr):
        self.expression = expression
    def __repr__(self):
        return f",{self.expression!r}"
    def __eq__(self, other):
        return isinstance(other, UnquoteExpr) and self.expression == other.expression

class UnquoteSplicingExpr(ChengAstNode):
    """Represents ,@datum"""
    def __init__(self, expression: ChengExpr):
        self.expression = expression
    def __repr__(self):
        return f",@{self.expression!r}"
    def __eq__(self, other):
        return isinstance(other, UnquoteSplicingExpr) and self.expression == other.expression


# --- Cons Cell (Used by evaluator/runtime, not directly by new parser for lists) ---
class Pair(ChengAstNode):
    """Cons Cell / 对，用于构建运行时列表（可能与 ListLiteralExpr 不同）"""
    def __init__(self, car: ChengAstNode, cdr: ChengAstNode):
        self.car = car # 第一个元素 (head)
        self.cdr = cdr # 剩余部分 (tail)

    def __repr__(self):
        # Restore simpler representation for AST tests
        # This might need adjustment later if cycles become an issue in AST repr
        items = []
        curr = self
        # Simple cycle detection to avoid infinite loops in basic cases
        seen_ids = {id(self)}

        while isinstance(curr, Pair):
            items.append(repr(curr.car))
            curr = curr.cdr
            if id(curr) in seen_ids:
                items.append("...") # Indicate cycle
                return f"({' '.join(items)})"
            seen_ids.add(id(curr))
            # Limit depth for safety in case of very long non-cyclic lists
            if len(items) > 20:
                items.append("...")
                break

        if curr is Nil:
            # Proper list
            return f"({' '.join(items)})"
        else:
            # Dotted pair
            items.append(".")
            items.append(repr(curr))
            return f"({' '.join(items)})"

        # Old robust representation:
        # items = []
        # curr = self
        seen = set() # For cycle detection
        while isinstance(curr, Pair):
            if id(curr) in seen:
                 items.append("...") # Cycle detected
                 return f"({' '.join(items)}) # CycleDetected"
            seen.add(id(curr))
            items.append(repr(curr.car))
            curr = curr.cdr
            if len(items) > 50: # Limit depth for safety
                 items.append("...")
                 return f"({' '.join(items)}) # DepthLimited"

        # After the loop, curr is the final cdr
        if curr is Nil:
            # Proper list
            return f"({' '.join(items)})"
        else:
            # Dotted pair
            items.append(".")
            items.append(repr(curr))
            return f"({' '.join(items)})"

    def __eq__(self, other):
        # Basic equality check, might need adjustment for cycles if hashing is needed
        print(f"DEBUG Pair.__eq__: Comparing self={repr(self)} (id={id(self)}) with other={repr(other)} (id={id(other)})") # DEBUG
        if not isinstance(other, Pair):
            print(f"DEBUG Pair.__eq__: Other is not Pair (type={type(other)}). Returning False.") # DEBUG
            return False
        # Recursive comparison
        car_eq = self.car == other.car
        print(f"DEBUG Pair.__eq__: Comparing car: self.car={repr(self.car)} vs other.car={repr(other.car)}. Result: {car_eq}") # DEBUG
        if not car_eq:
            print(f"DEBUG Pair.__eq__: Cars not equal. Returning False.") # DEBUG
            return False
        cdr_eq = self.cdr == other.cdr
        print(f"DEBUG Pair.__eq__: Comparing cdr: self.cdr={repr(self.cdr)} vs other.cdr={repr(other.cdr)}. Result: {cdr_eq}") # DEBUG
        print(f"DEBUG Pair.__eq__: Returning {cdr_eq}") # DEBUG
        return cdr_eq
    # Pair 是可变的，通常不建议哈希，但如果需要可以实现
    def __hash__(self):
        # Pairs are mutable, so they shouldn't be hashable for use in sets/dicts.
        # Raise TypeError if hashing is attempted.
        raise TypeError("Pair objects are mutable and cannot be hashed")

# --- Runtime Values (Closures, References, etc.) ---
# 需要前向声明 Environment
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from .environment import Environment

class Closure(ChengAstNode):
    """表示用户定义的函数（闭包） - Runtime value"""
    def __init__(self, params: List[Param], body: ChengExpr, definition_env: 'Environment'):
        self.params: List[Param] = params
        self.body = body # 函数体表达式
        self.definition_env = definition_env # 函数定义时的环境（用于词法作用域）

    def __repr__(self):
        param_names = ' '.join(p.name.value for p in self.params)
        return f"<Closure ({param_names})>"

class Reference(ChengAstNode):
    """Represents a runtime reference to a variable in a specific environment."""
    def __init__(self, variable_name: str, environment: 'Environment'):
        if not isinstance(variable_name, str):
             raise TypeError(f"Reference variable_name must be a string, got {type(variable_name)}")
        # We need to import Environment properly here. The forward declaration is already present.
        self.variable_name: str = variable_name
        self.environment: 'Environment' = environment # The environment where the variable is defined

    def __repr__(self) -> str:
        # Avoid showing the whole environment in repr for clarity
        env_id = id(self.environment)
        return f"<Reference to '{self.variable_name}' in env@{env_id}>"

    def __eq__(self, other):
        # References are equal if they point to the same variable in the same environment instance
        return isinstance(other, Reference) and \
               self.variable_name == other.variable_name and \
               self.environment is other.environment # Identity check for environment

    # References are tied to specific environments and variable states, making them mutable in effect.
    # They should not be hashable.
    def __hash__(self):
        raise TypeError("Reference objects are mutable and cannot be hashed")

# --- Type Aliases ---
ChengAtom = Union[Symbol, Integer, Float, Boolean, String, NilType]

# Define the final ChengExpr type alias, including all valid AST nodes
ChengExpr = Union[
    # Atoms
    Symbol, Integer, Float, Boolean, String, NilType,
    # Core Structures
    Param, LambdaExpr, DefineMacroExpr, AssignmentExpr, ApplyExpr,
    IfExpr, TernaryExpr, PrefixExpr, BinaryExpr, IndexExpr, SliceExpr, ListLiteralExpr,
    SequenceExpr, QuoteExpr, QuasiquoteExpr, UnquoteExpr, UnquoteSplicingExpr,
    # Runtime values (might move out of core AST later)
    Pair, Closure, Reference
]

# Removed: DefineExpr, LetBinding, LetExpr, LetRecExpr, TypeSyntax
