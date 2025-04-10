# C++ Parser Main Entry Point for nifcpp
# This file imports the submodules and provides the main parse function.

import std/sequtils
import std/options
import std/sequtils
import std/options
import std/strutils # Keep for error message formatting if needed
import ./lexer
import ./ast
import ./parser_impl # Import the combined implementation

# --- Main Entry Point ---
proc parse*(l: Lexer): (Option[TranslationUnit], seq[string]) =
  # newParser is defined in parser_impl.nim
  var p = parser_impl.newParser(l)

  # --- Register Parse Functions ---
  # Registration should happen after parser object 'p' is created.
  # All parsing functions are now in parser_impl.

  # Prefix functions
  p.registerPrefix(tkIdentifier, parser_impl.parseIdentifier)
  p.registerPrefix(tkIntegerLiteral, parser_impl.parseIntegerLiteral)
  p.registerPrefix(tkFloatLiteral, parser_impl.parseFloatLiteral)
  p.registerPrefix(tkStringLiteral, parser_impl.parseStringLiteral)
  p.registerPrefix(tkCharLiteral, parser_impl.parseCharLiteral)
  p.registerPrefix(tkLParen, parser_impl.parseGroupedExpression)
  p.registerPrefix(tkLBrace, parser_impl.parseInitializerListExpression)
  p.registerPrefix(tkPlus, parser_impl.parsePrefixExpression)
  p.registerPrefix(tkMinus, parser_impl.parsePrefixExpression)
  p.registerPrefix(tkStar, parser_impl.parsePrefixExpression) # Dereference
  p.registerPrefix(tkBitwiseAnd, parser_impl.parsePrefixExpression) # Address-of
  p.registerPrefix(tkIncrement, parser_impl.parsePrefixExpression) # Pre-increment
  p.registerPrefix(tkDecrement, parser_impl.parsePrefixExpression) # Pre-decrement
  p.registerPrefix(tkNot, parser_impl.parsePrefixExpression) # Logical NOT
  p.registerPrefix(tkTilde, parser_impl.parsePrefixExpression) # Bitwise NOT
  p.registerPrefix(tkThrow, parser_impl.parseThrowExpression)
  p.registerPrefix(tkLBracket, parser_impl.parseLambdaExpression) # Lambda start

  # Infix functions
  p.registerInfix(tkPlus, parser_impl.parseInfixExpression)
  p.registerInfix(tkMinus, parser_impl.parseInfixExpression)
  p.registerInfix(tkStar, parser_impl.parseInfixExpression)
  p.registerInfix(tkSlash, parser_impl.parseInfixExpression)
  p.registerInfix(tkPercent, parser_impl.parseInfixExpression)
  p.registerInfix(tkEqual, parser_impl.parseInfixExpression)
  p.registerInfix(tkNotEqual, parser_impl.parseInfixExpression)
  p.registerInfix(tkLess, parser_impl.parseInfixExpression)
  p.registerInfix(tkLessEqual, parser_impl.parseInfixExpression)
  p.registerInfix(tkGreater, parser_impl.parseInfixExpression)
  p.registerInfix(tkGreaterEqual, parser_impl.parseInfixExpression)
  p.registerInfix(tkLogicalAnd, parser_impl.parseInfixExpression)
  p.registerInfix(tkLogicalOr, parser_impl.parseInfixExpression)
  p.registerInfix(tkBitwiseAnd, parser_impl.parseInfixExpression)
  p.registerInfix(tkBitwiseOr, parser_impl.parseInfixExpression)
  p.registerInfix(tkBitwiseXor, parser_impl.parseInfixExpression)
  p.registerInfix(tkLeftShift, parser_impl.parseInfixExpression)
  p.registerInfix(tkRightShift, parser_impl.parseInfixExpression)
  p.registerInfix(tkAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkPlusAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkMinusAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkMulAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkDivAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkModAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkAndAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkOrAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkXorAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkLShiftAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkRShiftAssign, parser_impl.parseInfixExpression)
  p.registerInfix(tkLParen, parser_impl.parseCallExpression) # Function call
  p.registerInfix(tkLBracket, parser_impl.parseIndexExpression) # Array subscript
  p.registerInfix(tkDot, parser_impl.parseMemberAccessExpression) # Member access
  p.registerInfix(tkArrow, parser_impl.parseMemberAccessExpression) # Member access (pointer)
  p.registerInfix(tkIncrement, parser_impl.parsePostfixExpression) # Post-increment
  p.registerInfix(tkDecrement, parser_impl.parsePostfixExpression) # Post-decrement

  # --- End Register Parse Functions ---

  # Call the main parse function from parser_impl
  let tu = parser_impl.parse(p)

  if p.errors.len > 0:
    return (none(TranslationUnit), p.errors)
  else:
    return (some(tu), p.errors)
