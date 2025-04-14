import dataclasses
from typing import List, Union, Any, Optional, Tuple # Added Tuple

# --- Base Node ---
@dataclasses.dataclass(frozen=True) # Make nodes immutable and hashable
class ASTNode:
    """Base class for all AST nodes."""
    # Optional: Add line/column info later for error reporting
    # line: int = -1
    # col: int = -1
    pass

# --- Atoms ---
@dataclasses.dataclass(frozen=True)
class Symbol(ASTNode):
    """Represents a symbol (identifier)."""
    name: str

@dataclasses.dataclass(frozen=True)
class IntegerLiteral(ASTNode):
    """Represents an integer literal."""
    value: int

@dataclasses.dataclass(frozen=True)
class FloatLiteral(ASTNode):
    """Represents a floating-point literal."""
    value: float

@dataclasses.dataclass(frozen=True)
class StringLiteral(ASTNode):
    """Represents a string literal."""
    value: str

@dataclasses.dataclass(frozen=True)
class BoolLiteral(ASTNode):
    """Represents a boolean literal (#t or #f)."""
    value: bool

# --- Forward Declaration for Type Hinting ---
# We need this because some nodes contain other nodes (potentially recursively)
Expression = Any # Placeholder, will be refined below

# --- Specific Language Constructs ---

@dataclasses.dataclass(frozen=True)
class DefineNode(ASTNode):
    """Represents a variable definition: (define symbol value)."""
    symbol: Symbol
    value: Expression # The value being assigned

# Removed duplicate class definitions block

@dataclasses.dataclass(frozen=True)
class DefmacroNode(ASTNode):
    """Represents a macro definition: (defmacro name (param1 ... [. rest]) body)."""
    name: Symbol
    params: Tuple[Symbol, ...] # Regular parameter list for the macro (use tuple)
    body: Expression     # The body that generates code
    rest_param: Optional[Symbol] = None # Optional symbol for rest parameters (after dot)

@dataclasses.dataclass(frozen=True)
class IfNode(ASTNode):
    """Represents a conditional expression: (if condition then_branch else_branch)."""
    condition: Expression
    then_branch: Expression
    else_branch: Expression # Assuming else is mandatory for now

@dataclasses.dataclass(frozen=True)
class FunctionCallNode(ASTNode):
    """Represents a function call: (function_name arg1 arg2 ...)."""
    function: Expression # Could be a Symbol or another expression yielding a function
    arguments: Tuple[Expression, ...] = dataclasses.field(default_factory=tuple) # Use tuple

@dataclasses.dataclass(frozen=True)
class LambdaNode(ASTNode):
    """Represents a lambda expression: (lambda (param1 param2 ...) body)."""
    params: Tuple[Symbol, ...] # List of parameter symbols (use tuple)
    body: Expression     # Single expression for the body

@dataclasses.dataclass(frozen=True)
class LetBinding(ASTNode):
    """Helper for let bindings: (symbol value). Not directly parsed, but used in LetNode."""
    symbol: Symbol
    value: Expression

@dataclasses.dataclass(frozen=True)
class LetNode(ASTNode):
    """Represents a let expression: (let ((sym1 val1) (sym2 val2) ...) body)."""
    bindings: Tuple[LetBinding, ...] # List of binding pairs (use tuple)
    body: Expression           # Single expression for the body

@dataclasses.dataclass(frozen=True)
class SetBangNode(ASTNode):
    """Represents assignment: (set! symbol value)."""
    symbol: Symbol
    value: Expression

@dataclasses.dataclass(frozen=True)
class BeginNode(ASTNode):
    """Represents a sequence of expressions: (begin expr1 expr2 ...)."""
    expressions: Tuple[Expression, ...] = dataclasses.field(default_factory=tuple) # Use tuple

@dataclasses.dataclass(frozen=True)
class QuoteNode(ASTNode):
    """Represents a quoted expression: (quote expression) or 'expression."""
    expression: Expression # The expression being quoted

@dataclasses.dataclass(frozen=True)
class QuasiquoteNode(ASTNode):
    """Represents a quasiquoted expression: `expression."""
    expression: Expression # The expression being quasiquoted

@dataclasses.dataclass(frozen=True)
class UnquoteNode(ASTNode):
    """Represents an unquoted expression within a quasiquote: ,expression."""
    expression: Expression # The expression being unquoted

@dataclasses.dataclass(frozen=True)
class UnquoteSplicingNode(ASTNode):
    """Represents an unquote-splicing expression within a quasiquote: ,@expression."""
    expression: Expression # The expression being unquoted and spliced


# --- S-Expression (Generic List - may be phased out by specific nodes later) ---
@dataclasses.dataclass(frozen=True)
class SExpression(ASTNode):
    """Represents a generic S-expression list: (elem1 elem2 ...).
       Should ideally be replaced by specific nodes (like FunctionCallNode) during parsing.
    """
    elements: Tuple[Expression, ...] = dataclasses.field(default_factory=tuple) # Use tuple


