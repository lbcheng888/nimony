# Core definitions and helpers for the C++ Parser

import std/tables
import std/options
import ./lexer
import ./ast

# --- Precedence Enum ---
type Precedence* = enum
  Lowest,      # Default/lowest precedence
  Assign,      # = += -= *= etc.
  Conditional, # ?: (Ternary operator)
  LogicalOr,   # ||
  LogicalAnd,  # &&
  BitwiseOr,   # |
  BitwiseXor,  # ^
  BitwiseAnd,  # &
  Equality,    # == !=
  Relational,  # < > <= >=
  Shift,       # << >>
  Additive,    # + -
  Multiplicative, # * / %
  Prefix,      # -x !x ++x --x *x &x
  Postfix,     # x++ x--
  Call,        # foo()
  Index,       # array[index]
  Member       # object.member, pointer->member

# --- Parser Type Definition ---
type
  Parser* = ref object
    l*: Lexer
