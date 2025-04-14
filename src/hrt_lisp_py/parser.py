import re
import sys
import os
from typing import List, Union, Optional, Tuple

# Adjust import path for direct script execution vs module execution
# Import necessary AST nodes and the updated Expression type alias
try:
    # Attempt relative import first (for module usage)
    from .ast_nodes import (
        ASTNode, Program, SExpression, Symbol, IntegerLiteral,
        FloatLiteral, StringLiteral, BoolLiteral, Expression,
        DefineNode, DefmacroNode, IfNode, FunctionCallNode, LambdaNode, LetNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, # Added Quasiquote/Unquote
        LetBinding, # Import helper
        print_ast
    )
except ImportError:
    # Fallback to absolute import based on expected project structure
    # Assumes the script is run from the project root (lbc-nimony)
    # or src is in PYTHONPATH
    from src.hrt_lisp_py.ast_nodes import (
        ASTNode, Program, SExpression, Symbol, IntegerLiteral,
        FloatLiteral, StringLiteral, BoolLiteral, Expression,
        DefineNode, DefmacroNode, IfNode, FunctionCallNode, LambdaNode, LetNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, # Added Quasiquote/Unquote
        LetBinding, # Import helper
        print_ast
    )

# Import semantic analyzer and macro expander components
try:
    from .semantic_analyzer import SemanticAnalyzer, SemanticError, Environment # Need full class now
    from .macro_expander import expand_macros, MacroExpansionError # Import expander
except ImportError:
    from src.hrt_lisp_py.semantic_analyzer import SemanticAnalyzer, SemanticError, Environment
    from src.hrt_lisp_py.macro_expander import expand_macros, MacroExpansionError


# --- Tokenizer ---

# Simple token representation (can be expanded later)
Token = Tuple[str, str] # (type, value) e.g., ('LPAREN', '('), ('SYMBOL', 'foo')

# Regex patterns for tokenization
# Order matters: match longer patterns first (e.g., floats before ints)
TOKEN_SPECIFICATION = [
    ('COMMENT',    r';[^\n]*'),         # Comments (ignored)
    ('WHITESPACE', r'[ \t\n\r]+'),      # Whitespace (ignored)
    ('LPAREN',     r'\('),              # Left parenthesis
    ('RPAREN',     r'\)'),              # Right parenthesis
    ('QUOTE_TICK', r"'"),               # Quote reader macro '
    ('QUASIQUOTE_TICK', r'`'),           # Quasiquote reader macro `
    ('UNQUOTE_SPLICING_TICK', r',@'),    # Unquote-splicing reader macro ,@ (MUST be before UNQUOTE_TICK)
    ('UNQUOTE_TICK', r','),              # Unquote reader macro ,
    ('STRING',     r'"(?:\\.|[^"\\])*"'),# String literals (handle escapes)
    ('FLOAT',      r'[+-]?\d+\.\d*'),   # Floating point numbers
    ('INTEGER',    r'[+-]?\d+'),        # Integer numbers
    ('BOOL',       r'#t|#f'),           # Boolean literals
    ('DOT',        r'\.'),              # Dot for pairs/rest args (needs careful placement)
    ('SYMBOL',     r'[a-zA-Z_+\-*/<>=!?][a-zA-Z0-9_+\-*/<>=!?]*'), # Symbols
    ('MISMATCH',   r'.'),               # Any other character (error)
]

TOKEN_REGEX = re.compile('|'.join(f'(?P<{pair[0]}>{pair[1]})' for pair in TOKEN_SPECIFICATION))

def tokenize(code: str) -> List[Token]:
    """Converts source code string into a list of tokens."""
    tokens: List[Token] = []
    line_num = 1
    line_start = 0
    for mo in TOKEN_REGEX.finditer(code):
        kind = mo.lastgroup
        value = mo.group()
        column = mo.start() - line_start
        if kind == 'WHITESPACE' or kind == 'COMMENT':
            if '\n' in value:
                line_num += value.count('\n')
                line_start = mo.end() - value.rfind('\n') -1
            continue # Skip whitespace and comments
        elif kind == 'MISMATCH':
            raise RuntimeError(f'{line_num}:{column}: Unexpected character: {value}')
        tokens.append((kind, value))
        if '\n' in value: # Should not happen for non-whitespace/comment, but safety check
             line_num += value.count('\n')
             line_start = mo.end() - value.rfind('\n') -1
    return tokens

