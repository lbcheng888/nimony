# src/cheng_py/parser/parser.py

import re
from typing import List, Union, Optional, Tuple
from ..core.ast import (
    ChengAstNode, Symbol, Integer, Float, Boolean, String, Pair, Nil, ChengExpr,
    Param, LambdaExpr, DefineMacroExpr, AssignmentExpr, ApplyExpr, QuoteExpr,
    PrefixExpr, BinaryExpr, IndexExpr, SliceExpr, ListLiteralExpr, SequenceExpr,
    IfExpr, TernaryExpr, QuasiquoteExpr, UnquoteExpr, UnquoteSplicingExpr # Added TernaryExpr, If and Quasiquote related
)

# --- Tokenization (Major Update for Infix Syntax) ---

token_specification = [
    # Punctuation
    ('LPAREN',     r'\('),
    ('RPAREN',     r'\)'),
    ('LBRACKET',   r'\['),
    ('RBRACKET',   r'\]'),
    # ('SEMICOLON',  r';'), # Semicolon is now ONLY part of COMMENT rule
    ('DOT',        r'\.'),               # Added for dotted pairs
    ('QUESTION',   r'\?'),               # Added for ternary
    ('COLON',      r':'),               # Added for ternary/slice
    ('BACKTICK',   r'`'),
    ('COMMA_AT',   r',@'),
    ('COMMA',      r','),                # Unquote ,

    # Literals MUST come AFTER punctuation/operators that might start similarly (e.g., '.')
    ('BOOLEAN',    r'true|false'),
    ('FLOAT',      r'[+-]?(\d+\.\d*|\.\d+)([eE][+-]?\d+)?'), # Improved float pattern
    ('INTEGER',    r'[+-]?\d+'),
    ('STRING',     r'"(?:\\.|[^"\\])*"'),

    # Operators (Order matters for regex matching, e.g., >= before >)
    ('EQEQ',       r'=='),               # Comparison ==
    ('LTEQ',       r'<='),               # Comparison <=
    ('GTEQ',       r'>='),               # Comparison >=
    ('EQUALS',     r'='),                # Assignment =
    ('LT',         r'<'),                # Comparison <
    ('GT',         r'>'),                # Comparison >
    ('PLUS',       r'\+'),               # Arithmetic +
    ('MINUS',      r'-'),                # Arithmetic -
    ('STAR',       r'\*'),               # Arithmetic *, Dereference * (handle in parser)
    ('SLASH',      r'/'),                # Arithmetic /
    # Borrowing operators
    ('AMPERSAND',  r'&'),                # Reference & (prefix) - Represents the single (mutable) borrow type
    ('QUOTE_TICK', r"'"),

    # Symbol (catches keywords too, handled in parser)
    # Must come AFTER specific operators/punctuation
    # Stricter regex to avoid consuming operators like =, &
    ('SYMBOL',     r'[a-zA-Z_?!][a-zA-Z0-9_?!]*'),

    # Whitespace
    ('WHITESPACE', r'\s+'),

    # Error
    ('MISMATCH',   r'.'),
]

# Build the master regex
# We need to ensure keywords are matched before symbols. One way is to list them first.
# Another is to check matched SYMBOL against a keyword set later. Let's list first.
token_regex = '|'.join(f'(?P<{name}>{pattern})' for name, pattern in token_specification)

class Token:
    def __init__(self, type: str, value: str, line: int, column: int):
        self.type = type
        self.value = value
        self.line = line
        self.column = column

    def __repr__(self):
        return f"Token({self.type!r}, {self.value!r}, line={self.line}, col={self.column})"

def tokenize(code: str) -> List[Token]:
    tokens = []
    line_num = 1
    line_start = 0
    for mo in re.finditer(token_regex, code):
        kind = mo.lastgroup
        value = mo.group()
        column = mo.start() - line_start + 1
        if kind == 'WHITESPACE':
            pass # Ignore whitespace
        elif kind == 'MISMATCH':
            raise SyntaxError(f"Unexpected character '{value}' at line {line_num}, column {column}")
        else:
            tokens.append(Token(kind, value, line_num, column))

        # Update line number and start position
        newlines = value.count('\n')
        if newlines > 0:
            line_num += newlines
            line_start = mo.start() + value.rfind('\n') + 1 # Start of the last new line

    return tokens

