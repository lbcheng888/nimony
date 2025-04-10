# C++ Expression Parsing Implementation for nifcpp

import std/strutils
import std/options
import std/tables # Add direct import for getOrDefault
import ./lexer
import ./ast
import ./parser_core # Import core types and helpers
# Import modules containing the actual implementations needed
import ./parser_types # For parseType, parseParameterList
import ./parser_statements # For parseBlockStatement

# --- Forward declaration for recursive expression parsing ---
proc parseExpression*(p: var Parser, precedence: Precedence): Expression # Forward

# --- Expression Parsing Implementations ---

proc parseIdentifier*(p: var Parser): Expression = # Exported for registration
  result = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p) # Consume the identifier token

proc parseIntegerLiteral*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseBiggestInt(currentToken.literal)
    result = IntegerLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the integer literal token
  except ValueError:
    p.errors.add("Invalid integer literal: " & currentToken.literal)
    consumeToken(p) # Consume the invalid token

proc parseFloatLiteral*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseFloat(currentToken.literal)
    result = FloatLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the float literal token
  except ValueError:
    p.errors.add("Invalid float literal: " & currentToken.literal)
    consumeToken(p) # Consume the invalid token

proc parseStringLiteral*(p: var Parser): Expression = # Exported for registration
  result = StringLiteral(value: p.l.currentToken.literal, line: p.l.currentToken.line, col: p.l.currentToken.col)
  consumeToken(p) # Consume the string literal token

proc parseCharLiteral*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  if currentToken.literal.len == 1:
     result = CharLiteral(value: currentToken.literal[0], line: currentToken.line, col: currentToken.col)
     consumeToken(p) # Consume the char literal token
  else:
     p.errors.add("Invalid character literal content from lexer: '" & currentToken.literal & "'")
     consumeToken(p) # Consume the invalid token

proc parsePrefixExpression*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let operatorToken = p.l.currentToken
  consumeToken(p) # Consume the prefix operator
  let right = p.parseExpression(Precedence.Prefix) # Use qualified Prefix
  if right == nil:
      p.errors.add("Failed to parse operand for prefix operator " & $operatorToken.kind)
      return nil
  # Create and return the UnaryExpression node
  result = UnaryExpression(
    op: operatorToken.kind,
    operand: right,
    isPrefix: true, # All operators handled here are prefix
    line: operatorToken.line,
    col: operatorToken.col
  )

proc parseInfixExpression*(p: var Parser, left: Expression): Expression = # Exported for registration
  result = nil # ProveInit
  let operatorToken = p.l.currentToken
  let precedence = p.currentPrecedence()
  consumeToken(p)

  var right: Expression
  if precedence == Precedence.Assign: # Use qualified Assign
    right = p.parseExpression(precedence.pred) # Slightly lower precedence for right operand
  else:
    right = p.parseExpression(precedence)

  if right == nil:
      p.errors.add("Failed to parse right operand for infix operator " & $operatorToken.kind)
      return nil

  result = BinaryExpression(left: left, op: operatorToken.kind, right: right, line: operatorToken.line, col: operatorToken.col)

proc parseGroupedExpression*(p: var Parser): Expression = # Exported for registration
  consumeToken(p) # Consume '('
  let exp = p.parseExpression(Precedence.Lowest) # Use qualified Lowest
  if not p.expectPeek(tkRParen): # Checks for ')' and consumes it if present
    return nil
  # expectPeek already consumed ')', no need for consumeToken(p) here
  return exp