# --- Parser ---

class ParseError(Exception):
    pass

class Parser:
    """Parses a list of tokens into an AST."""
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.position = 0

    def _peek(self) -> Optional[Token]:
        """Return the next token without consuming it."""
        if self.position < len(self.tokens):
            return self.tokens[self.position]
        return None

    def _consume(self) -> Optional[Token]:
        """Consume and return the current token."""
        token = self._peek()
        if token:
            self.position += 1
        return token

    def _expect(self, expected_kind: str) -> Token:
        """Consume the next token if it matches expected_kind, else raise error."""
        token = self._consume()
        if not token or token[0] != expected_kind:
            expected = expected_kind
            found = f"'{token[1]}'" if token else "end of input"
            raise ParseError(f"Expected {expected}, but found {found} at position {self.position}")
        return token

    def parse_program(self) -> Program:
        """Parses the entire token stream into a Program AST node."""
        body_list: List[Expression] = [] # Collect expressions in a list first
        while self._peek():
            # Program body now consists of Expressions
            body_list.append(self.parse_expression())
        # Convert the list to a tuple before creating the Program node
        return Program(body=tuple(body_list))

    # Renamed from parse_sexp_element to parse_expression for clarity
    def parse_expression(self) -> Expression:
        """Parses a single expression (atom or list/construct)."""
        token = self._peek()
        if not token:
            raise ParseError("Unexpected end of input while parsing expression")

        kind = token[0]
        if kind == 'LPAREN':
            # Handle lists, which could be special forms or function calls
            return self._parse_list_or_construct()
        elif kind == 'QUOTE_TICK':
            # Handle 'expr -> (quote expr)
            self._consume() # Consume the tick
            quoted_expr = self.parse_expression() # Parse the expression following the tick
            return QuoteNode(expression=quoted_expr)
        elif kind == 'QUASIQUOTE_TICK':
            # Handle `expr -> (quasiquote expr)
            self._consume() # Consume the tick
            quasiquoted_expr = self.parse_expression()
            return QuasiquoteNode(expression=quasiquoted_expr)
        elif kind == 'UNQUOTE_TICK':
             # Handle ,expr -> (unquote expr)
             # Note: Unquote typically only makes sense inside quasiquote,
             # but the parser can parse it anywhere for now. Expansion handles semantics.
            self._consume() # Consume the tick
            unquoted_expr = self.parse_expression()
            return UnquoteNode(expression=unquoted_expr)
        elif kind == 'UNQUOTE_SPLICING_TICK':
             # Handle ,@expr -> (unquote-splicing expr)
             # Similar to unquote, parsing it anywhere, expansion handles semantics.
            self._consume() # Consume the tick
            spliced_expr = self.parse_expression()
            # Need to import UnquoteSplicingNode if not already done implicitly
            # (Assuming it's imported via the wildcard/module import)
            # Need to ensure ast_nodes.py actually defines UnquoteSplicingNode
            from .ast_nodes import UnquoteSplicingNode # Explicit import just in case
            return UnquoteSplicingNode(expression=spliced_expr)
        elif kind in ('INTEGER', 'FLOAT', 'STRING', 'BOOL', 'SYMBOL'):
            return self.parse_atom()
        else:
            raise ParseError(f"Unexpected token type: {kind} ('{token[1]}')")

    def _parse_list_or_construct(self) -> Expression:
        """Parses a list, determining if it's a special form or function call."""
        start_pos = self.position
        self._expect('LPAREN')

        # Handle empty list ()
        if self._peek() and self._peek()[0] == 'RPAREN':
            self._consume() # Consume ')'
            return SExpression(elements=()) # Use empty tuple

        # Peek at the first element to check for keywords
        first_element_token = self._peek()
        if first_element_token and first_element_token[0] == 'SYMBOL':
            keyword = first_element_token[1]
            if keyword == 'define':
                return self._parse_define()
            elif keyword == 'if':
                 return self._parse_if()
            elif keyword == 'lambda':
                 return self._parse_lambda()
            # Add 'let' parsing
            elif keyword == 'let':
                 return self._parse_let()
            elif keyword == 'set!':
                return self._parse_set_bang()
            elif keyword == 'begin':
                return self._parse_begin()
            elif keyword == 'quote':
                return self._parse_quote()
            elif keyword == 'defmacro': # Add defmacro check
                return self._parse_defmacro()
            # Add other keywords here later
            else:
                # Not a known keyword, assume function call
                return self._parse_function_call()
        else:
             # First element is not a symbol (e.g., ((lambda ...) 5))
             # Treat as a function call where the first element is an expression
             return self._parse_function_call()


    def _parse_define(self) -> DefineNode:
        """Parses (define symbol value) or (define (func params...) body...)."""
        self._consume() # Consume 'define'

        next_token = self._peek()
        if not next_token:
            raise ParseError("Unexpected end of input after 'define'")

        if next_token[0] == 'SYMBOL':
            # Standard variable definition: (define symbol value)
            symbol_token = self._expect('SYMBOL') # Use _expect instead of _match
            symbol = Symbol(name=symbol_token[1]) # Access tuple element by index [1]
            value = self.parse_expression()
            self._expect('RPAREN')
            return DefineNode(symbol=symbol, value=value)

        elif next_token[0] == 'LPAREN':
            # Function definition shorthand: (define (func params...) body...)
            self._expect('LPAREN') # Use _expect instead of _match
            # Parse function name
            func_name_token = self._peek()
            if not func_name_token or func_name_token[0] != 'SYMBOL':
                raise ParseError(f"Expected function name symbol inside define, found {func_name_token}")
            func_name_symbol = self.parse_atom()
            if not isinstance(func_name_symbol, Symbol):
                 raise ParseError("Internal error: parsed function name is not Symbol")

            # Parse parameters
            params: List[Symbol] = []
            rest_param: Optional[Symbol] = None # Handle potential rest parameters later if needed
            while self._peek() and self._peek()[0] != 'RPAREN':
                 # TODO: Add support for rest parameters ('. symbol') if desired
                 param_token = self._peek()
                 if param_token[0] == 'SYMBOL':
                     param_symbol = self.parse_atom()
                     if isinstance(param_symbol, Symbol):
                         params.append(param_symbol)
                     else:
                         raise ParseError("Internal error: parsed parameter is not Symbol")
                 else:
                     raise ParseError(f"Expected symbol for parameter name, found {param_token}")

            self._expect('RPAREN') # Use _expect instead of _match
            # Parse function body (can be multiple expressions, implicitly wrapped in begin)
            body_expressions: List[Expression] = []
            while self._peek() and self._peek()[0] != 'RPAREN':
                body_expressions.append(self.parse_expression())

            if not body_expressions:
                raise ParseError("Function definition requires at least one body expression")

            # Wrap multiple body expressions in BeginNode
            body_node: Expression
            if len(body_expressions) == 1:
                body_node = body_expressions[0]
            else:
                # Convert list to tuple for BeginNode
                body_node = BeginNode(expressions=tuple(body_expressions))

            # Create the LambdaNode for the function value, converting params list to tuple
            lambda_node = LambdaNode(params=tuple(params), body=body_node)

            self._expect('RPAREN') # Use _expect instead of _match
            return DefineNode(symbol=func_name_symbol, value=lambda_node)

        else:
            raise ParseError(f"Expected symbol or '(' after 'define', found {next_token}")

    def _parse_if(self) -> IfNode:
        """Parses (if condition then else). Assumes LPAREN and 'if' symbol consumed."""
        # Consume 'if' symbol (already peeked)
        self._consume()

        # Expect condition, then, else expressions
        condition_node = self.parse_expression()
        then_node = self.parse_expression()
        else_node = self.parse_expression() # Assuming mandatory else

        # Expect RPAREN
        self._expect('RPAREN')
        return IfNode(condition=condition_node, then_branch=then_node, else_branch=else_node)

    def _parse_lambda(self) -> LambdaNode:
        """Parses (lambda (param1 ...) body). Assumes LPAREN and 'lambda' consumed."""
        self._consume() # Consume 'lambda'

        # Expect parameter list (an SExpression of Symbols)
        param_list_token = self._peek()
        if not param_list_token or param_list_token[0] != 'LPAREN':
            raise ParseError("Expected parameter list '(' after lambda")
        self._expect('LPAREN')
        params: List[Symbol] = []
        while self._peek() and self._peek()[0] == 'SYMBOL':
            param_atom = self.parse_atom()
            if isinstance(param_atom, Symbol):
                 params.append(param_atom)
            else:
                 # Should not happen based on peek, but defensive check
                 raise ParseError("Internal error: Expected Symbol in lambda params list")
        self._expect('RPAREN') # Close parameter list

        # Expect body expression
        body_node = self.parse_expression()

        # Expect RPAREN for the lambda form itself
        self._expect('RPAREN')
        # Convert params list to tuple for LambdaNode
        return LambdaNode(params=tuple(params), body=body_node)

    def _parse_let(self) -> LetNode:
        """Parses (let ((var1 val1) (var2 val2) ...) body...). Assumes LPAREN and 'let' consumed."""
        self._consume() # Consume 'let'

        # Expect bindings list
        bindings_list_token = self._peek()
        if not bindings_list_token or bindings_list_token[0] != 'LPAREN':
            raise ParseError("Expected bindings list '(' after let")
        self._expect('LPAREN') # Consume bindings list LPAREN

        bindings: List[LetBinding] = []
        while self._peek() and self._peek()[0] == 'LPAREN':
            self._expect('LPAREN') # Consume start of a binding pair

            symbol_token = self._peek()
            if not symbol_token or symbol_token[0] != 'SYMBOL':
                raise ParseError(f"Expected symbol in let binding, found {symbol_token}")
            symbol_node = self.parse_atom()
            if not isinstance(symbol_node, Symbol):
                raise ParseError("Internal error: parsed atom in let binding is not Symbol")

            value_node = self.parse_expression()

            self._expect('RPAREN') # Consume end of a binding pair
            bindings.append(LetBinding(symbol=symbol_node, value=value_node))

        self._expect('RPAREN') # Consume end of bindings list

        # Expect body expressions (can be multiple, implicitly wrapped in begin)
        body_expressions: List[Expression] = []
        while self._peek() and self._peek()[0] != 'RPAREN':
            body_expressions.append(self.parse_expression())

        if not body_expressions:
            raise ParseError("Let form requires at least one body expression")

        # Expect RPAREN for the let form itself
        self._expect('RPAREN') # Consume final RPAREN of let

        # If there's only one body expression, use it directly.
        # If multiple, wrap in an implicit BeginNode.
        body_node: Expression
        if len(body_expressions) == 1:
            body_node = body_expressions[0]
        else:
            # Convert list to tuple for BeginNode
            body_node = BeginNode(expressions=tuple(body_expressions))

        # Convert bindings list to tuple for LetNode
        return LetNode(bindings=tuple(bindings), body=body_node)

    def _parse_set_bang(self) -> SetBangNode:
        """Parses (set! symbol value). Assumes LPAREN and 'set!' consumed."""
        self._consume() # Consume 'set!'

        # Expect symbol
        symbol_token = self._peek()
        if not symbol_token or symbol_token[0] != 'SYMBOL':
            raise ParseError(f"Expected symbol after 'set!', found {symbol_token}")
        symbol_node = self.parse_atom()
        if not isinstance(symbol_node, Symbol):
             raise ParseError("Internal error: parsed atom after set! is not Symbol")

        # Expect value expression
        value_node = self.parse_expression()

        # Expect RPAREN
        self._expect('RPAREN')
        return SetBangNode(symbol=symbol_node, value=value_node)

    def _parse_begin(self) -> BeginNode:
        """Parses (begin expr1 expr2 ...). Assumes LPAREN and 'begin' consumed."""
        self._consume() # Consume 'begin'

        expressions: List[Expression] = []
        while self._peek() and self._peek()[0] != 'RPAREN':
            expressions.append(self.parse_expression())

        self._expect('RPAREN')
        # Handle (begin) case - return BeginNode with empty list? Or maybe require at least one expr?
        # Let's allow empty for now, semantics can decide later.
        # Convert expressions list to tuple for BeginNode
        return BeginNode(expressions=tuple(expressions))

    def _parse_quote(self) -> QuoteNode:
        """Parses (quote expr). Assumes LPAREN and 'quote' consumed."""
        self._consume() # Consume 'quote'

        # Expect exactly one expression to quote
        quoted_expr = self.parse_expression()

        # Ensure there are no more expressions before RPAREN
        if self._peek() and self._peek()[0] != 'RPAREN':
            raise ParseError("Quote expects exactly one expression")

        self._expect('RPAREN')
        return QuoteNode(expression=quoted_expr)

    def _parse_defmacro(self) -> DefmacroNode:
        """Parses (defmacro name (param1 ...) body). Assumes LPAREN and 'defmacro' consumed."""
        self._consume() # Consume 'defmacro'

        # Expect macro name (Symbol)
        name_token = self._peek()
        if not name_token or name_token[0] != 'SYMBOL':
            raise ParseError(f"Expected symbol for macro name after 'defmacro', found {name_token}")
        name_node = self.parse_atom()
        if not isinstance(name_node, Symbol):
             raise ParseError("Internal error: parsed atom for macro name is not Symbol")

        # Expect parameter list (an SExpression of Symbols)
        param_list_token = self._peek()
        if not param_list_token or param_list_token[0] != 'LPAREN':
            raise ParseError("Expected parameter list '(' after macro name")
        self._expect('LPAREN')
        params: List[Symbol] = []
        rest_param: Optional[Symbol] = None
        while self._peek() and self._peek()[0] != 'RPAREN':
            next_token = self._peek()
            if next_token[0] == 'SYMBOL':
                param_atom = self.parse_atom()
                if isinstance(param_atom, Symbol):
                    params.append(param_atom)
                else:
                    # Should not happen based on peek
                    raise ParseError("Internal error: Expected Symbol in defmacro params list")
            elif next_token[0] == 'DOT':
                if rest_param is not None:
                    raise ParseError("Multiple dots found in defmacro parameter list")
                self._consume() # Consume '.'
                rest_token = self._peek()
                if not rest_token or rest_token[0] != 'SYMBOL':
                    raise ParseError("Expected symbol after dot in defmacro parameter list")
                rest_atom = self.parse_atom()
                if isinstance(rest_atom, Symbol):
                    rest_param = rest_atom
                else:
                    # Should not happen
                    raise ParseError("Internal error: Expected Symbol after dot")
                # After the rest param symbol, we must have RPAREN
                if self._peek() and self._peek()[0] != 'RPAREN':
                     raise ParseError("Expected ')' after rest parameter symbol in defmacro")
                break # Exit loop after processing rest parameter
            else:
                raise ParseError(f"Unexpected token in defmacro parameter list: {next_token[1]}")

        self._expect('RPAREN') # Close parameter list

        # Expect body expression (the macro transformer)
        body_node = self.parse_expression()

        # Expect RPAREN for the defmacro form itself
        self._expect('RPAREN')
        # Convert params list to tuple for DefmacroNode
        return DefmacroNode(name=name_node, params=tuple(params), rest_param=rest_param, body=body_node)

    def _parse_function_call(self) -> FunctionCallNode:
        """Parses (func arg1 arg2 ...). Assumes LPAREN consumed."""
        # The first element is the function (could be symbol or complex expression)
        func_expr = self.parse_expression()

        # Parse arguments until RPAREN
        args: List[Expression] = []
        while self._peek() and self._peek()[0] != 'RPAREN':
            args.append(self.parse_expression())

        self._expect('RPAREN')
        # Convert args list to tuple for FunctionCallNode
        return FunctionCallNode(function=func_expr, arguments=tuple(args))


    # parse_atom remains largely the same, but its return type is part of Expression
    def parse_atom(self) -> Union[Symbol, IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral]:
        """Parses an atomic token into its corresponding AST node."""
        # Removed duplicated self._consume() call here
        token = self._consume()
        if not token:
             raise ParseError("Unexpected end of input while parsing atom") # Should not happen if called correctly

        kind, value = token
        if kind == 'INTEGER':
            return IntegerLiteral(value=int(value))
        elif kind == 'FLOAT':
            return FloatLiteral(value=float(value))
        elif kind == 'STRING':
            # Remove surrounding quotes and handle basic escapes (more robust handling needed for production)
            processed_value = value[1:-1].encode('utf-8').decode('unicode_escape')
            return StringLiteral(value=processed_value)
        elif kind == 'BOOL':
            return BoolLiteral(value=(value == '#t'))
        elif kind == 'SYMBOL':
            return Symbol(name=value)
        else:
            # This case should ideally be unreachable if parse_expression works correctly
            raise ParseError(f"Internal error: Unexpected token type in parse_atom: {kind}")

# --- Main Parsing Function ---
def parse(code: str) -> Program:
    """Tokenizes and parses the source code string."""
    tokens = tokenize(code)
    parser = Parser(tokens)
    return parser.parse_program()

# --- Example Usage (for testing) ---
if __name__ == '__main__':
    # Determine the path to the test file relative to this script
    script_dir = os.path.dirname(__file__)
    test_file_path = os.path.join(script_dir, 'test_parser.hrtpy')

    if not os.path.exists(test_file_path):
        print(f"Error: Test file not found at {test_file_path}")
        sys.exit(1)

    print(f"Parsing file: {test_file_path}")
    try:
        with open(test_file_path, 'r', encoding='utf-8') as f:
            test_code = f.read()
        print("--- File Content ---")
        print(test_code)
        print("--------------------")

        ast = parse(test_code)
        print("\n--- AST Structure ---")
        print_ast(ast)
        print("---------------------")

        # --- Phase 1: Semantic Analysis (for Macro Definitions) ---
        # We need to run analysis once to populate the environment with macro definitions.
        print("\n--- Initial Semantic Analysis (Populating Macros) ---")
        initial_analyzer = SemanticAnalyzer()
        initial_analyzer.analyze(ast) # This defines macros in initial_analyzer.current_env
        print("Initial semantic analysis successful.")
        print("-----------------------------------------------------")

        # --- Phase 2: Macro Expansion ---
        print("\n--- Macro Expansion ---")
        # Pass the original AST and the environment containing macro definitions
        expanded_ast = expand_macros(ast, initial_analyzer.current_env)
        print("\n--- Expanded AST ---")
        print_ast(expanded_ast)
        print("----------------------")


        # --- Phase 3: Semantic Analysis on Expanded Code ---
        # Analyze the code *after* macro expansion using a fresh analyzer/environment
        # to ensure the generated code is valid.
        print("\n--- Final Semantic Analysis (Scope Check on Expanded Code) ---")
        final_analyzer = SemanticAnalyzer() # Use a fresh environment
        final_analyzer.analyze(expanded_ast) # Analyze the expanded AST
        print("Final semantic analysis successful.")
        print("--------------------------------------------------------------")

        # --- Phase 4: Code Generation (Python Target) ---
        print("\n--- Python Code Generation ---")
        # Need to import the code generator function
        try:
            from .code_generator import generate_python_code
        except ImportError:
            from src.hrt_lisp_py.code_generator import generate_python_code

        generated_code = generate_python_code(expanded_ast)
        print("\n--- Generated Python Code ---")
        print(generated_code)
        print("-----------------------------")

        # --- Phase 5: Execute Generated Code (Optional) ---
        print("\n--- Executing Generated Code ---")
        try:
            # Execute in a controlled environment if needed
            exec_globals = {}
            exec(generated_code, exec_globals)
            print("Generated code executed successfully.")
        except Exception as exec_e:
            print(f"Error during execution of generated code: {exec_e}")
            import traceback
            traceback.print_exc()
        print("------------------------------")


    except (RuntimeError, ParseError, SemanticError, MacroExpansionError, NotImplementedError) as e: # Added MacroExpansionError, NotImplementedError
        # Adjusted error message prefix
        error_type = type(e).__name__
        print(f"\nError during processing of {test_file_path}: [{error_type}] {e}")
    except FileNotFoundError:
         print(f"Error: Could not read test file at {test_file_path}")
    except Exception as e: # Catch any other unexpected errors
        print(f"\nAn unexpected error occurred: {e}")
        import traceback
        traceback.print_exc() # Print full traceback for unexpected errors