# --- Parsing (Will be replaced by Pratt Parser structure) ---

# Keep parse_atom for converting literal tokens, but it won't handle SYMBOL anymore
# as symbols are handled differently in Pratt (nud for variables).
def parse_atom(token: Token) -> Union[Integer, Float, Boolean, String]:
    """Converts a single literal token into an AST atom."""
    if token.type == 'INTEGER':
        return Integer(int(token.value))
    elif token.type == 'FLOAT':
        return Float(float(token.value))
    elif token.type == 'BOOLEAN':
        return Boolean(True) if token.value == 'true' else Boolean(False)
    elif token.type == 'STRING':
        return String(eval(token.value)) # Using eval for simplicity, consider a safer unescaper
    # Removed SYMBOL case - handled by nud/led in Pratt
    else:
        # This function should only be called for literal tokens now
        raise SyntaxError(f"Internal Error: Cannot parse token as literal atom: {token!r}")

# --- Pratt Parser Implementation ---

# Operator Precedence Levels (Binding Powers) - Updated based on grammar
# Higher numbers mean higher precedence
PRECEDENCE = {
    'DEFAULT': 0,
    # ASSIGN precedence raised to handle sequences like 'a=b *c' correctly.
    # Needs to be higher than PRODUCT to stop parsing after 'b'.
    'ASSIGN': 6, # Increased from 1 to 6
    'TERNARY': 2,     # Added for ?: (Right-associative)
    # SEQUENCE REMOVED
    'COMPARISON': 3,  # ==, <, >, <=, >= (Non-associative)
    'SUM': 4,         # +, - (Left-associative)
    'PRODUCT': 5,     # *, / (Left-associative)
    'PREFIX': 7,      # &, * (Right-associative) - INCREASED
    'INDEX': 8,       # [...] (Left-associative) - INCREASED
    # Note: Call precedence is handled implicitly by S-expression structure
}

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0
        self.current_token: Optional[Token] = self.tokens[self.pos] if self.pos < len(self.tokens) else None
        # nud: null denotation (prefix operators, literals, grouping)
        # led: left denotation (infix operators, postfix operators like call/index)
        self.nud_lookup = self._init_nud()
        self.led_lookup = self._init_led()
        self.bp_lookup = self._init_bp() # Binding power lookup

    def _init_nud(self):
        # Maps token types to their null denotation parsing functions
        return {
            'INTEGER': self.parse_literal,
            'FLOAT': self.parse_literal,
            'BOOLEAN': self.parse_literal,
            'STRING': self.parse_literal,
            'SYMBOL': self.parse_symbol,
            # Prefix operators
            'AMPERSAND': self.parse_prefix_operator, # Ref & (Represents the single mutable borrow type)
            'STAR': self.parse_prefix_operator,      # Deref *
            # Note: Unary minus is handled by parse_infix_operator checking context if needed,
            # or requires a dedicated nud if syntax allows `- x` without space. Assuming infix only for now.
            'LPAREN': self.parse_paren_constructs, # Handles (), grouping, lists, func defs, calls, special forms
            'BACKTICK': self.parse_quasiquote,     # `expr
            'COMMA': self.parse_unquote,           # ,expr
            'COMMA_AT': self.parse_unquote_splicing, # ,@expr
            'QUOTE_TICK': self.parse_quote_tick,   # 'expr
            'LBRACKET': self.parse_list_literal,   # List literal [...]
        }

    def _init_led(self):
        # Maps token types to their left denotation parsing functions (infix/postfix)
        return {
            # Infix operators
            'PLUS': self.parse_infix_operator,
            'MINUS': self.parse_infix_operator,
            'STAR': self.parse_infix_operator, # Binary multiply
            'SLASH': self.parse_infix_operator,
            'EQEQ': self.parse_infix_operator,
            'LT': self.parse_infix_operator,
            'GT': self.parse_infix_operator,
            'LTEQ': self.parse_infix_operator,
            'GTEQ': self.parse_infix_operator,
            'EQUALS': self.parse_assignment, # Assignment =
            'QUESTION': self.parse_ternary,   # Ternary ?
            # Postfix operators
            'LBRACKET': self.parse_index_or_slice, # Index/slice [...]
            # Sequence operator REMOVED
            # 'SEMICOLON': self.parse_sequence_led,
            # LPAREN removed - application handled by S-expr structure in parse_paren_constructs
        }

    def _init_bp(self):
        # Maps token types to their binding power (precedence) for infix/postfix ops
        return {
            'EQUALS': PRECEDENCE['ASSIGN'],     # bp = 1
            'QUESTION': PRECEDENCE['TERNARY'],  # bp = 2
            # 'SEMICOLON': PRECEDENCE['SEQUENCE'], # REMOVED
            'EQEQ': PRECEDENCE['COMPARISON'],
            'LT': PRECEDENCE['COMPARISON'],
            'GT': PRECEDENCE['COMPARISON'],
            'LTEQ': PRECEDENCE['COMPARISON'],
            'GTEQ': PRECEDENCE['COMPARISON'],
            'PLUS': PRECEDENCE['SUM'],
            'MINUS': PRECEDENCE['SUM'],
            'STAR': PRECEDENCE['PRODUCT'],
            'SLASH': PRECEDENCE['PRODUCT'],
            'LBRACKET': PRECEDENCE['INDEX'],
            # LPAREN removed - no infix/postfix binding power
            # RPAREN, EOF etc implicitly have 0
        }

    def _get_bp(self, token_type: Optional[str]) -> int:
        """Get binding power for a token type, default 0."""
        return self.bp_lookup.get(token_type, PRECEDENCE['DEFAULT']) if token_type else PRECEDENCE['DEFAULT']

    def _advance(self):
        """Move to the next token."""
        self.pos += 1
        self.current_token = self.tokens[self.pos] if self.pos < len(self.tokens) else None

    def _peek(self) -> Optional[Token]:
        """Look at the next token without consuming."""
        next_pos = self.pos + 1
        return self.tokens[next_pos] if next_pos < len(self.tokens) else None

    def _consume(self, expected_type: str) -> Token:
        """Consume the current token if it matches expected_type, else raise error."""
        token = self.current_token
        if token is None or token.type != expected_type:
            expected = expected_type
            found = f"'{token.value}' ({token.type})" if token else 'EOF'
            line = token.line if token else '?'
            raise SyntaxError(f"Expected token {expected} but found {found} at line {line}")
        consumed_token = token
        self._advance()
        return consumed_token

    # --- NUD Methods ---
    def parse_literal(self) -> ChengExpr:
        """Parses Integer, Float, Boolean, String literals."""
        token = self.current_token
        if token is None: raise SyntaxError("Expected literal but found EOF")
        try:
            ast_node = parse_atom(token)
            self._advance()
            return ast_node
        except SyntaxError as e:
            raise e
        except Exception as e:
            raise SyntaxError(f"Internal error parsing literal {token!r}: {e}") from e

    def parse_symbol(self) -> Symbol:
        """Parses a SYMBOL token into a Symbol AST node."""
        token = self.current_token
        if token is None or token.type != 'SYMBOL':
             found = f"'{token.value}' ({token.type})" if token else 'EOF'
             line = token.line if token else '?'
             raise SyntaxError(f"Expected SYMBOL but found {found} at line {line}")
        symbol_node = Symbol(token.value)
        self._advance()
        return symbol_node

    def parse_prefix_operator(self) -> PrefixExpr:
        """Parses prefix operators like -, &, *."""
        operator_token = self.current_token
        if operator_token is None: raise SyntaxError("Expected prefix operator but found EOF")
        self._advance()
        operand = self.parse_expression(PRECEDENCE['PREFIX'])
        return PrefixExpr(operator=operator_token.value, right=operand)

    # Removed parse_if_expression - handled in parse_paren_constructs

    def parse_quasiquote(self) -> QuasiquoteExpr:
        """Parses quasiquote: `datum """
        self._advance() # Consume `
        # Parse the following structure as data, not an expression
        datum = self.parse_datum()
        return QuasiquoteExpr(expression=datum) # Store the parsed datum

    def parse_unquote(self) -> UnquoteExpr:
        """Parses unquote: ,expr """
        self._advance() # Consume ,
        expression = self.parse_expression(PRECEDENCE['PREFIX'])
        return UnquoteExpr(expression=expression)

    def parse_unquote_splicing(self) -> UnquoteSplicingExpr:
        """Parses unquote-splicing: ,@expr """
        self._advance() # Consume ,@
        expression = self.parse_expression(PRECEDENCE['PREFIX'])
        return UnquoteSplicingExpr(expression=expression)

    def parse_quote_tick(self) -> QuoteExpr:
        """Parses quote shorthand: 'datum """
        # print(f"DEBUG [QuoteTick]: Start. Current token: {self.current_token!r}") # DEBUG REMOVED
        self._advance() # Consume '
        # Parse the following structure as data, not an expression
        datum = self.parse_datum()
        # print(f"DEBUG [QuoteTick]: End. Parsed datum: {type(datum).__name__}. Next token: {self.current_token!r}") # DEBUG REMOVED
        return QuoteExpr(datum=datum)

    def parse_paren_constructs(self) -> ChengExpr:
        """Parses constructs starting with '(': (), (expr), ((p) b), (if c t e), (op a b), etc."""
        self._consume('LPAREN') # Consume outer '('

        if self.current_token is None: raise SyntaxError("EOF after '('")
        if self.current_token.type == 'RPAREN': # Handle ()
            self._advance()
            return Nil

        # --- Peek Ahead for Anonymous Lambda ---
        # Check if the sequence is: LPAREN (SYMBOL* | RPAREN) RPAREN (not RPAREN)
        is_potential_lambda = False
        params_peek: List[Param] = []
        saved_pos = self.pos # Save current position

        if self.current_token is not None and self.current_token.type == 'LPAREN':
            peek_pos = self.pos + 1 # Position after inner LPAREN
            valid_param_list = True
            # Peek inside the inner parentheses for symbols
            while peek_pos < len(self.tokens) and self.tokens[peek_pos].type != 'RPAREN':
                if self.tokens[peek_pos].type == 'SYMBOL':
                    # Store potential param info if needed, or just validate type
                    params_peek.append(Param(Symbol(self.tokens[peek_pos].value))) # Store for later use if lambda
                    peek_pos += 1
                else: # Found non-symbol in param list
                    valid_param_list = False
                    break
            # Check if inner list closed properly
            if valid_param_list and peek_pos < len(self.tokens) and self.tokens[peek_pos].type == 'RPAREN':
                # Check if there's a body expression *before* the outer RPAREN
                # The outer RPAREN would be at self.pos corresponding to the initial outer LPAREN
                # We need to find the matching outer RPAREN to know the boundary.
                # This peek logic is getting complex. Let's simplify the check:
                # If we have LPAREN LPAREN SYMBOL* RPAREN, and the *next* token
                # after the inner RPAREN is NOT the outer RPAREN, assume lambda.

                peek_pos += 1 # Position after inner RPAREN
                # Check if we are immediately at the end of the outer list
                # This requires finding the matching outer RPAREN, which is hard with peek.
                # Alternative simpler peek: Check if token after inner RPAREN exists and is not RPAREN
                if peek_pos < len(self.tokens) and self.tokens[peek_pos].type != 'RPAREN':
                     is_potential_lambda = True
                # Handle edge case: ((params)) - this is NOT a lambda call
                # This is covered because the token after inner RPAREN would be outer RPAREN.

            # If inner structure wasn't LPAREN SYMBOL* RPAREN, it's not lambda
            # (valid_param_list handles this)

        # Restore position after peeking - PEEKING ONLY, NO CONSUMPTION YET
        self.pos = saved_pos
        self.current_token = self.tokens[self.pos] if self.pos < len(self.tokens) else None

        # --- If Lambda Detected by Peek, Parse It ---
        if is_potential_lambda:
            self._consume('LPAREN') # Consume inner '('
            parsed_params: List[Param] = [] # Re-parse params properly now we consume
            while self.current_token is not None and self.current_token.type != 'RPAREN':
                 if self.current_token.type == 'SYMBOL':
                     parsed_params.append(Param(Symbol(self.current_token.value)))
                     self._advance()
                 else: # Should not happen if peek was correct
                     raise SyntaxError(f"Internal parser error: Mismatch after lambda peek. Expected SYMBOL, got {self.current_token!r}")
            self._consume('RPAREN') # Consume inner ')'

            # Parse body
            body_nodes = []
            while self.current_token is not None and self.current_token.type != 'RPAREN':
                body_nodes.append(self.parse_expression(PRECEDENCE['DEFAULT']))
            self._consume('RPAREN') # Consume outer ')'

            if not body_nodes: body = Nil
            elif len(body_nodes) == 1: body = body_nodes[0]
            else: body = SequenceExpr(expressions=body_nodes)
            # Use parsed_params here, not params_peek
            return LambdaExpr(params=parsed_params, body=body)

        # --- If Not Lambda, Parse Normally (Unified Logic) ---
        else:
            # Parse all elements within the current parentheses
            elements: List[ChengExpr] = []
            while self.current_token is not None and self.current_token.type != 'RPAREN':
                expr = self.parse_expression(PRECEDENCE['DEFAULT'])
                elements.append(expr)
            self._consume('RPAREN') # Consume final ')'

            if not elements: raise SyntaxError("Internal Error: Empty elements list after parsing non-empty parens") # Should be unreachable

            first = elements[0]
            rest = elements[1:]

            # Case 1: Grouping (expr)
            # This case is now hit correctly for (x) because lambda check failed
            if not rest: return first

            # Case 2: Handle based on whether the first element is a Symbol
            if isinstance(first, Symbol):
                # First element IS a Symbol.
                symbol_value = first.value # Use a temporary variable

                # Check for keywords explicitly
                if symbol_value == 'if':
                    if len(rest) < 2 or len(rest) > 3: raise SyntaxError("'if' needs 2 or 3 args")
                    condition = rest[0]
                    then_branch = rest[1]
                    else_branch = rest[2] if len(rest) == 3 else Nil
                    return IfExpr(condition=condition, then_branch=then_branch, else_branch=else_branch)
                elif symbol_value == 'lambda':
                     # Keyword lambda: (lambda params body...)
                     if not rest: raise SyntaxError("'lambda' needs params and body")
                     param_list_node = rest[0]
                     body_nodes = rest[1:]
                     params: List[Param] = []
                     # Allow S-expression style params (x y z) or () represented by Pair/Nil
                     if isinstance(param_list_node, Pair):
                          curr = param_list_node
                          while isinstance(curr, Pair):
                               if isinstance(curr.car, Symbol): params.append(Param(curr.car))
                               else: raise SyntaxError(f"Lambda parameters must be symbols, found {curr.car!r}")
                               curr = curr.cdr
                          if curr is not Nil: raise SyntaxError("Lambda parameter list must be a proper list")
                     elif param_list_node is Nil: pass # lambda () ...
                     else: raise SyntaxError(f"Lambda parameters must be a list of symbols or (), found {param_list_node!r}")

                     if not body_nodes: body = Nil
                     elif len(body_nodes) == 1: body = body_nodes[0]
                     else: body = SequenceExpr(expressions=body_nodes)
                     return LambdaExpr(params=params, body=body)
                elif symbol_value == 'quote':
                     if len(rest) != 1: raise SyntaxError("'quote' requires exactly one argument (datum)")
                     return QuoteExpr(datum=rest[0])
                elif symbol_value == 'begin':
                     if not rest: return Nil
                     elif len(rest) == 1: return rest[0]
                     else: return SequenceExpr(expressions=rest)
                else:
                    # If it's a symbol but not a keyword, it's an application
                    return ApplyExpr(operator=first, operands=rest)

            elif isinstance(first, ChengExpr):
                # Case 3: Application where the first element is NOT a symbol
                # Examples: ((lambda (x) x) 5), (1 2)
                return ApplyExpr(operator=first, operands=rest)
            else:
                 # This case should theoretically not be reached if parsing is correct
                 raise SyntaxError(f"Internal parser error: Unexpected type for first element in list: {type(first)}")

    def parse_list_literal(self) -> ListLiteralExpr:
        """ Parses list literals: [elem1 elem2 ...] """
        # print(f"DEBUG [ListLiteral]: Start. Current token: {self.current_token!r}") # DEBUG REMOVED
        self._consume('LBRACKET')
        elements = []
        while self.current_token is not None and self.current_token.type != 'RBRACKET':
            # print(f"DEBUG [ListLiteral]: Loop start. Current token: {self.current_token!r}") # DEBUG REMOVED
            element_expr = self.parse_expression(PRECEDENCE['DEFAULT'])
            elements.append(element_expr)
            # print(f"DEBUG [ListLiteral]: Loop end. Parsed: {type(element_expr).__name__}. Next token: {self.current_token!r}") # DEBUG REMOVED
        # print(f"DEBUG [ListLiteral]: After loop. Current token: {self.current_token!r}") # DEBUG REMOVED
        self._consume('RBRACKET')
        return ListLiteralExpr(elements=elements)

    # --- New Method: Parse Datum ---
    # --- New Method: Parse Datum ---
    def parse_datum(self) -> ChengExpr:
        """ Parses the current token stream as a data structure (Symbol, Literal, Pair, Nil),
            returning the corresponding AST node representing that data. Used by 'quote'.
            Does NOT parse nested quotes/quasiquotes etc. as special forms here,
            treats ' ` , ,@ as symbols if encountered directly within data.
        """
        # print(f"DEBUG [Datum]: Start. Current token: {self.current_token!r}") # DEBUG REMOVED
        token = self.current_token
        if token is None:
            raise SyntaxError("Unexpected EOF while parsing datum")

        result: ChengExpr

        if token.type == 'LPAREN':
            self._advance() # Consume '('
            elements = []
            while self.current_token is not None and self.current_token.type != 'RPAREN':
                # TODO: Handle dotted pairs '.'
                if self.current_token.type == 'DOT':
                     raise NotImplementedError("Parsing dotted pairs in quoted data not implemented yet.")
                elements.append(self.parse_datum()) # Recursively parse elements as data
            self._consume('RPAREN') # Consume ')'
            # Build runtime Pair structure
            res: ChengExpr = Nil
            for el in reversed(elements):
                res = Pair(el, res)
            result = res
        # Atoms
        elif token.type == 'SYMBOL':
            self._advance()
            result = Symbol(token.value)
        elif token.type == 'INTEGER':
            self._advance()
            result = Integer(int(token.value))
        elif token.type == 'FLOAT':
            self._advance()
            result = Float(float(token.value))
        elif token.type == 'BOOLEAN':
            self._advance()
            result = Boolean(True) if token.value == 'true' else Boolean(False)
        elif token.type == 'STRING':
            self._advance()
            result = String(eval(token.value)) # Use eval for unescaping, consider safer alternative
        # Treat quote/quasiquote/unquote tokens encountered *directly* as symbols within data
        elif token.type in ['QUOTE_TICK', 'BACKTICK', 'COMMA', 'COMMA_AT']:
             self._advance()
             result = Symbol(token.value)
        # Any other token is unexpected in a datum context
        else:
             raise SyntaxError(f"Unexpected token while parsing datum: {token!r}")

        # print(f"DEBUG [Datum]: End. Parsed: {type(result).__name__}. Next token: {self.current_token!r}") # DEBUG REMOVED
        return result


    # --- LED Methods ---
    def parse_infix_operator(self, left: ChengExpr) -> BinaryExpr:
        """Parses binary infix operators like +, -, *, /, ==, <, etc."""
        operator_token = self.current_token
        if operator_token is None: raise SyntaxError("Expected infix operator but found EOF")
        bp = self._get_bp(operator_token.type)
        self._advance()
        right = self.parse_expression(bp)
        return BinaryExpr(left=left, operator=operator_token.value, right=right)

    def parse_assignment(self, left: ChengExpr) -> AssignmentExpr:
        """Parses assignment: target = value. Target can be a Symbol or a dereference (*expr)."""
        # Check if the left-hand side is a valid assignment target
        is_valid_target = isinstance(left, Symbol) or \
                          (isinstance(left, PrefixExpr) and left.operator == '*')
        if not is_valid_target:
            raise SyntaxError(f"Invalid assignment target: {left!r}. Must be a symbol or a dereference (*expr).")

        operator_token = self.current_token # Should be '='
        if operator_token is None: raise SyntaxError("Expected '=' but found EOF")
        bp = self._get_bp(operator_token.type)
        self._advance()
        # Assignment is right-associative, so parse right with bp-1
        right = self.parse_expression(bp - 1) # Reverted back to bp - 1
        return AssignmentExpr(target=left, value=right)

    def parse_ternary(self, condition: ChengExpr) -> TernaryExpr:
        """Parses the ternary operator: condition ? true_expr : false_expr"""
        # Current token is '?'
        bp = self._get_bp('QUESTION') # Binding power of '?'
        self._advance() # Consume '?'

        # Parse the true expression. Ternary is right-associative, so use bp-1.
        true_expr = self.parse_expression(bp - 1)

        # Expect and consume ':'
        self._consume('COLON')

        # Parse the false expression, also with bp-1 for right-associativity
        false_expr = self.parse_expression(bp - 1)

        return TernaryExpr(condition=condition, true_expr=true_expr, false_expr=false_expr)


    # Removed parse_call - handled in parse_paren_constructs

    def parse_index_or_slice(self, sequence: ChengExpr) -> Union[IndexExpr, SliceExpr]:
        """Parses indexing or slicing: sequence[index] or sequence[start:end] """
        start_token = self.current_token
        self._advance() # Consume LBRACKET

        start: Optional[ChengExpr] = None
        end: Optional[ChengExpr] = None
        is_slice = False

        if self.current_token is not None and self.current_token.type != 'COLON' and self.current_token.type != 'RBRACKET':
            start = self.parse_expression()

        if self.current_token is not None and self.current_token.type == 'COLON':
            is_slice = True
            self._advance() # Consume COLON
            if self.current_token is not None and self.current_token.type != 'RBRACKET':
                end = self.parse_expression()
        elif start is None:
             raise SyntaxError(f"Expected index or slice start/end inside [...] at line {start_token.line if start_token else '?'}")

        self._consume('RBRACKET')

        if is_slice:
            return SliceExpr(sequence=sequence, start=start, end=end)
        else:
            if start is None:
                  raise SyntaxError(f"Internal error: Missing index in [...] at line {start_token.line if start_token else '?'}")
            return IndexExpr(sequence=sequence, index=start) # End of parse_index_or_slice

    # Removed parse_juxtaposition_or_call - application handled by S-expr structure in parse_paren_constructs

    # Removed parse_sequence_led - Use (begin ...) instead

    # --- [DEPRECATED] Helper for parsing sequences within bodies ---
    # This method relied on SEMICOLON which has been removed in favor of (begin ...)
    # def parse_sequence_expr(self) -> ChengExpr:
    #     """ Parses a sequence of expressions separated by ';', returning SequenceExpr or single expr.
    #         [DEPRECATED - SEMICOLON REMOVED]
    #         Used for parsing bodies (e.g., lambda) where the sequence isn't necessarily
    #         enclosed in its own parentheses for the sequence itself. Stops at RPAREN or EOF.
    #     """
    #     expressions = []
    #     # Parse the first expression
    #     expressions.append(self.parse_expression())
    #
    #     # Parse subsequent expressions separated by SEMICOLON
    #     while self.current_token is not None and self.current_token.type == 'SEMICOLON':
    #         self._advance() # Consume ';'
    #         # Check if there's an expression after ';' before RPAREN or EOF
    #         if self.current_token is None or self.current_token.type == 'RPAREN':
    #             # Allow trailing semicolon, treat as Nil expression? Or error?
    #             # Let's disallow dangling semicolon for now.
    #             raise SyntaxError("Expression expected after semicolon ';'")
    #         expressions.append(self.parse_expression())
    #
    #     # Return SequenceExpr if multiple expressions, else the single expression
    #     if len(expressions) == 1:
    #         return expressions[0]
    #     elif len(expressions) > 1:
    #         return SequenceExpr(expressions=expressions)
    #     else:
    #         # Should not happen if parsing started correctly
    #         return Nil

    def parse_macro_definition(self) -> DefineMacroExpr: # Ensure this line has exactly 4 spaces indent
        """ Parses top-level macro definitions: name = ` (params...) body
            Assumes the caller has already identified this pattern.
            Current token should be the SYMBOL (macro name).
        """
        # Assumes current token is SYMBOL, peeked ahead to confirm = ` (
        macro_name_token = self._consume('SYMBOL')
        macro_name = Symbol(macro_name_token.value)

        self._consume('EQUALS')
        self._consume('BACKTICK')
        self._consume('LPAREN') # Start parameters

        params: List[Param] = []
        variadic_param: Optional[Param] = None
        while self.current_token is not None and self.current_token.type != 'RPAREN':
            if self.current_token.type == 'SYMBOL':
                params.append(Param(Symbol(self.current_token.value)))
                self._advance()
            elif self.current_token.type == 'DOT':
                if variadic_param is not None:
                    raise SyntaxError("Multiple '.' found in macro parameter list")
                self._advance() # Consume '.'
                if self.current_token is None or self.current_token.type != 'SYMBOL':
                    raise SyntaxError(f"Expected variadic parameter name (symbol) after '.', found {self.current_token!r}")
                variadic_param = Param(Symbol(self.current_token.value))
                self._advance()
                # After the variadic param, we must close the list
                break # Exit the loop, next token must be RPAREN
            else:
                raise SyntaxError(f"Expected parameter name (symbol) or '.' in macro definition, found {self.current_token!r}")

        self._consume('RPAREN') # End parameters

        # Parse the macro body (single expression)
        body = self.parse_expression()

        # Ensure DefineMacroExpr is defined correctly in ast.py
        return DefineMacroExpr(name=macro_name, params=params, body=body, variadic_param=variadic_param)

    # --- Core Pratt Parsing Logic ---
    def parse_expression(self, rbp: int = PRECEDENCE['DEFAULT']) -> ChengExpr:
        """ Parses an expression using Pratt parsing algorithm up to a given right binding power (rbp). """
        # print(f"DEBUG: Enter parse_expression(rbp={rbp}), current={self.current_token!r}") # DEBUG REMOVED
        if self.current_token is None:
            raise SyntaxError("Unexpected end of input while parsing expression")

        # Get the nud function for the current token
        nud_fn = self.nud_lookup.get(self.current_token.type)
        if nud_fn is None:
            raise SyntaxError(f"Unexpected token (no NUD): {self.current_token!r}")

        # Parse the prefix part (literal, variable, prefix op, grouped expr)
        left = nud_fn()

        # Look ahead for infix/postfix operators as long as their precedence is higher than rbp
        while self.current_token is not None and rbp < self._get_bp(self.current_token.type):
            led_fn = self.led_lookup.get(self.current_token.type)
            if led_fn is None:
                # If no led function, it means the current token cannot act as an infix/postfix operator here.
                # This could be the end of the expression or a syntax error if unexpected.
                break # Stop processing infix operators for this expression level

            # If there's an led function, consume the operator and parse the rest
            left = led_fn(left) # led function consumes operator and parses right operand

        # print(f"DEBUG: Exit parse_expression(rbp={rbp}), returning {type(left).__name__}, next={self.current_token!r}") # DEBUG REMOVED
        return left

    # --- Top-Level Parsing Function ---
    def parse_program(self) -> ChengExpr:
        """Parses the entire token stream into a sequence of top-level expressions."""
        if not self.tokens:
            return Nil

        expressions = []
        while self.current_token is not None:
            # Peek ahead to check for macro definition pattern: SYMBOL = ` (
            is_macro_def = False
            if self.current_token.type == 'SYMBOL':
                peek1 = self._peek()
                if peek1 is not None and peek1.type == 'EQUALS':
                     peek2_pos = self.pos + 2
                     peek3_pos = self.pos + 3
                     if peek2_pos < len(self.tokens) and self.tokens[peek2_pos].type == 'BACKTICK' and \
                        peek3_pos < len(self.tokens) and self.tokens[peek3_pos].type == 'LPAREN':
                         is_macro_def = True

            if is_macro_def:
                expressions.append(self.parse_macro_definition())
            else:
                # Parse the next top-level expression
                parsed_expr = self.parse_expression(PRECEDENCE['DEFAULT'])
                expressions.append(parsed_expr)

            # No need to check for semicolons anymore.
            # The loop continues as long as there are tokens.
            # If parse_expression doesn't consume all tokens, it implies a syntax error
            # (e.g., unexpected token after a complete expression), which should ideally
            # be caught within parse_expression or by checking remaining tokens after the loop.
            # For now, we assume parse_expression consumes what it should and the loop
            # continues until EOF.

        # After the loop, self.current_token should be None.

        # Return the result based on the number of expressions parsed
        if len(expressions) == 1:
            return expressions[0]
        elif len(expressions) > 1:
            # Wrap multiple top-level expressions in an implicit (begin ...)
            # which is represented by SequenceExpr
            return SequenceExpr(expressions=expressions)
        else: # Only whitespace/comments in the input
             return Nil

# --- Main parse function (entry point) ---
def parse(code: str) -> ChengExpr:
    """ Top-level function to parse a string into a Cheng AST. """
    tokens = tokenize(code)
    parser = Parser(tokens)
    return parser.parse_program()