proc parseCallExpression*(p: var Parser, function: Expression): Expression = # Exported for registration
  result = nil # ProveInit
  # Assumes currentToken is tkLParen (already consumed by infix logic)
  var args: seq[Expression] = @[]
  let line = p.l.currentToken.line; let col = p.l.currentToken.col # Capture '(' location

  if p.l.peekToken.kind != tkRParen:
    consumeToken(p) # Consume first token of first argument
    let firstArg = p.parseExpression(Precedence.Assign) # Use qualified Assign
    if firstArg == nil:
        p.errors.add("Failed to parse first argument in function call at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
        return nil
    args.add(firstArg)
    while p.l.currentToken.kind == tkComma:
      consumeToken(p) # Consume ','
      let nextArg = p.parseExpression(Precedence.Assign) # Use qualified Assign
      if nextArg == nil:
          p.errors.add("Failed to parse subsequent argument in function call at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
          return nil
      args.add(nextArg)

  # Now, the current token *should* be the closing parenthesis ')'
  if p.l.currentToken.kind != tkRParen:
    let errorLine = if function != nil: function.line else: line # Use '(' line if possible
    let errorCol = if function != nil: function.col else: col # Use '(' col if possible
    p.errors.add("Expected ')' or ',' in argument list, got " & $p.l.currentToken.kind & " at " & $errorLine & ":" & $errorCol)
    return nil
  consumeToken(p) # Consume ')'

  result = CallExpression(function: function, arguments: args, line: function.line, col: function.col) # Use function's location

proc parseInitializerListExpression*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let line = p.l.currentToken.line
  let col = p.l.currentToken.col
  consumeToken(p) # Consume '{'

  var values: seq[Expression] = @[]

  if p.l.currentToken.kind == tkRBrace:
    consumeToken(p) # Consume '}'
    return InitializerListExpr(values: values, line: line, col: col)

  let firstValue = p.parseExpression(Precedence.Assign) # Use qualified Assign
  if firstValue == nil:
    p.errors.add("Failed to parse first value in initializer list at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    return nil
  values.add(firstValue)

  while p.l.currentToken.kind == tkComma:
    consumeToken(p) # Consume ','
    if p.l.currentToken.kind == tkRBrace: break # Trailing comma
    let nextValue = p.parseExpression(Precedence.Assign) # Use qualified Assign
    if nextValue == nil:
      p.errors.add("Failed to parse subsequent value in initializer list at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
      return nil
    values.add(nextValue)

  if p.l.currentToken.kind != tkRBrace:
    p.errors.add("Expected '}' or ',' in initializer list, got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    return nil
  consumeToken(p) # Consume '}'

  result = InitializerListExpr(values: values, line: line, col: col)

proc parseMemberAccessExpression*(p: var Parser, left: Expression): Expression = # Exported for registration
  result = nil # ProveInit
  let operatorToken = p.l.currentToken # Should be tkDot or tkArrow
  # DO NOT consumeToken(p) here. Infix logic already did.

  if p.l.peekToken.kind != tkIdentifier: # Peek ahead for the member name
    p.errors.add("Expected identifier after '" & $operatorToken.kind & "', got " & $p.l.peekToken.kind & " at " & $p.l.peekToken.line & ":" & $p.l.peekToken.col)
    return nil
  consumeToken(p) # Consume the identifier token

  let memberIdent = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)

  result = MemberAccessExpression(
    targetObject: left,
    member: memberIdent, # No need to cast
    isArrow: (operatorToken.kind == tkArrow),
    line: operatorToken.line, # Use operator token for line/col
    col: operatorToken.col
  )

proc parseIndexExpression*(p: var Parser, left: Expression): Expression = # Exported for registration
  result = nil # ProveInit
  let lbracketToken = p.l.currentToken # Should be tkLBracket
  # DO NOT consumeToken(p) here. Infix logic already did.

  consumeToken(p) # Consume the token *after* '[' to start parsing the index expression
  let indexExpr = p.parseExpression(Precedence.Lowest)
  if indexExpr == nil:
    p.errors.add("Failed to parse index expression within '[]'")
    return nil

  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected ']' after index expression, got " & $p.l.currentToken.kind)
    return nil
  consumeToken(p) # Consume ']'

  result = ArraySubscriptExpression(
    array: left,
    index: indexExpr,
    line: lbracketToken.
# C++ Expression Parsing Implementation for nifcpp

import std/strutils
import std/options
import std/tables # Add direct import for getOrDefault
import ./lexer
import ./ast
import ./parser_core # Import core types and helpers
# Import modules containing the actual implementations needed
import ./parser_types # For parseType, parseParameterList
import ./parser_statements # For parseBlockStatement

# --- Forward declaration for recursive expression parsing ---
proc parseExpression*(p: var Parser, precedence: Precedence): Expression # Forward

# --- Expression Parsing Implementations ---

proc parseIdentifier*(p: var Parser): Expression = # Exported for registration
  result = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p) # Consume the identifier token

proc parseIntegerLiteral*(p: var Parser): Expression = # Exported for registration
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils
