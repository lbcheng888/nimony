# C++ Parser - Expression Parsing Logic

import std/strutils
import std/options # Import Option type
import ./lexer
import ./ast
import ./parser_core # Import core definitions (Parser type, Precedence, helpers, and forward declarations)
# Removed imports for parser_types and parser_statements to break circular dependency

# --- Expression Parsing Implementations ---

proc parseIdentifier(p: var Parser): Expression =
  result = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p) # Consume the identifier token

proc parseIntegerLiteral(p: var Parser): Expression =
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseBiggestInt(currentToken.literal)
    result = IntegerLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the integer literal token
  except ValueError:
    p.errors.add("Invalid integer literal: " & currentToken.literal)
    result = nil
    consumeToken(p) # Consume the invalid token

proc parseFloatLiteral(p: var Parser): Expression =
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseFloat(currentToken.literal)
    result = FloatLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the float literal token
  except ValueError:
    p.errors.add("Invalid float literal: " & currentToken.literal)
    result = nil
    consumeToken(p) # Consume the invalid token

proc parseStringLiteral(p: var Parser): Expression =
  result = StringLiteral(value: p.l.currentToken.literal, line: p.l.currentToken.line, col: p.l.currentToken.col)
  consumeToken(p) # Consume the string literal token

proc parseCharLiteral(p: var Parser): Expression =
  let currentToken = p.l.currentToken # Capture token info before consuming
  if currentToken.literal.len == 1:
     result = CharLiteral(value: currentToken.literal[0], line: currentToken.line, col: currentToken.col)
     consumeToken(p) # Consume the char literal token
  else:
     p.errors.add("Invalid character literal content from lexer: '" & currentToken.literal & "'")
     result = nil
     consumeToken(p) # Consume the invalid token
     return nil

proc parsePrefixExpression(p: var Parser): Expression =
  let operatorToken = p.l.currentToken
  consumeToken(p) # Consume the prefix operator
  let right = p.parseExpression(Precedence.Prefix) # Use qualified Prefix
  if right == nil:
      p.errors.add("Failed to parse operand for prefix operator " & $operatorToken.kind)
      return nil
  # Create and return the UnaryExpression node
  return UnaryExpression(
    op: operatorToken.kind,
    operand: right,
    isPrefix: true, # All operators handled here are prefix
    line: operatorToken.line,
    col: operatorToken.col
  )

proc parseInfixExpression(p: var Parser, left: Expression): Expression =
  # Corrected Debug: Use repr()
  # echo "[DEBUG infixExpr] Start. Operator=", $p.l.currentToken.kind, " Left=", (if left != nil: repr(left) else: "nil"), " current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  # The infix operator is the current token (p.l.currentToken).
  let operatorToken = p.l.currentToken
  let precedence = p.currentPrecedence()

  # Consume the operator token *before* parsing the right operand.
  consumeToken(p)
  # echo "[DEBUG infixExpr] Consumed operator. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind

  # Handle right-associativity for assignment
  var right: Expression
  # echo "[DEBUG infixExpr] Parsing right operand with precedence: ", $precedence
  if precedence == Precedence.Assign: # Use qualified Assign
    right = p.parseExpression(precedence.pred) # Slightly lower precedence for right operand
  else:
    right = p.parseExpression(precedence)

  # Corrected Debug: Use repr()
  # echo "[DEBUG infixExpr] Parsed right operand. Right=", (if right != nil: repr(right) else: "nil"), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  if right == nil:
      p.errors.add("Failed to parse right operand for infix operator " & $operatorToken.kind)
      return nil

  # TODO: Handle specific infix operators like [], ., -> if needed
  # For now, assume BinaryExpression covers most cases
  let resultNode = BinaryExpression(left: left, op: operatorToken.kind, right: right, line: operatorToken.line, col: operatorToken.col)
  # echo "[DEBUG infixExpr] Created BinaryExpr. Returning. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  return resultNode

# Added implementation for parseGroupedExpression
proc parseGroupedExpression(p: var Parser): Expression =
  consumeToken(p) # Consume '('
  let exp = p.parseExpression(Precedence.Lowest) # Use qualified Lowest
  if not p.expectPeek(tkRParen): # Checks for ')' and consumes '(' if present
    return nil
  consumeToken(p) # Consume ')'
  return exp