# --- Refined Type Alias for Expressions ---
# Represents any valid expression or statement component in the AST
Expression = Union[
    Symbol, IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral,
    DefineNode, DefmacroNode, IfNode, FunctionCallNode, LambdaNode, LetNode, SetBangNode, BeginNode,
    QuoteNode, QuasiquoteNode, UnquoteNode, UnquoteSplicingNode, # Added UnquoteSplicingNode
    SExpression # Include SExpression for now
]


# --- Program ---
@dataclasses.dataclass(frozen=True)
class Program(ASTNode):
    """Represents the top-level program structure."""
    body: Tuple[Expression, ...] = dataclasses.field(default_factory=tuple) # Body contains Expressions (use tuple)


# --- Helper for pretty printing (updated) ---
def print_ast(node: ASTNode, indent: str = "") -> None:
    """Recursively prints the AST structure."""
    if isinstance(node, Program):
        print(f"{indent}Program:")
        for item in node.body:
            print_ast(item, indent + "  ")
    elif isinstance(node, DefineNode):
        print(f"{indent}DefineNode:")
        print_ast(node.symbol, indent + "  (Symbol)")
        print_ast(node.value, indent + "  (Value)")
    elif isinstance(node, DefmacroNode): # Add handler for DefmacroNode
        print(f"{indent}DefmacroNode:")
        print_ast(node.name, indent + "  (Name)")
        print(f"{indent}  (Params):")
        for param in node.params:
            print_ast(param, indent + "    ")
        if node.rest_param:
            print_ast(node.rest_param, indent + "  (Rest Param)")
        print_ast(node.body, indent + "  (Body)")
    elif isinstance(node, IfNode):
        print(f"{indent}IfNode:")
        print_ast(node.condition, indent + "  (Condition)")
        print_ast(node.then_branch, indent + "  (Then)")
        print_ast(node.else_branch, indent + "  (Else)")
    elif isinstance(node, LambdaNode):
        print(f"{indent}LambdaNode:")
        print(f"{indent}  (Params):")
        for param in node.params:
            print_ast(param, indent + "    ")
        print_ast(node.body, indent + "  (Body)")
    elif isinstance(node, LetNode):
        print(f"{indent}LetNode:")
        print(f"{indent}  (Bindings):")
        for binding in node.bindings:
             print(f"{indent}    Binding:")
             print_ast(binding.symbol, indent + "      (Symbol)")
             print_ast(binding.value, indent + "      (Value)")
        print_ast(node.body, indent + "  (Body)")
    elif isinstance(node, SetBangNode):
        print(f"{indent}SetBangNode:")
        print_ast(node.symbol, indent + "  (Symbol)")
        print_ast(node.value, indent + "  (Value)")
    elif isinstance(node, BeginNode):
        print(f"{indent}BeginNode:")
        for expr in node.expressions:
            print_ast(expr, indent + "  ")
    elif isinstance(node, QuoteNode):
        print(f"{indent}QuoteNode:")
        # Decide how to print quoted expression - maybe just its type/value for now?
        # Or recursively print, but mark it as quoted? Let's print recursively.
        print_ast(node.expression, indent + "  (Quoted)")
    elif isinstance(node, QuasiquoteNode): # Added handler
        print(f"{indent}QuasiquoteNode:")
        print_ast(node.expression, indent + "  (Quasiquoted)")
    elif isinstance(node, UnquoteNode): # Added handler
        print(f"{indent}UnquoteNode:")
        print_ast(node.expression, indent + "  (Unquoted)")
    elif isinstance(node, UnquoteSplicingNode): # Added handler for UnquoteSplicingNode
        print(f"{indent}UnquoteSplicingNode:")
        print_ast(node.expression, indent + "  (Spliced)")
    elif isinstance(node, FunctionCallNode):
        print(f"{indent}FunctionCallNode:")
        print_ast(node.function, indent + "  (Function)")
        if node.arguments:
            print(f"{indent}  (Arguments):")
            for arg in node.arguments:
                print_ast(arg, indent + "    ")
    elif isinstance(node, SExpression): # Keep for generic lists if parser doesn't specialize
        print(f"{indent}SExpression:")
        for item in node.elements:
            print_ast(item, indent + "  ")
    elif isinstance(node, Symbol):
        # Adjusted print format slightly for clarity within structures
        print(f"{indent}Symbol: {node.name}")
    elif isinstance(node, IntegerLiteral):
        print(f"{indent}Integer: {node.value}")
    elif isinstance(node, FloatLiteral):
        print(f"{indent}Float: {node.value}")
    elif isinstance(node, StringLiteral):
        # Adjusted print format slightly for clarity within structures
        print(f"{indent}String: \"{node.value}\"")
    elif isinstance(node, BoolLiteral):
        # Adjusted print format slightly for clarity within structures
        print(f"{indent}Bool: {'#t' if node.value else '#f'}")
    elif isinstance(node, list): # Handle potential lists within structures if type hints aren't perfect
        print(f"{indent}List:")
        for item in node:
             print_ast(item, indent + "  ")
    elif node is None:
         print(f"{indent}None")
    else:
        print(f"{indent}Unknown/Unhandled node type: {type(node)} ({node})")
