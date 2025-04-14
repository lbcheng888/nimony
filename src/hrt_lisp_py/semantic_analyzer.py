import dataclasses
from typing import Dict, Optional, List, Any, Tuple # Added Tuple

# Import shared types
try:
    from .types import Type, IntType, FloatType, BoolType, StringType, VoidType, AnyType, ErrorType, FunctionType
except ImportError:
    from src.hrt_lisp_py.types import Type, IntType, FloatType, BoolType, StringType, VoidType, AnyType, ErrorType, FunctionType

# Adjust import path similarly to parser.py
try:
    from .ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode, LetBinding,
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, # Added Quasiquote/Unquote
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression
    )
except ImportError:
    from src.hrt_lisp_py.ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode, LetBinding,
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, # Added Quasiquote/Unquote
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression
    )


# --- Semantic Error ---
class SemanticError(Exception):
    """Custom exception for semantic errors found during analysis."""
    pass

# --- Environment (Symbol Table) ---
@dataclasses.dataclass
class Environment:
    """Manages scopes, symbol definitions (with types), and macro definitions."""
    # Store the Type object associated with each variable/function name
    bindings: Dict[str, Type] = dataclasses.field(default_factory=dict)
    # Store the macro definition node (e.g., DefmacroNode)
    macros: Dict[str, DefmacroNode] = dataclasses.field(default_factory=dict)
    # Link to the enclosing (parent) environment
    enclosing: Optional['Environment'] = None

    def define_variable(self, name: str, var_type: Type):
        """Define a variable or function with its type in the current scope."""
        if not isinstance(var_type, Type):
             raise TypeError(f"Attempted to define variable '{name}' with non-Type value: {var_type}")
        # Prevent defining a variable with the same name as a macro in the *same* scope
        if name in self.macros:
            raise SemanticError(f"Cannot define variable '{name}', a macro with this name exists in the current scope.")
        # TODO: Handle variable redefinition rules (allow shadowing?)
        if name in self.bindings:
             print(f"Warning: Shadowing previous definition of variable/function '{name}' in the same scope.")
        self.bindings[name] = var_type

    def define_macro(self, name: str, macro_node: DefmacroNode):
        """Define a macro in the current scope."""
        if not isinstance(macro_node, DefmacroNode):
             raise TypeError(f"Attempted to define macro '{name}' with non-DefmacroNode value: {macro_node}")
        # Prevent defining a macro with the same name as a variable/function in the *same* scope
        if name in self.bindings:
            raise SemanticError(f"Cannot define macro '{name}', a variable/function with this name exists in the current scope.")
        # TODO: Handle macro redefinition rules (allow shadowing?)
        if name in self.macros:
             print(f"Warning: Redefining macro '{name}' in the same scope.")
        self.macros[name] = macro_node

    def lookup_variable_type(self, name: str) -> Optional[Type]:
        """Look up a symbol's type, searching current and enclosing scopes."""
        if name in self.bindings:
            return self.bindings[name]
        elif self.enclosing:
            return self.enclosing.lookup_variable_type(name)
        else:
            return None # Variable not found

    def lookup_macro(self, name: str) -> Optional[DefmacroNode]:
        """Look up a macro definition, searching current and enclosing scopes."""
        # Corrected implementation: Search the 'macros' dictionary recursively.
        if name in self.macros:
            return self.macros[name]
        elif self.enclosing:
            return self.enclosing.lookup_macro(name)
        else:
            return None # Macro not found

    def _find_defining_env(self, name: str) -> Optional['Environment']:
         """Helper to find the environment where a variable is defined."""
         if name in self.bindings:
             return self
         elif self.enclosing:
             return self.enclosing._find_defining_env(name)
         else:
             return None

