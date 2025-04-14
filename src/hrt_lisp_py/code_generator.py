import dataclasses
from typing import Dict, List, Optional, Set, Any, Tuple

# Import shared types
try:
    from .types import Type, IntType, FloatType, BoolType, StringType, VoidType, AnyType, ErrorType, FunctionType
except ImportError:
    from src.hrt_lisp_py.types import Type, IntType, FloatType, BoolType, StringType, VoidType, AnyType, ErrorType, FunctionType

# Adjust import path similarly to parser.py
try:
    from .ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode, # Added DefmacroNode
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, UnquoteSplicingNode,
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression,
        LetBinding,
        print_ast # For debugging
    )
    from .semantic_analyzer import Environment, SemanticError # May need env for context?
except ImportError:
    # Fallback for running script directly or different project structure
    from src.hrt_lisp_py.ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode, # Added DefmacroNode
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, UnquoteSplicingNode,
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression,
        LetBinding,
        print_ast
    )
    from src.hrt_lisp_py.semantic_analyzer import Environment, SemanticError

class CodeGenerationError(Exception):
    """Custom exception for errors during code generation."""
    pass

# --- C Code Generation Specific Imports ---
# (We might need type mapping later)
# from .semantic_analyzer import Type # Assuming Type info is available
# Import SemanticAnalyzer as well
from .semantic_analyzer import Environment, SemanticError, SemanticAnalyzer, Type, FunctionType