proc parseCallExpression(p: var Parser, function: Expression): Expression =
  # Assumes currentToken is tkLParen (already consumed by infix logic)
  # echo "[DEBUG parseCallExpr] Start. Function=", repr(function), " current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  var args: seq[Expression] = @[]
  if p.l.peekToken.kind != tkRParen:
    consumeToken(p) # Consume first token of first argument
    # echo "[DEBUG parseCallExpr] Parsing first argument. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    let firstArg = p.parseExpression(Precedence.Assign) # Use qualified Assign
    # echo "[DEBUG parseCallExpr] Parsed first argument. Result=", (if firstArg != nil: repr(firstArg) else: "nil"), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    if firstArg == nil:
        p.errors.add("Failed to parse first argument in function call at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
        return nil
    args.add(firstArg)
    # Corrected loop condition: Check CURRENT token for comma
    while p.l.currentToken.kind == tkComma:
      consumeToken(p) # Consume ','
      # Now the current token should be the start of the next argument
      # echo "[DEBUG parseCallExpr] Parsing next argument after comma. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
      let nextArg = p.parseExpression(Precedence.Assign) # Use qualified Assign
      # echo "[DEBUG parseCallExpr] Parsed next argument. Result=", (if nextArg != nil: repr(nextArg) else: "nil"), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
      if nextArg == nil:
          p.errors.add("Failed to parse subsequent argument in function call at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
          return nil
      args.add(nextArg)

  # Now, the current token *should* be the closing parenthesis ')'
  # echo "[DEBUG parseCallExpr] Finished parsing arguments. Expecting ')'. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  if p.l.currentToken.kind != tkRParen:
    # Use line/col from the opening parenthesis if possible for better error location
    let errorLine = if function != nil: function.line else: p.l.currentToken.line
    let errorCol = if function != nil: function.col else: p.l.currentToken.col
    p.errors.add("Expected ')' or ',' in argument list, got " & $p.l.currentToken.kind & " at " & $errorLine & ":" & $errorCol)
    return nil
  let line = p.l.currentToken.line; let col = p.l.currentToken.col # Capture ')' location before consuming
  consumeToken(p) # Consume ')'
  # echo "[DEBUG parseCallExpr] Consumed ')'. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind

  return CallExpression(function: function, arguments: args, line: function.line, col: function.col)

proc parseInitializerListExpression(p: var Parser): Expression =
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

  return InitializerListExpr(values: values, line: line, col: col)

# Added implementation for parseMemberAccessExpression
proc parseMemberAccessExpression(p: var Parser, left: Expression): Expression =
  # The member access operator is the current token (p.l.currentToken)
  # It was already consumed by the main parseExpression loop.
  let operatorToken = p.l.currentToken # Should be tkDot or tkArrow
  # DO NOT consumeToken(p) here.

  if p.l.currentToken.kind != tkIdentifier:
    p.errors.add("Expected identifier after '" & $operatorToken.literal & "', got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col) # Improved error
    return nil

  let memberIdent = p.parseIdentifier() # Parse the member identifier
  if memberIdent == nil or not (memberIdent of Identifier): # Check if parsing succeeded and is Identifier
      p.errors.add("Failed to parse member identifier after '" & $operatorToken.kind & "'")
      return nil

  return MemberAccessExpression(
    targetObject: left, # Use the renamed field
    member: Identifier(memberIdent), # Cast to Identifier
    isArrow: (operatorToken.kind == tkArrow),
    line: operatorToken.line, # Use operator token for line/col
    col: operatorToken.col
  )

# Added implementation for parseIndexExpression
proc parseIndexExpression(p: var Parser, left: Expression): Expression =
  # The '[' operator is the current token (p.l.currentToken)
  # It was already consumed by the main parseExpression loop.
  let lbracketToken = p.l.currentToken # Should be tkLBracket
  # DO NOT consumeToken(p) here.

  let indexExpr = p.parseExpression(Precedence.Lowest)
  if indexExpr == nil:
    p.errors.add("Failed to parse index expression within '[]'")
    return nil

  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected ']' after index expression, got " & $p.l.currentToken.kind)
    return nil
  consumeToken(p) # Consume ']'

  return ArraySubscriptExpression(
    array: left,
    index: indexExpr,
    line: lbracketToken.line, # Use '[' token for line/col
    col: lbracketToken.col
  )

# Added implementation for parsePostfixExpression
proc parsePostfixExpression(p: var Parser, left: Expression): Expression =
  # Assumes currentToken is tkIncrement or tkDecrement (already consumed by infix logic)
  let operatorToken = p.l.currentToken
  # No need to consume token here, infix logic already did.
  # We just need to create the node.
  return UnaryExpression(
    op: operatorToken.kind,
    operand: left,
    isPrefix: false, # This handles postfix operators
    line: operatorToken.line,
    col: operatorToken.col
  )

proc parseLambdaExpression(p: var Parser): Expression =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  if p.l.currentToken.kind != tkLBracket:
    p.errors.add("Expected '[' to start lambda expression")
    return nil
  consumeToken(p) # Consume '['

  var captureDefault: Option[TokenType] = none(TokenType)
  var captures: seq[Identifier] = @[]

  # Parse capture list
  if p.l.currentToken.kind == tkAssign:
    captureDefault = some(tkAssign)
    consumeToken(p)
    if p.l.currentToken.kind == tkComma: consumeToken(p) # Optional comma after default capture
  elif p.l.currentToken.kind == tkBitwiseAnd:
    captureDefault = some(tkBitwiseAnd)
    consumeToken(p)
    if p.l.currentToken.kind == tkComma: consumeToken(p) # Optional comma after default capture

  while p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkEndOfFile:
    # Simplified capture parsing: just identifiers for now
    # TODO: Handle '&var', 'this', '=var' (init-capture)
    if p.l.currentToken.kind == tkIdentifier:
      captures.add(newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col))
      consumeToken(p)
    else:
      p.errors.add("Expected identifier or ']' in lambda capture list, got " & $p.l.currentToken.kind)
      # Attempt recovery
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
      if p.l.currentToken.kind == tkEndOfFile: return nil

    if p.l.currentToken.kind == tkComma:
      consumeToken(p)
    elif p.l.currentToken.kind != tkRBracket:
      p.errors.add("Expected ',' or ']' after capture item, got " & $p.l.currentToken.kind)
      # Attempt recovery
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
      if p.l.currentToken.kind == tkEndOfFile: return nil


  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected ']' to end lambda capture list")
    return nil
  consumeToken(p) # Consume ']'

  # Parse parameter list (optional)
  var parameters: seq[Parameter] = @[]
  if p.l.currentToken.kind == tkLParen:
    let errorCountBefore = p.errors.len
    parameters = p.parseParameterList()
    if p.errors.len > errorCountBefore:
      p.errors.add("Failed to parse lambda parameter list")
      return nil # Error during parameter parsing

  # Parse trailing return type (optional)
  var returnType: TypeExpr = nil
  if p.l.currentToken.kind == tkArrow:
    consumeToken(p) # Consume '->'
    returnType = p.parseType()
    if returnType == nil:
      p.errors.add("Failed to parse lambda return type after '->'")
      return nil

  # Parse body
  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for lambda body")
    return nil
  let bodyStmt = p.parseBlockStatement()
  if bodyStmt == nil or not (bodyStmt of BlockStatement):
    p.errors.add("Failed to parse lambda body")
    return nil

  return LambdaExpression(
    captureDefault: captureDefault,
    captures: captures,
    parameters: parameters,
    returnType: returnType,
    body: BlockStatement(bodyStmt),
    line: line, col: col
  )

# --- Exception Handling Implementations ---

proc parseThrowExpression(p: var Parser): Expression =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'throw'

  var expr: Expression = nil
  # Check if there's an expression to parse (not just 'throw;')
  if p.l.currentToken.kind != tkSemicolon:
    expr = p.parseExpression(Precedence.Assign) # Throw expression has low precedence binding
    if expr == nil:
      p.errors.add("Failed to parse expression after 'throw'")
      # Don't return nil immediately, let semicolon check handle final failure if used as statement

  # Note: Throw can be an expression (e.g., x = throw Y;) or a statement (throw Y;).
  # If used as a statement, parseExpressionStatement will handle the semicolon.
  # If used as an expression, the surrounding expression parser handles context.
  return ThrowExpression(expression: expr, line: line, col: col)

# --- Main Parsing Logic ---

proc parseExpression(p: var Parser, precedence: Precedence): Expression =
  # Find prefix function for the current token
  let prefixFn = p.prefixParseFns.getOrDefault(p.l.currentToken.kind)
  if prefixFn == nil:
    p.errors.add("No prefix parse function found for " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    consumeToken(p) # Consume the problematic token to prevent loops
    return nil

  var leftExp = prefixFn(p) # Prefix function MUST consume its token(s).
  if leftExp == nil: # Prefix function failed (assume it consumed its token or errored appropriately)
      return nil

  # echo "[DEBUG parseExpr] After prefix. leftExp=", (if leftExp != nil: repr(leftExp) else: "nil"), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind, " currentPrec=", $precedence

  # --- Corrected Pratt Loop ---
  # Loop while the *current* token is an infix operator AND
  # the current context precedence is lower than the *current* token's precedence.
  while p.infixParseFns.hasKey(p.l.currentToken.kind) and precedence < p.currentPrecedence():
    # echo "[DEBUG parseExpr] Loop Condition Met: currentPrec=", $precedence, " < currentOpPrec=", $p.currentPrecedence(), " (currentOp=", $p.l.currentToken.kind, ")"
    # Get the infix function for the *current* token (which is the operator).
    let infixFn = p.infixParseFns[p.l.currentToken.kind] # Guaranteed to exist due to loop condition
    # DO NOT consume token here. The operator is already the current token.
    # The infixFn is responsible for consuming the operator and parsing the right operand.
    # echo "[DEBUG parseExpr] Calling infixFn for operator: ", $p.l.currentToken.kind
    leftExp = infixFn(p, leftExp) # Pass the current leftExp
    # Corrected Debug: Use repr()
    # echo "[DEBUG parseExpr] Returned from infixFn. leftExp=", (if leftExp != nil: repr(leftExp) else: "nil"), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    if leftExp == nil: # Infix function failed
        return nil
    # Loop continues, checking the *new* currentToken
  # echo "[DEBUG parseExpr] Loop finished or condition not met. Returning leftExp. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  return leftExp