# --- Semantic Analyzer (AST Visitor) ---
class SemanticAnalyzer:
    """Performs semantic analysis (e.g., scope checking) on the AST."""

    def __init__(self):
        # Start with a global environment
        self.current_env = Environment()
        # Pre-populate the global environment with known built-ins/primitives and their types
        # TODO: Refine these types (e.g., specific int/float sizes, polymorphism)
        builtin_types = {
            # Arithmetic (Placeholder: Int -> Int -> Int)
            '+': FunctionType(name="function", param_types=(IntType, IntType), return_type=IntType),
            '-': FunctionType(name="function", param_types=(IntType, IntType), return_type=IntType), # Also unary later
            '*': FunctionType(name="function", param_types=(IntType, IntType), return_type=IntType),
            '/': FunctionType(name="function", param_types=(IntType, IntType), return_type=IntType), # Integer division?
            # Comparisons (Placeholder: Any -> Any -> Bool) - Needs refinement
            '<': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            '>': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            '=': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            '<=': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            '>=': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            '!=': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType),
            'eq?': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=BoolType), # Reference equality?
            # Basic I/O (Placeholder: Any -> Void)
            'print': FunctionType(name="function", param_types=(AnyType,), return_type=VoidType), # Added name
            # List operations (Placeholder - needs proper list type)
            'list': FunctionType(name="function", param_types=(AnyType,), return_type=AnyType), # Variadic later, Added name
            'cons': FunctionType(name="function", param_types=(AnyType, AnyType), return_type=AnyType), # Added name
            'car': FunctionType(name="function", param_types=(AnyType,), return_type=AnyType), # Added name
            'cdr': FunctionType(name="function", param_types=(AnyType,), return_type=AnyType), # Added name
            'null?': FunctionType(name="function", param_types=(AnyType,), return_type=BoolType), # Added name
        }
        for name, type_obj in builtin_types.items():
            # Ensure the type object itself has a name before defining
            if not hasattr(type_obj, 'name') or not type_obj.name:
                 # This case might happen if we accidentally create a Type without a name
                 raise TypeError(f"Internal error: Builtin type for '{name}' is missing a name attribute.")
            self.current_env.define_variable(name, var_type=type_obj)
        # Store types of expressions (ASTNode -> Type)
        self.expression_types: Dict[ASTNode, Type] = {}

    def analyze(self, node: ASTNode):
        """Public entry point to start analysis."""
        self.visit(node)

    def visit(self, node: ASTNode, env: Optional[Environment] = None):
        """Generic visit method (Visitor pattern dispatcher)."""
        # Use the provided environment or the analyzer's current one
        active_env = env if env is not None else self.current_env
        method_name = f'visit_{type(node).__name__}'
        visitor = getattr(self, method_name, self.generic_visit)
        # Pass the active environment to the visitor method
        result_type = visitor(node, active_env)
        # Store the determined type for this node
        if isinstance(node, Expression): # Only store types for expression nodes
             self.expression_types[node] = result_type
        return result_type

    def lookup_expression_type(self, node: ASTNode) -> Optional[Type]:
        """Retrieves the previously stored type for an expression node."""
        return self.expression_types.get(node)

    def generic_visit(self, node: ASTNode, env: Environment):
        """Default visitor for nodes without specific handling."""
        # Most nodes just require visiting their children recursively
        for field in dataclasses.fields(node):
            value = getattr(node, field.name)
            if isinstance(value, ASTNode):
                self.visit(value, env)
            elif isinstance(value, list):
                for item in value:
                    if isinstance(item, ASTNode):
                        self.visit(item, env)
            # Handle LetBinding specifically if needed, though generic might cover it
            elif isinstance(value, LetBinding):
                 self.visit(value.value, env) # Visit the value part of the binding

    # --- Visitor Methods for Specific Node Types ---

    def visit_Program(self, node: Program, env: Environment):
        """Visit all top-level expressions in the program."""
        for expr in node.body:
            self.visit(expr, env) # Use the initial global environment

    def visit_DefineNode(self, node: DefineNode, env: Environment) -> Type:
        """Analyze definition, define symbol with its type, and return VoidType."""
        symbol_name = node.symbol.name

        # Check for redefinition in the *same* scope (global in this case)
        # Note: define_variable/define_macro now handle warnings/errors internally
        # if symbol_name in env.bindings and env.enclosing is None: ...
        # if symbol_name in env.macros and env.enclosing is None: ...

        if isinstance(node.value, LambdaNode):
            # Function definition:
            # 1. Tentatively define the function with a placeholder type to allow recursion
            #    A more robust approach might involve multiple passes or forward declarations.
            #    For now, let's define it with AnyType initially.
            # TODO: Handle potential redefinition/shadowing based on language rules
            temp_func_type = AnyType # Placeholder
            env.define_variable(symbol_name, temp_func_type)

            # 2. Analyze the lambda to get its actual type
            actual_func_type = self.visit(node.value, env)
            if not isinstance(actual_func_type, FunctionType):
                 # This shouldn't happen if visit_LambdaNode works correctly
                 raise SemanticError(f"Internal error: LambdaNode analysis did not return a FunctionType for '{symbol_name}'.")

            # 3. Redefine the variable with the correct FunctionType
            env.define_variable(symbol_name, actual_func_type)

        elif isinstance(node.value, DefmacroNode):
             # Macro definition: Define the macro name. Type analysis of body is deferred.
             env.define_macro(symbol_name, node.value)
             # Don't visit the DefmacroNode's body here.
        else:
            # Standard variable definition:
            # 1. Analyze the value to get its type
            value_type = self.visit(node.value, env)
            if value_type is ErrorType:
                 # Propagate type errors
                 print(f"Error analyzing value for variable '{symbol_name}'.")
                 # Define with ErrorType to prevent cascade errors? Or stop?
                 env.define_variable(symbol_name, ErrorType)
            else:
                 # 2. Define the variable with the inferred type
                 env.define_variable(symbol_name, value_type)

        # Define statements don't have a value themselves in expression context
        return VoidType

    def visit_DefmacroNode(self, node: DefmacroNode, env: Environment) -> Type:
        """Handle macro definitions."""
        # Macros are defined before their body is analyzed in detail (usually)
        # The body itself is often treated like quoted data until expansion time.
        macro_name = node.name.name
        # Check for redefinition in the *same* scope
        if macro_name in env.macros:
             print(f"Warning: Redefining macro '{macro_name}' in the same scope.")
        # Use the specific define method, store the whole node for now
        env.define_macro(macro_name, node)
        # We don't typically analyze the macro body for scopes at definition time,
        # only when it's expanded. We also don't visit the name symbol.
        # We might want to check the parameter list for duplicates later.
        # Macros don't have a runtime type in the same way variables do.
        # The definition itself doesn't produce a value.
        return VoidType

    def visit_Symbol(self, node: Symbol, env: Environment) -> Type:
        """Look up the type of a symbol used as a variable/function."""
        symbol_type = env.lookup_variable_type(node.name)
        if symbol_type is None:
            # Check if it might be a macro name (macros don't have types in the same way)
            if env.lookup_macro(node.name) is not None:
                 # Using a macro name directly as a value is likely an error
                 raise SemanticError(f"Cannot use macro name '{node.name}' as a value.")
            else:
                 raise SemanticError(f"Undefined variable/function: '{node.name}'")
        # TODO: Check if symbol_type is ErrorType and handle?
        return symbol_type

    def visit_LambdaNode(self, node: LambdaNode, env: Environment) -> Type:
        """Analyze lambda parameters and body to determine FunctionType."""
        # Create a new environment for the lambda's scope
        lambda_env = Environment(enclosing=env)

        # Analyze parameter types (use AnyType for now, needs type annotations later)
        param_types = []
        for param in node.params:
            # TODO: Check for duplicate parameter names
            # TODO: Use type annotations on params if available
            param_type = AnyType # Placeholder
            lambda_env.define_variable(param.name, param_type)
            param_types.append(param_type)

        # Analyze the lambda body within the new environment to find return type
        # The type of the body *is* the return type
        return_type = self.visit(node.body, lambda_env)

        # Construct and return the FunctionType, providing a name for the lambda
        return FunctionType(name="<lambda>", param_types=tuple(param_types), return_type=return_type)


    def visit_LetNode(self, node: LetNode, env: Environment):
        """Create a new scope for let bindings and visit the body."""
        let_env = Environment(enclosing=env)
        # Analyze and define bindings. Assuming parallel 'let' semantics.
        binding_types: Dict[str, Type] = {}
        for binding in node.bindings:
             # 1. Analyze the value in the *outer* environment
             value_type = self.visit(binding.value, env)
             # TODO: Check for duplicate binding names
             if binding.symbol.name in binding_types:
                  raise SemanticError(f"Duplicate binding name '{binding.symbol.name}' in let.")
             binding_types[binding.symbol.name] = value_type

        # 2. Define all symbols with their inferred types in the new environment
        for name, var_type in binding_types.items():
             let_env.define_variable(name, var_type)

        # 3. Visit the let body within the new environment and return its type
        body_type = self.visit(node.body, let_env)
        return body_type


    def visit_SetBangNode(self, node: SetBangNode, env: Environment):
        """Check if the assigned variable exists and visit the value."""
        """Analyze assignment, check type compatibility, return VoidType."""
        symbol_name = node.symbol.name

        # 1. Check if the variable exists and get its declared type
        target_var_type = env.lookup_variable_type(symbol_name)
        if target_var_type is None:
             if env.lookup_macro(symbol_name) is not None:
                 raise SemanticError(f"Cannot set! '{symbol_name}' is defined as a macro, not a variable.")
             else:
                 raise SemanticError(f"Cannot set! undefined variable: '{symbol_name}'")

        # 2. Analyze the value expression to get its type
        value_type = self.visit(node.value, env)

        # 3. Check for type compatibility (basic check for now)
        # TODO: Implement proper type compatibility rules (e.g., subtypes, coercions?)
        if value_type != target_var_type and value_type != AnyType and target_var_type != AnyType:
             # Allow assigning 'any' to anything, or anything to 'any' for now
             raise SemanticError(f"Type mismatch in set!: Cannot assign type '{value_type}' to variable '{symbol_name}' of type '{target_var_type}'.")

        # Removed call to non-existent env.assign. Existence check is done by lookup_variable_type above.
        # Future checks might involve constness etc.

        # set! statement itself doesn't produce a value
        return VoidType


    def visit_IfNode(self, node: IfNode, env: Environment) -> Type:
        """Analyze condition and branches, determine expression type."""
        # 1. Analyze condition - must be boolean
        cond_type = self.visit(node.condition, env)
        # TODO: Allow coercions? For now, strict bool check.
        if cond_type != BoolType and cond_type != AnyType:
             raise SemanticError(f"If condition must be boolean, got '{cond_type}'.")

        # 2. Analyze branches
        then_type = self.visit(node.then_branch, env)
        else_type = self.visit(node.else_branch, env)

        # 3. Determine result type - branches must match (for now)
        # TODO: Implement type unification/compatibility rules (e.g., find common supertype?)
        if then_type == else_type:
            return then_type
        elif then_type == AnyType: # If one branch is Any, result is the other branch's type
             return else_type
        elif else_type == AnyType:
             return then_type
        elif then_type == ErrorType or else_type == ErrorType:
             return ErrorType # Propagate errors
        else:
            # TODO: Consider VoidType compatibility?
            raise SemanticError(f"Type mismatch in if branches: then branch has type '{then_type}', else branch has type '{else_type}'.")


    def visit_BeginNode(self, node: BeginNode, env: Environment) -> Type:
        """Analyze sequence of expressions, return type of the last one."""
        last_expr_type: Type = VoidType # Default if empty
        if not node.expressions:
             return VoidType

        for i, expr in enumerate(node.expressions):
            expr_type = self.visit(expr, env)
            if i == len(node.expressions) - 1:
                last_expr_type = expr_type
            # TODO: Check if intermediate expressions have side effects if their type is not Void?

        return last_expr_type


    def visit_FunctionCallNode(self, node: FunctionCallNode, env: Environment) -> Type:
        """Analyze function call, check types, and return the result type."""

        # 1. Check for macro call attempt (should be handled by expansion phase)
        if isinstance(node.function, Symbol):
            if env.lookup_macro(node.function.name) is not None:
                # In a full compiler, macro expansion runs before type checking.
                # If we encounter a macro name here, it means expansion didn't happen
                # or something went wrong.
                raise SemanticError(f"Attempting to call macro '{node.function.name}' as a function. Macro expansion should precede type checking.")

        # 2. Analyze the type of the expression in the function position
        func_type = self.visit(node.function, env)

        # 3. Check if it's a callable type (FunctionType)
        if not isinstance(func_type, FunctionType):
             # Allow calling 'AnyType' for now, assuming it might be a function
             if func_type == AnyType:
                 # If we call 'Any', we can't check args, and result is 'Any'
                 # Analyze arguments just to catch errors within them
                 for arg in node.arguments:
                     self.visit(arg, env)
                 return AnyType
             elif func_type == ErrorType:
                 return ErrorType # Propagate error
             else:
                 raise SemanticError(f"Cannot call non-function type '{func_type}'.")

        # 4. Analyze argument types
        arg_types = [self.visit(arg, env) for arg in node.arguments]

        # 5. Check argument count
        # TODO: Handle variadic functions later
        if len(arg_types) != len(func_type.param_types):
            raise SemanticError(f"Function call arity mismatch: Expected {len(func_type.param_types)} arguments, got {len(arg_types)} for function type {func_type}.")

        # 6. Check argument types against parameter types
        for i, (arg_type, param_type) in enumerate(zip(arg_types, func_type.param_types)):
            # TODO: Implement proper type compatibility rules
            if arg_type != param_type and arg_type != AnyType and param_type != AnyType and arg_type != ErrorType:
                 # Allow AnyType compatibility and ignore errors already found
                 raise SemanticError(f"Type mismatch in function call argument {i+1}: Expected type '{param_type}', got '{arg_type}'.")

        # 7. Return the function's declared return type
        return func_type.return_type


    def visit_QuoteNode(self, node: QuoteNode, env: Environment):
        """Do not analyze inside a quoted expression."""
        # Quoted expressions are treated as data, so we don't check scopes inside.
        pass # Intentionally do nothing

    def visit_QuasiquoteNode(self, node: QuasiquoteNode, env: Environment):
        """Analyze inside a quasiquoted expression.
           During initial analysis (macro definition phase), we just traverse.
           The real quasiquote logic happens during expansion.
           During this initial pass, treat it like quote - don't analyze inside.
        """
        pass # Do not analyze contents during initial pass

    def visit_UnquoteNode(self, node: UnquoteNode, env: Environment):
        """Analyze inside an unquoted expression.
           Similar to quasiquote, during initial analysis, treat like quote.
        """
        pass # Do not analyze contents during initial pass

    # --- Visitors for Literals (Return Type) ---
    def visit_IntegerLiteral(self, node: IntegerLiteral, env: Environment) -> Type:
        # TODO: Determine specific int type (i32, i64) based on value or context?
        return IntType

    def visit_FloatLiteral(self, node: FloatLiteral, env: Environment) -> Type:
        # TODO: Determine specific float type (f32, f64) based on value or context?
        return FloatType

    def visit_StringLiteral(self, node: StringLiteral, env: Environment) -> Type:
        # Represents a static/stack string
        return StringType

    def visit_BoolLiteral(self, node: BoolLiteral, env: Environment) -> Type:
        return BoolType

    def visit_SExpression(self, node: SExpression, env: Environment):
        """
        Handles SExpression nodes encountered during semantic analysis.
        After macro expansion, SExpressions often represent literal data lists
        generated by quasiquote (e.g., `(a b c)` expands to SExpression(Symbol(a), Symbol(b), Symbol(c))).
        We should NOT analyze the contents of these SExpressions as if they were
        executable code in the current scope (e.g., don't try to lookup 'a').
        The only standard SExpression that typically appears directly in code is '().
        """
        # If it's the empty list '()', it's valid.
        if not node.elements:
            pass # Valid empty list, nothing to analyze within it.
        else:
            # If it's a non-empty SExpression resulting from macro expansion (like `(a b c)`),
            # treat it as literal data. Do not visit its children for scope checking.
            # print(f"Note: Skipping semantic analysis inside non-empty SExpression (likely literal data): {node}")
            pass # Intentionally do nothing with the contents