class CCodeGenerator:
    """
    Generates C code (C99/C11 standard) from an expanded and analyzed HRT-Lisp AST.
    Assumes the input AST has undergone macro expansion and semantic analysis.
    Strictly adheres to stack allocation and no GC.
    """
    def __init__(self, analyzer: SemanticAnalyzer): # Changed parameter
        # Analyzer instance holds both environment and expression types
        if not isinstance(analyzer, SemanticAnalyzer):
             raise TypeError("CCodeGenerator requires a SemanticAnalyzer instance.")
        self.analyzer = analyzer # Store the analyzer instance
        # self.env can be accessed via self.analyzer.current_env if needed, but prefer specific lookups
        # self.env = initial_env # Remove direct env storage
        self._indent_level = 0
        self._c_code_buffer = [] # Stores lines of generated C code
        self._global_definitions = [] # Store global vars, structs, function prototypes/definitions
        self._includes = set(["<stdio.h>", "<stdbool.h>", "<stdint.h>"]) # Basic includes
        self._temp_var_counter = 0
        self._current_function_return_type = None # Track return type for 'return' statements

    def _add_include(self, header: str):
        """Adds a header to the include list (e.g., '"my_module.h"' or '<math.h>')."""
        self._includes.add(header)

    def generate(self, node: ASTNode) -> str:
        """Public entry point to generate C code for an AST node (typically Program)."""
        self._c_code_buffer = []
        self._global_definitions = []
        self._indent_level = 0
        self._temp_var_counter = 0

        # Main generation logic (likely starts within a main function context for Program)
        self.visit(node) # Populates _global_definitions and _c_code_buffer

        # Assemble the final C code
        include_lines = [f"#include {inc}" for inc in sorted(list(self._includes))]

        # Combine globals and main code (assuming _c_code_buffer contains main function body)
        # This structure needs refinement based on how Program/main is handled
        # Forcefully ensure comma in main signature string
        main_signature = "int main(int argc, char *argv[])"
        print(f"DEBUG: Generating main signature: {main_signature}") # DEBUG PRINT
        final_code = "\n".join(include_lines) + "\n\n" + \
                     "\n\n".join(self._global_definitions) + "\n\n" + \
                     f"{main_signature} {{\n" + \
                     "\n".join([f"    {line}" for line in self._c_code_buffer]) + \
                     "\n    return 0;\n}\n"

        return final_code.strip()

    def _indent(self) -> str:
        """Returns the current C indentation string."""
        return "    " * self._indent_level # Use 4 spaces for C

    def _emit(self, line: str):
        """Appends an indented line to the current code buffer."""
        self._c_code_buffer.append(f"{self._indent()}{line}")

    def _emit_global(self, definition: str):
        """Adds a definition to the global scope."""
        self._global_definitions.append(definition)

    def _get_c_type(self, hrt_type: Any) -> str:
         """Maps HRT-Lisp type representation to C type string. Needs semantic info."""
         # TODO: Implement actual type mapping based on semantic analysis results
         # This is a placeholder and needs the actual type system integration
         if isinstance(hrt_type, str): # Basic placeholder logic
             if hrt_type == 'i32': return 'int32_t'
             if hrt_type == 'i64': return 'int64_t'
             if hrt_type == 'f32': return 'float'
             if hrt_type == 'f64': return 'double'
             if hrt_type == 'bool': return 'bool'
             if hrt_type == 'str': return 'const char*' # Assuming static strings for now
             # Add mappings for u8, u16, u32, u64, struct types, enums etc.
             # For structs: return struct name
             # For enums: return enum name
             return f"/* Unknown HRT Type: {hrt_type} */ void*" # Default/Error case
         # If hrt_type is a complex Type object from semantic analysis:
         # elif isinstance(hrt_type, Type): ... extract info ...
         """Maps HRT-Lisp Type object to a C type string."""
         # Import necessary types from semantic_analyzer if not already done implicitly
         # (May need explicit import at top if running standalone)
         from .semantic_analyzer import Type, IntType, FloatType, BoolType, StringType, VoidType, FunctionType, AnyType, ErrorType

         if hrt_type == IntType: return "int32_t" # Default to int32_t for now
         elif hrt_type == FloatType: return "double" # Default to double for now
         elif hrt_type == BoolType: return "bool" # Requires <stdbool.h>
         elif hrt_type == StringType: return "const char*" # Assuming immutable string literals
         elif hrt_type == VoidType: return "void"
         elif isinstance(hrt_type, FunctionType):
             # Generate C function pointer type string
             # e.g., int (*)(int, double)
             # TODO: Handle function pointer generation more robustly
             param_c_types = [self._get_c_type(pt) for pt in hrt_type.param_types]
             return_c_type = self._get_c_type(hrt_type.return_type)
             params_str = ", ".join(param_c_types) if param_c_types else "void"
             # Basic function pointer syntax
             return f"{return_c_type} (*)({params_str})"
         elif hrt_type == AnyType:
             # How to represent 'any' in C? void* is common but unsafe.
             # Maybe use a tagged union or a specific runtime type info struct?
             # HACK: Temporarily map AnyType to int32_t to allow basic compilation.
             # This is fundamentally incorrect and needs proper type handling later.
             print("Warning: HACK! Mapping AnyType to int32_t. Requires type annotations or inference.")
             return "int32_t"
             # print("Warning: Mapping AnyType to void*. This requires a runtime type system or is unsafe.")
             # return "void*"
         elif hrt_type == ErrorType:
              print("Warning: Encountered ErrorType during C type mapping.")
              return "/* ERROR_TYPE */ void*" # Indicate error, maybe stop generation?
         elif isinstance(hrt_type, Type): # Catch other potential Type subclasses
             # Handle StructType, EnumType etc. when added
             # For now, use the type's name as a placeholder C type name
             # Requires corresponding C struct/enum definition exists.
             c_name = hrt_type.name.replace('-', '_') # Basic mangling
             return c_name # e.g., StructType("my-struct") -> "my_struct"
         else:
             # Handle cases where input is not a Type object (e.g., old string placeholders)
             print(f"Warning: _get_c_type received unexpected input: {hrt_type} ({type(hrt_type)})")
             return "/* UNKNOWN_TYPE */ void*"


    def visit(self, node: ASTNode, is_statement: bool = False) -> Optional[str]:
        """
        Generic visit method. Returns generated C expression string or None if handled via _emit.
        'is_statement' hints if the node is used as a statement vs. an expression.
        """
        method_name = f'visit_{type(node).__name__}'
        visitor = getattr(self, method_name, self.generic_visit)
        # print(f"Visiting {type(node).__name__} (is_statement={is_statement})") # Debug
        result = visitor(node, is_statement=is_statement) if is_statement else visitor(node)
        # print(f" -> Result: {repr(result)[:50]}...") # Debug
        return result

    def generic_visit(self, node: ASTNode, is_statement: bool = False) -> Optional[str]:
        """Default visitor for unhandled node types."""
        print_ast(node) # Print the unhandled node for debugging
        raise CodeGenerationError(f"Code generation not implemented for AST node type: {type(node).__name__}")

    # --- Visitor Methods for Specific Node Types ---

    def visit_Program(self, node: Program, is_statement: bool = False) -> Optional[str]:
        """Generate code for the entire program. Assumes top-level is main context."""
        # Global definitions (functions, structs) should be handled by visit_DefineNode etc.
        # and added to self._global_definitions.
        # Expressions in the program body are treated as statements within main().
        for expr in node.body:
            self.visit(expr, is_statement=True) # Visit top-level forms as statements
        return None # Main code is emitted directly to buffer

    # --- Literals ---
    def visit_IntegerLiteral(self, node: IntegerLiteral, is_statement: bool = False) -> str:
        # TODO: Consider suffixes like L, LL, U based on semantic type if available
        return str(node.value)

    def visit_FloatLiteral(self, node: FloatLiteral, is_statement: bool = False) -> str:
        # TODO: Consider suffixes like f for float vs double based on semantic type
        return str(node.value)

    def visit_StringLiteral(self, node: StringLiteral, is_statement: bool = False) -> str:
        # Generate C string literal. Need to handle C-style escaping.
        # repr() is Python-specific. Manual escaping or a library is needed.
        # For now, a simple placeholder:
        c_escaped = node.value.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
        # TODO: This assumes static allocation. Where does the string live?
        # For now, treat as const char* literal.
        return f'"{c_escaped}"'

    def visit_BoolLiteral(self, node: BoolLiteral, is_statement: bool = False) -> str:
        # Assumes <stdbool.h> is included
        return "true" if node.value else "false"

    # --- Symbols (Variables) ---
    def visit_Symbol(self, node: Symbol, is_statement: bool = False) -> str:
        # Basic symbol to C variable name mapping.
        # TODO: Handle name mangling (e.g., clashes with C keywords).
        # TODO: Use environment to resolve scope and potentially add prefixes/suffixes.
        c_name = node.name.replace('-', '_') # Basic Lisp->C convention
        # Add checks for C keywords (if, else, while, int, float, etc.)
        c_keywords = {"auto", "break", "case", "char", "const", "continue", "default", "do",
                      "double", "else", "enum", "extern", "float", "for", "goto", "if",
                      "inline", "int", "long", "register", "restrict", "return", "short",
                      "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
                      "unsigned", "void", "volatile", "while", "_Bool", "_Complex", "_Imaginary"}
        if c_name in c_keywords:
            c_name = f"_hrt_{c_name}" # Simple mangling prefix
        return c_name

    # --- Definitions ---
    def visit_DefineNode(self, node: DefineNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for global/local variable or function definition using type info."""
        var_name = self.visit(node.symbol)

        # Get the type determined by semantic analysis using the analyzer's environment
        # Assuming lookup should happen in the current scope where define is encountered
        # Need to decide if we pass env explicitly or rely on analyzer's state.
        # Let's assume lookup happens in the analyzer's *global* env for top-level defines.
        # This needs refinement based on how scopes are handled during generation.
        # For now, let's use the analyzer's *current* env state.
        var_hrt_type = self.analyzer.current_env.lookup_variable_type(node.symbol.name)
        if var_hrt_type is None:
             # This should ideally be caught by semantic analysis, but double-check
             raise CodeGenerationError(f"Type information not found for defined symbol '{node.symbol.name}'. Semantic analysis might be incomplete.")

        if isinstance(var_hrt_type, FunctionType):
            # Function definition
            if not isinstance(node.value, LambdaNode):
                 raise CodeGenerationError(f"Type mismatch: Symbol '{var_name}' has function type {var_hrt_type}, but definition value is not LambdaNode.")
            # Pass the specific FunctionType to visit_LambdaNode
            func_code = self.visit_LambdaNode(node.value, name=var_name, func_type=var_hrt_type)
            self._emit_global(func_code) # Add function definition to global scope
            return None # Definition handled globally
        else:
            # Variable definition
            c_type = self._get_c_type(var_hrt_type)
            value_code = self.visit(node.value)
            if value_code is None:
                 raise CodeGenerationError(f"Value expression for define '{var_name}' generated no code.")

            # Determine scope (global vs. local) using the analyzer's current environment state
            # A define at the top level (analyzer.current_env has no enclosing) is global.
            is_global = self.analyzer.current_env.enclosing is None

            definition = f"{c_type} {var_name} = {value_code};"
            if is_global:
                 self._emit_global(definition) # Add global variable definition
            else:
                 self._emit(definition) # Emit local variable definition
            return None # Emitted directly

    def visit_LambdaNode(self, node: LambdaNode, name: Optional[str] = None, func_type: Optional[Any] = None, is_statement: bool = False) -> str:
        """Generate code for a C function definition using FunctionType info."""
        # Import FunctionType here if needed, or rely on _get_c_type's import
        from .semantic_analyzer import FunctionType

        if name is None:
             raise CodeGenerationError("Anonymous lambda generation to C not yet supported.")
        if not isinstance(func_type, FunctionType):
             raise CodeGenerationError(f"visit_LambdaNode requires a valid FunctionType, got {type(func_type)} for function '{name}'.")

        # --- Determine Parameter Types and Names from FunctionType ---
        params_c = []
        if len(node.params) != len(func_type.param_types):
             raise CodeGenerationError(f"Lambda parameter list length ({len(node.params)}) doesn't match FunctionType parameter types length ({len(func_type.param_types)}) for function '{name}'.")

        for p_sym, p_hrt_type in zip(node.params, func_type.param_types):
            p_name = self.visit(p_sym)
            p_c_type = self._get_c_type(p_hrt_type)
            params_c.append(f"{p_c_type} {p_name}")
        # Build params_str more explicitly for debugging
        if not params_c:
            params_str = "void"
        else:
            params_str = params_c[0]
            for i in range(1, len(params_c)):
                params_str += ", " + params_c[i] # Explicitly add comma and space
        print(f"DEBUG: Generated lambda params_str: '{params_str}'") # DEBUG PRINT

        # --- Determine Return Type from FunctionType ---
        return_c_type = self._get_c_type(func_type.return_type)
        self._current_function_return_type = return_c_type # Store for return statements

        # --- Generate Function Body ---
        original_indent = self._indent_level
        self._indent_level = 1 # Indent inside function body
        body_buffer_backup = self._c_code_buffer
        self._c_code_buffer = [] # Use a temporary buffer for the function body

        # Generate the body. If the body is an expression, we need to return its value.
        body_code = self.visit(node.body) # Visit body, try as expression first

        if body_code:
            # If visit returned an expression string and the function is not void, return it.
            if return_c_type != "void":
                self._emit(f"return {body_code};")
            else:
                # If function is void, just emit the expression as a statement.
                self._emit(f"{body_code};")
        # If visit(node.body) returned None (meaning it emitted statements directly),
        # we assume the necessary return statement was part of the body's structure (e.g., inside an if).
        # TODO: This might need more robust handling for implicit returns.

        processed_body_lines = self._c_code_buffer
        self._c_code_buffer = body_buffer_backup # Restore main buffer
        self._indent_level = original_indent # Restore indent
        self._current_function_return_type = None # Reset return type context

        # Assemble the function definition
        func_signature = f"{return_c_type} {name}({params_str})"
        body_str = "\n".join(processed_body_lines)
        # Add braces and potentially a final return 0 for non-void if needed (complex)
        return f"{func_signature} {{\n{body_str}\n}}"


    def _generate_unique_temp_name(self, prefix="_hrt_temp_"):
        """Generates a unique name for temporary C variables."""
        self._temp_var_counter += 1
        return f"{prefix}{self._temp_var_counter}"

    def visit_LetNode(self, node: LetNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for a let expression using C scopes."""
        # 'let' in Lisp is an expression, but in C we often map it to statements.
        # If used as a statement: emit a new scope with variable definitions.
        # If used as an expression: more complex, might need a temporary variable for the result.

        self._emit("{ // Start let") # Start C scope
        self._indent_level += 1

        # Let creates a new scope, but type lookup should happen in the context
        # where let is defined, or types should be inferred/annotated on nodes.
        # Assuming semantic analysis added type info or env handles lookup correctly.

        for binding in node.bindings:
            var_name = self.visit(binding.symbol)
            # Get type of the value being bound using the analyzer's stored types
            value_hrt_type = self.analyzer.lookup_expression_type(binding.value)
            if value_hrt_type is None:
                 # This implies semantic analysis didn't store a type for this node
                 raise CodeGenerationError(f"Type information not found for let binding value expression: {binding.value}")
            c_type = self._get_c_type(value_hrt_type)

            value_code = self.visit(binding.value)
            if value_code is None:
                 raise CodeGenerationError(f"Value expression for let binding '{var_name}' generated no code.")
            self._emit(f"{c_type} {var_name} = {value_code};")

        # Generate body code
        # The environment used for visiting the body should be the one inside the let scope
        # This requires Environment to handle enter/exit scope correctly during visit
        body_result_expr = self.visit(node.body) # Try as expression first

        if is_statement:
             # If let is used as a statement, body_result_expr (if any) is ignored unless emitted by visit
             if body_result_expr:
                 # If visit(body) returned an expression string, emit it as the last statement
                 self._emit(f"{body_result_expr}; // Let body result (statement context)")
             self._indent_level -= 1
             self._emit("} // End let")
             return None # Handled via _emit
        else:
             # Let used as expression: Need to store the result of the body in a temp var
             if body_result_expr is None:
                  raise CodeGenerationError("Let body used as expression generated no value code.")

             # Need type of the body expression for the temporary variable
             body_hrt_type = self.analyzer.lookup_expression_type(node.body)
             if body_hrt_type is None:
                  raise CodeGenerationError(f"Type information not found for let body expression: {node.body}")
             body_c_type = self._get_c_type(body_hrt_type)

             temp_var = self._generate_unique_temp_name("_let_result_")
             # Declare the temp variable outside the let's C scope but assign inside
             # This is tricky. A better approach might be needed depending on C scoping rules.
             # Alternative: Declare temp var before the let block.
             # For now, declare inside, assign inside, return name. Needs testing.
             self._emit(f"{body_c_type} {temp_var};")
             self._emit(f"{temp_var} = {body_result_expr}; // Let body result (expression context)")
             self._indent_level -= 1
             self._emit("} // End let")
             return temp_var # Return the name of the temp var holding the result


    def visit_SetBangNode(self, node: SetBangNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for variable assignment (set!). Should be a statement."""
        if not is_statement:
             # set! is typically a statement with side effects, not an expression returning a value.
             # Although some Lisps return the assigned value, C assignment is often used as a statement.
             # We might need to adapt if HRT-Lisp defines set! to return the value.
             print("Warning: set! used in expression context. Generating statement, value might be lost.")

        var_name = self.visit(node.symbol)
        value_code = self.visit(node.value)
        if value_code is None:
             raise CodeGenerationError(f"Value for set! '{var_name}' generated no code.")

        assignment = f"{var_name} = {value_code};"
        self._emit(assignment)
        return None # Emitted directly

    # --- Control Flow ---
    def visit_IfNode(self, node: IfNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for an if expression/statement."""
        cond_code = self.visit(node.condition)
        if cond_code is None:
             raise CodeGenerationError("If condition generated no code.")

        if is_statement:
             # Generate if (...) { ... } else { ... } statement
             self._emit(f"if ({cond_code}) {{")
             self._indent_level += 1
             self.visit(node.then_branch, is_statement=True)
             self._indent_level -= 1
             self._emit("} else {")
             self._indent_level += 1
             self.visit(node.else_branch, is_statement=True)
             self._indent_level -= 1
             self._emit("}")
             return None # Handled via _emit
        else:
             # Generate C ternary operator: (condition ? then_expr : else_expr)
             then_expr = self.visit(node.then_branch)
             else_expr = self.visit(node.else_branch)
             if then_expr is None or else_expr is None:
                  raise CodeGenerationError("If expression branches generated no code.")
             # TODO: Need type analysis to ensure then/else branches have compatible types for ternary.
             # Removed extra parentheses around cond_code
             return f"({cond_code} ? ({then_expr}) : ({else_expr}))"


    def visit_BeginNode(self, node: BeginNode, is_statement: bool = False) -> Optional[str]:
         """Generate code for a sequence of expressions (like C block)."""
         # If used as statement, just execute sequence.
         # If used as expression, the value is the value of the last expression.

         results_expr = None
         for i, expr in enumerate(node.expressions):
             is_last = (i == len(node.expressions) - 1)
             # Visit intermediate expressions as statements, last one depends on context
             if not is_last:
                 self.visit(expr, is_statement=True)
             else:
                 results_expr = self.visit(expr, is_statement=is_statement) # Pass context to last expr

         if is_statement:
             return None # Statements emitted directly
         else:
             # If begin is used as expression, return the C code for the last expression
             if results_expr is None:
                  # This might happen if the last expr was a definition or statement-only form
                  raise CodeGenerationError("Begin block used as expression ended with non-value statement.")
             return results_expr


    # --- Function Calls ---
    def visit_FunctionCallNode(self, node: FunctionCallNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for a C function call."""
        # Handle built-in functions/operators specially
        if isinstance(node.function, Symbol):
            func_name = node.function.name
            c_op_map = { # Map Lisp names to C operators
                '+': '+', '-': '-', '*': '*', '/': '/', # Basic arithmetic
                '=': '==', 'eq?': '==', # Equality (needs type context for struct/etc)
                '<': '<', '>': '>', '<=': '<=', '>=': '>=' # Comparison
                # TODO: Add bitwise, logical operators etc.
            }
            c_op = c_op_map.get(func_name)

            args_code = [self.visit(arg) for arg in node.arguments]
            valid_args_code = [code for code in args_code if code is not None]
            if len(valid_args_code) != len(node.arguments):
                 raise CodeGenerationError("Argument generation failed for function call.")
            # Build args_str more explicitly for debugging
            if not valid_args_code:
                args_str = ""
            else:
                args_str = valid_args_code[0]
                for i in range(1, len(valid_args_code)):
                    args_str += ", " + valid_args_code[i] # Explicitly add comma and space
            print(f"DEBUG: Generated function call args_str: '{args_str}'") # DEBUG PRINT

            if c_op:
                # Generate C operator expression
                if func_name == '+' and len(valid_args_code) > 1:
                    # C '+' is binary, fold variadic '+'
                    return "(" + " + ".join(valid_args_code) + ")"
                elif func_name == '-' and len(valid_args_code) == 1:
                    return f"(-{valid_args_code[0]})" # Unary minus
                elif len(valid_args_code) == 2:
                    return f"({valid_args_code[0]} {c_op} {valid_args_code[1]})"
                else:
                    # Handle incorrect arity for operators
                    raise CodeGenerationError(f"Operator '{func_name}' called with {len(valid_args_code)} args, expected 1 (unary -) or 2.")
            elif func_name == 'print': # Basic print mapping
                 # TODO: Need robust printing based on type (use printf format specifiers)
                 # Use type information to generate correct printf format specifiers
                 format_specifiers = []
                 printf_args = []
                 # Need to iterate through original arguments to look up types
                 for i, arg_node in enumerate(node.arguments):
                     arg_code = valid_args_code[i] # Get the already generated C code for the arg
                     arg_type = self.analyzer.lookup_expression_type(arg_node)
                     if arg_type is None:
                         # Should not happen if semantic analysis is complete
                         print(f"Warning: Type info missing for print argument: {arg_node}. Defaulting to %p.")
                         format_specifiers.append("%p") # Pointer format as fallback
                     elif arg_type == IntType:
                         format_specifiers.append("%d") # Assuming i32 maps to int
                     elif arg_type == FloatType:
                         format_specifiers.append("%f") # Assuming f64 maps to double
                     elif arg_type == StringType:
                         format_specifiers.append("%s")
                     elif arg_type == BoolType:
                         # Print bools as "true" or "false" strings using ternary
                         arg_code = f"({arg_code} ? \"true\" : \"false\")"
                         format_specifiers.append("%s")
                     # TODO: Add cases for other types (char, structs, enums?)
                     else:
                         print(f"Warning: Unhandled type for printf: {arg_type}. Using %p.")
                         format_specifiers.append("%p") # Default fallback

                     printf_args.append(arg_code)

                 format_string = " ".join(format_specifiers) + "\\n"
                 # Build printf_args_str explicitly
                 if not printf_args:
                     printf_args_str = ""
                 else:
                     printf_args_str = printf_args[0]
                     for i in range(1, len(printf_args)):
                         printf_args_str += ", " + printf_args[i]

                 # Construct printf call string explicitly
                 if printf_args_str:
                     printf_call = f'printf("{format_string}", {printf_args_str})' # Explicit comma
                 else:
                     printf_call = f'printf("{format_string}")'
                 print(f"DEBUG: Generated printf call: '{printf_call}'") # DEBUG PRINT

                 if is_statement:
                     self._emit(printf_call + ";")
                     return None
                 else:
                     # 'print' doesn't typically return a useful value in C context
                      # Maybe return 0 or handle differently?
                      print("Warning: 'print' used in expression context.")
                      return f"({printf_call}, 0)" # C comma operator trick? Risky.
            else:
                # Assume regular C function call
                c_func_name = self.visit(node.function)
                call_expr = f"{c_func_name}({args_str})"
                if is_statement:
                     self._emit(call_expr + ";")
                     return None
                else:
                     return call_expr

        else: # Calling a computed function (function pointers in C)
            func_ptr_code = self.visit(node.function)
            args_code = [self.visit(arg) for arg in node.arguments]
            valid_args_code = [code for code in args_code if code is not None]
            # Build args_str explicitly for function pointers
            if not valid_args_code:
                args_str = ""
            else:
                args_str = valid_args_code[0]
                for i in range(1, len(valid_args_code)):
                    args_str += ", " + valid_args_code[i] # Explicitly add comma and space
            print(f"DEBUG: Generated function pointer args_str: '{args_str}'") # DEBUG PRINT

            # TODO: Need type info for function pointer signature
            call_expr = f"({func_ptr_code})({args_str})" # Basic function pointer call
            if is_statement:
                self._emit(call_expr + ";")
                return None
            else:
                return call_expr


    # --- Other Forms (Placeholders/Skipped) ---

    def visit_DefmacroNode(self, node: DefmacroNode, is_statement: bool = False) -> Optional[str]:
        """Macros are expanded before C code generation."""
        return None # Ignore macro definitions

    def visit_QuoteNode(self, node: QuoteNode, is_statement: bool = False) -> Optional[str]:
        """Generate code for quoted expressions (treat as data)."""
        # This is complex in C. How to represent Lisp data structures?
        # Option 1: Generate static C structs/arrays representing the data.
        # Option 2: Ignore in C generation (if quote is only for compile time).
        # For now, raise error as it needs a clear strategy.
        raise CodeGenerationError("C code generation for 'quote' not implemented yet.")
        # return self._generate_quoted_c_data(node.expression) # Placeholder

    def _generate_quoted_c_data(self, node: ASTNode) -> str:
        """Helper to convert quoted AST back to C static data representation (TODO)."""
        # Placeholder implementation - needs significant design
        if isinstance(node, IntegerLiteral): return str(node.value)
        if isinstance(node, FloatLiteral): return str(node.value)
        if isinstance(node, StringLiteral): return self.visit_StringLiteral(node) # Use C string literal
        if isinstance(node, BoolLiteral): return self.visit_BoolLiteral(node)
        if isinstance(node, Symbol):
            # Represent symbols maybe as static strings or enums?
            return f'"sym_{node.name}"' # Placeholder: static string
        if isinstance(node, SExpression):
            # Represent S-expressions as static arrays of tagged unions or similar?
            # elements = [self._generate_quoted_c_data(elem) for elem in node.elements]
            # return f"(hrt_list_t){{ {', '.join(elements)} }}" # Placeholder struct
            raise CodeGenerationError("Cannot generate quoted C data for SExpression yet.")
        # ... handle other types ...
        raise CodeGenerationError(f"Cannot generate quoted C data representation for {type(node).__name__}")


    def visit_SExpression(self, node: SExpression, is_statement: bool = False) -> Optional[str]:
        """SExpressions usually represent lists. How to generate C for list literals?"""
        # HRT-Lisp has no heap, so lists must be stack-allocated fixed-size arrays.
        # This implies list literals need type and size info at compile time.
        # Option 1: Map to C array initialization `type name[] = {elem1, elem2};`
        # Option 2: If used in expression context, maybe compound literal `(type[]){elem1, ...}`
        # Needs type information!
        # For now, raise error.
        raise CodeGenerationError("C code generation for SExpression (list literal) not implemented yet.")
        # elements_code = [self.visit(elem) for elem in node.elements]
        # valid_elements_code = [code for code in elements_code if code is not None]
        # c_type = "/* TODO: list_elem_type */ int" # Placeholder element type
        # return f"({c_type}[]) {{ {', '.join(valid_elements_code)} }}" # Compound literal attempt


    def visit_QuasiquoteNode(self, node: QuasiquoteNode, is_statement: bool = False) -> Optional[str]:
        """Quasiquote is handled by the macro expander."""
        # Macros handle this, so this node shouldn't appear here. Raise error if it does.
        raise CodeGenerationError("QuasiquoteNode encountered during C code generation. Should have been expanded.")


    def visit_UnquoteNode(self, node: UnquoteNode, is_statement: bool = False) -> Optional[str]:
        """Unquote is handled by the macro expander."""
        raise CodeGenerationError("UnquoteNode encountered during C code generation. Should have been expanded.")


    def visit_UnquoteSplicingNode(self, node: UnquoteSplicingNode, is_statement: bool = False) -> Optional[str]:
        """UnquoteSplicing is handled by the macro expander."""
        raise CodeGenerationError("UnquoteSplicingNode encountered during C code generation. Should have been expanded.")

    # TODO: Add visit methods for other AST nodes (struct, enum, match, loops, etc.)

# --- Helper Function ---
def generate_c_code(ast: Program, analyzer: SemanticAnalyzer) -> str: # Changed parameter
    """Generates C code from the fully expanded and analyzed AST."""
    # Ensure analyzer is provided
    if not isinstance(analyzer, SemanticAnalyzer):
        raise TypeError("generate_c_code requires a valid SemanticAnalyzer object.")

    generator = CCodeGenerator(analyzer) # Pass analyzer to generator
    code = generator.generate(ast)
    print("C code generation phase complete (initial).")
    return code

# --- Example Usage (for testing C generator) ---
if __name__ == '__main__':
    # This example needs a semantic analysis step first to create the environment
    # For now, let's manually create a very simple AST and a dummy environment

    # Dummy Analyzer (replace with actual semantic analysis result)
    # Need to run semantic analysis first to get a populated analyzer
    from .semantic_analyzer import SemanticAnalyzer, analyze_semantics # Import necessary parts
    from .parser import parse # Assuming parser is available

    # Example AST (similar to before)
    # dummy_env.define('+', some_type_info_for_plus_op) # Pseudocode
    # dummy_env.define('print', some_type_info_for_print) # Pseudocode

    # Example AST (similar to before)
    test_ast = Program(body=[
        DefineNode(symbol=Symbol(name='my-var'), value=IntegerLiteral(value=100)),
        DefineNode(symbol=Symbol(name='my-func'), value=LambdaNode(
            params=[Symbol(name='x')],
            body=FunctionCallNode(function=Symbol(name='+'), arguments=[Symbol(name='x'), IntegerLiteral(value=1)])
        )),
        FunctionCallNode(function=Symbol(name='print'), arguments=[
            FunctionCallNode(function=Symbol(name='my-func'), arguments=[Symbol(name='my-var')])
        ])
    ])

    print("\n--- Analyzing Semantics (for C Code Gen Test) ---")
    try:
        # 1. Parse
        test_ast = parse("(define x 10) (print x)") # Simple example
        # 2. Analyze
        dummy_analyzer = SemanticAnalyzer()
        dummy_analyzer.analyze(test_ast) # Populate analyzer's env and expression_types
        print("Semantic analysis for test done.")

        print("\n--- Generating C Code (Initial) ---")
        # 3. Generate C code using the analyzer
        generated_c_code = generate_c_code(test_ast, dummy_analyzer)
        print("\n--- Generated C Code ---")
        print(generated_c_code)
        print("------------------------")

    except Exception as e:
        print(f"\nC Code Generation Failed: {e}")
        import traceback
        traceback.print_exc()

    print("\nNote: Generated C code is preliminary and requires type information from semantic analysis.")