# --- Helper Function ---
def analyze_semantics(program_ast: Program) -> None:
    """Analyzes the scope and type rules for a given Program AST."""
    analyzer = SemanticAnalyzer()
    print("Starting semantic analysis (scopes and types)...")
    try:
        # The top-level visit doesn't explicitly return a type here,
        # but errors will be raised if analysis fails.
        analyzer.analyze(program_ast)
        print("Semantic analysis successful.")
    except SemanticError as e:
        print(f"Semantic Error: {e}")
        # Optionally re-raise or exit
        raise e
    except Exception as e:
        print(f"An unexpected error occurred during semantic analysis: {e}")
        raise e


# --- Example Usage (can be added later) ---
if __name__ == '__main__':
    # Example demonstrating basic type checking
    from .parser import parse # Assuming parser is available

    test_code_ok = """
    (define (add x y) (+ x y))
    (define result (add 5 10)) ; result should be int
    (define is-good (> result 0)) ; is-good should be bool
    (if is-good (print "Good") (print "Bad"))
    """

    test_code_type_error = """
    (define (add x y) (+ x y))
    (add 5 "hello") ; Type error: trying to add int and string
    """

    test_code_arity_error = """
    (define (add x y) (+ x y))
    (add 5 10 15) ; Arity error: too many arguments
    """

    test_code_non_function_call = """
    (define x 10)
    (x 5) ; Error: trying to call an integer
    """

    print("\n--- Testing OK Code ---")
    try:
        ast_ok = parse(test_code_ok)
        analyze_semantics(ast_ok)
    except Exception as e:
        print(f"Error processing OK code: {e}")

    print("\n--- Testing Type Error Code ---")
    try:
        ast_type_error = parse(test_code_type_error)
        analyze_semantics(ast_type_error)
    except Exception as e:
        print(f"Successfully caught expected error: {e}") # Expecting SemanticError

    print("\n--- Testing Arity Error Code ---")
    try:
        ast_arity_error = parse(test_code_arity_error)
        analyze_semantics(ast_arity_error)
    except Exception as e:
        print(f"Successfully caught expected error: {e}") # Expecting SemanticError

    print("\n--- Testing Non-Function Call Code ---")
    try:
        ast_non_func = parse(test_code_non_function_call)
        analyze_semantics(ast_non_func)
    except Exception as e:
        print(f"Successfully caught expected error: {e}") # Expecting SemanticError
