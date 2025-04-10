# Combined C++ Parser Implementation for nifcpp

import std/sequtils
import std/tables
import std/strutils
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
    errors*: seq[string]
    prefixParseFns: Table[TokenType, proc (p: var Parser): Expression]
    infixParseFns: Table[TokenType, proc (p: var Parser, left: Expression): Expression]

# --- Forward Declarations ---
proc parseExpression*(p: var Parser, precedence: Precedence): Expression # Exported
proc parseStatement(p: var Parser): Statement
proc parseBlockStatement(p: var Parser): Statement
proc parseType(p: var Parser): TypeExpr
proc parseDeclarator(p: var Parser, baseType: TypeExpr): (Identifier, TypeExpr)
proc parseParameterList(p: var Parser): seq[Parameter]
proc parseTemplateParameterList(p: var Parser): Option[seq[TemplateParameter]]
proc parseAttributeSpecifierSeq*(p: var Parser): seq[AttributeGroup] # Exported
proc parseEnumerator(p: var Parser): Enumerator
proc parseCatchClause(p: var Parser): CatchClause
proc parseDeclarationStatement(p: var Parser,
                               parsedType: TypeExpr = nil,
                               requireSemicolon: bool = true,
                               attributes: seq[AttributeGroup] = @[]): Statement
proc parseExpressionStatement(p: var Parser): Statement
proc parseIfStatement(p: var Parser): Statement
proc parseWhileStatement(p: var Parser): Statement
proc parseForStatement(p: var Parser): Statement
proc parseReturnStatement(p: var Parser): Statement
proc parseTryStatement(p: var Parser): Statement
proc parseTemplateDeclaration(p: var Parser): Statement
proc parseNamespaceDefinition(p: var Parser): Statement
proc parseClassDefinition(p: var Parser): Statement
proc parseBreakStatement(p: var Parser): Statement
proc parseContinueStatement(p: var Parser): Statement
proc parseSwitchStatement(p: var Parser): Statement
proc parseCaseStatement(p: var Parser): Statement
proc parseEnumDefinition(p: var Parser): Statement
proc parseTypeAliasDeclaration(p: var Parser): Statement
proc parseDecltypeSpecifier(p: var Parser): TypeExpr
proc parseTranslationUnit*(p: var Parser): TranslationUnit # Exported

# --- Helper Procs ---
proc consumeToken*(p: var Parser) =
  nextToken(p.l)

proc peekError*(p: var Parser, expected: TokenType) =
  let msg = "Expected next token to be " & $expected & ", got " & $p.l.peekToken.kind & " instead at " & $p.l.peekToken.line & ":" & $p.l.peekToken.col & "."
  p.errors.add(msg)

proc expectPeek*(p: var Parser, expected: TokenType): bool =
  if p.l.peekToken.kind == expected:
    consumeToken(p)
    return true
  else:
    p.peekError(expected)
    return false # Explicitly return false to satisfy ProveInit

# --- Precedence Helper Procs ---
const precedences: Table[TokenType, Precedence] = {
  tkAssign: Precedence.Assign, tkPlusAssign: Precedence.Assign, tkMinusAssign: Precedence.Assign,
  tkMulAssign: Precedence.Assign, tkDivAssign: Precedence.Assign, tkModAssign: Precedence.Assign,
  tkAndAssign: Precedence.Assign, tkOrAssign: Precedence.Assign, tkXorAssign: Precedence.Assign,
  tkLShiftAssign: Precedence.Assign, tkRShiftAssign: Precedence.Assign,
  tkQuestion: Precedence.Conditional, # Ternary
  tkLogicalOr: Precedence.LogicalOr,
  tkLogicalAnd: Precedence.LogicalAnd,
  tkBitwiseOr: Precedence.BitwiseOr,
  tkBitwiseXor: Precedence.BitwiseXor,
  tkBitwiseAnd: Precedence.BitwiseAnd, # Also used for address-of (Prefix)
  tkEqual: Precedence.Equality, tkNotEqual: Precedence.Equality,
  tkLess: Precedence.Relational, tkGreater: Precedence.Relational,
  tkLessEqual: Precedence.Relational, tkGreaterEqual: Precedence.Relational,
  tkLeftShift: Precedence.Shift, tkRightShift: Precedence.Shift,
  tkPlus: Precedence.Additive, tkMinus: Precedence.Additive,
  tkStar: Precedence.Multiplicative, # Also used for dereference (Prefix)
  tkSlash: Precedence.Multiplicative, tkPercent: Precedence.Multiplicative,
  # Prefix operators have their own precedence level handled by parseExpression
  tkIncrement: Precedence.Postfix, # Also used for prefix
  tkDecrement: Precedence.Postfix, # Also used for prefix
  tkLParen: Precedence.Call, # Also used for grouping (Lowest)
  tkLBracket: Precedence.Index, # Also used for attributes/lambda start (Lowest?)
  tkDot: Precedence.Member, tkArrow: Precedence.Member
}.toTable

proc peekPrecedence*(p: Parser): Precedence = # Changed to non-var Parser
  result = precedences.getOrDefault(p.l.peekToken.kind, Precedence.Lowest)

proc currentPrecedence*(p: Parser): Precedence = # Changed to non-var Parser
  result = precedences.getOrDefault(p.l.currentToken.kind, Precedence.Lowest)

# --- Constructor ---
proc newParser*(l: Lexer): Parser = # Exported
  new(result)
  result.l = l
  result.errors = @[]
  result.prefixParseFns = initTable[TokenType, proc (p: var Parser): Expression]()
  result.infixParseFns = initTable[TokenType, proc (p: var Parser, left: Expression): Expression]()
  # Registration happens in parser.nim

# --- Registration Procs ---
proc registerPrefix*(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser): Expression) = # Exported
  p.prefixParseFns[tokenType] = fn

proc registerInfix*(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser, left: Expression): Expression) = # Exported
  p.infixParseFns[tokenType] = fn

# --- Attribute Implementations ---

proc parseAttributeArgument(p: var Parser): AttributeArgument =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let expr = p.parseExpression(Precedence.Assign) # Use Assign precedence? Lowest?
  if expr == nil:
    p.errors.add("Failed to parse attribute argument expression")
    return nil
  result = AttributeArgument(value: expr, line: line, col: col)

proc parseAttribute(p: var Parser): Attribute =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  var scope: Identifier = nil
  var name: Identifier = nil

  if p.l.currentToken.kind != tkIdentifier:
    p.errors.add("Expected identifier for attribute name")
    return nil
  name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p)

  # Check for scoped attribute (e.g., gnu::const)
  if p.l.currentToken.kind == tkScope: # ::
    consumeToken(p)
    if p.l.currentToken.kind != tkIdentifier:
      p.errors.add("Expected identifier after scope resolution operator '::' in attribute")
      return nil
    scope = name # The first identifier was the scope
    name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)

  var arguments: seq[AttributeArgument] = @[]
  if p.l.currentToken.kind == tkLParen:
    consumeToken(p) # Consume '('
    if p.l.currentToken.kind != tkRParen:
      let firstArg = p.parseAttributeArgument()
      if firstArg == nil: return nil # Error already added
      arguments.add(firstArg)
      while p.l.currentToken.kind == tkComma:
        consumeToken(p) # Consume ','
        if p.l.currentToken.kind == tkRParen: break # Trailing comma?
        let nextArg = p.parseAttributeArgument()
        if nextArg == nil: return nil # Error already added
        arguments.add(nextArg)

    if p.l.currentToken.kind != tkRParen:
      p.errors.add("Expected ')' or ',' in attribute argument list")
      return nil
    consumeToken(p) # Consume ')'

  result = Attribute(scope: scope, name: name, arguments: arguments, line: line, col: col)

proc parseAttributeGroup(p: var Parser): AttributeGroup =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  if p.l.currentToken.kind != tkLBracket: # Should be [[
    p.errors.add("Internal error: parseAttributeGroup called without '[['")
    return nil
  consumeToken(p) # Consume first '['
  if p.l.currentToken.kind != tkLBracket:
    p.errors.add("Expected second '[' for attribute specifier")
    return nil
  consumeToken(p) # Consume second '['

  var attributes: seq[Attribute] = @[]
  if p.l.currentToken.kind != tkRBracket: # Check for empty [[ ]]
    let firstAttr = p.parseAttribute()
    if firstAttr == nil: return nil # Error already added
    attributes.add(firstAttr)

    while p.l.currentToken.kind == tkComma:
      consumeToken(p) # Consume ','
      if p.l.currentToken.kind == tkRBracket: break # Trailing comma?
      let nextAttr = p.parseAttribute()
      if nextAttr == nil: return nil # Error already added
      attributes.add(nextAttr)

  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected ']]' or ',' in attribute list")
    return nil
  consumeToken(p) # Consume first ']'
  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected second ']' to close attribute specifier")
    # Attempt recovery? Consume the token anyway?
    if p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
    return nil
  consumeToken(p) # Consume second ']'

  result = AttributeGroup(attributes: attributes, line: line, col: col)

proc parseAttributeSpecifierSeq*(p: var Parser): seq[AttributeGroup] = # Exported
  result = @[]
  while p.l.currentToken.kind == tkLBracket and p.l.peekToken.kind == tkLBracket:
    let group = p.parseAttributeGroup()
    if group != nil:
      result.add(group)
    else:
      # Error occurred during group parsing, stop parsing attributes
      break

# --- Expression Parsing Implementations ---

proc parseIdentifier*(p: var Parser): Expression = # Exported
  result = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p) # Consume the identifier token

proc parseIntegerLiteral*(p: var Parser): Expression = # Exported
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseBiggestInt(currentToken.literal)
    result = IntegerLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the integer literal token
  except ValueError:
    p.errors.add("Invalid integer literal: " & currentToken.literal)
    consumeToken(p) # Consume the invalid token

proc parseFloatLiteral*(p: var Parser): Expression = # Exported
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  try:
    let val = strutils.parseFloat(currentToken.literal)
    result = FloatLiteral(value: val, line: currentToken.line, col: currentToken.col)
    consumeToken(p) # Consume the float literal token
  except ValueError:
    p.errors.add("Invalid float literal: " & currentToken.literal)
    consumeToken(p) # Consume the invalid token

proc parseStringLiteral*(p: var Parser): Expression = # Exported
  result = StringLiteral(value: p.l.currentToken.literal, line: p.l.currentToken.line, col: p.l.currentToken.col)
  consumeToken(p) # Consume the string literal token

proc parseCharLiteral*(p: var Parser): Expression = # Exported
  result = nil # ProveInit
  let currentToken = p.l.currentToken # Capture token info before consuming
  if currentToken.literal.len == 1:
     result = CharLiteral(value: currentToken.literal[0], line: currentToken.line, col: currentToken.col)
     consumeToken(p) # Consume the char literal token
  else:
     p.errors.add("Invalid character literal content from lexer: '" & currentToken.literal & "'")
     consumeToken(p) # Consume the invalid token

proc parsePrefixExpression*(p: var Parser): Expression = # Exported
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

proc parseInfixExpression*(p: var Parser, left: Expression): Expression = # Exported
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

proc parseGroupedExpression*(p: var Parser): Expression = # Exported
  consumeToken(p) # Consume '('
  let exp = p.parseExpression(Precedence.Lowest) # Use qualified Lowest
  if not p.expectPeek(tkRParen): # Checks for ')' and consumes it if present
    return nil
  # expectPeek already consumed ')', no need for consumeToken(p) here
  return exp

proc parseCallExpression*(p: var Parser, function: Expression): Expression = # Exported
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

proc parseInitializerListExpression*(p: var Parser): Expression = # Exported
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

proc parseMemberAccessExpression*(p: var Parser, left: Expression): Expression = # Exported
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

proc parseIndexExpression*(p: var Parser, left: Expression): Expression = # Exported
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
    line: lbracketToken.line, # Use '[' token for line/col
    col: lbracketToken.col
  )

proc parsePostfixExpression*(p: var Parser, left: Expression): Expression = # Exported
  let operatorToken = p.l.currentToken # Should be tkIncrement or tkDecrement
  # DO NOT consumeToken(p) here. Infix logic already did.
  return UnaryExpression(
    op: operatorToken.kind,
    operand: left,
    isPrefix: false, # This handles postfix operators
    line: operatorToken.line,
    col: operatorToken.col
  )

proc parseLambdaExpression*(p: var Parser): Expression = # Exported
  result = nil # ProveInit
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
    if p.l.currentToken.kind == tkIdentifier:
      captures.add(newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col))
      consumeToken(p)
    else:
      p.errors.add("Expected identifier or ']' in lambda capture list, got " & $p.l.currentToken.kind)
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
      if p.l.currentToken.kind == tkEndOfFile: return nil

    if p.l.currentToken.kind == tkComma:
      consumeToken(p)
    elif p.l.currentToken.kind != tkRBracket:
      p.errors.add("Expected ',' or ']' after capture item, got " & $p.l.currentToken.kind)
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
      if p.l.currentToken.kind == tkEndOfFile: return nil

  if p.l.currentToken.kind != tkRBracket:
    p.errors.add("Expected ']' to end lambda capture list")
    return nil
  consumeToken(p) # Consume ']'

  var parameters: seq[Parameter] = @[]
  if p.l.currentToken.kind == tkLParen:
    let errorCountBefore = p.errors.len
    parameters = p.parseParameterList()
    if p.errors.len > errorCountBefore:
      p.errors.add("Failed to parse lambda parameter list")
      return nil

  var returnType: TypeExpr = nil
  if p.l.currentToken.kind == tkArrow:
    consumeToken(p) # Consume '->'
    returnType = p.parseType()
    if returnType == nil:
      p.errors.add("Failed to parse lambda return type after '->'")
      return nil

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for lambda body")
    return nil
  let bodyStmt = p.parseBlockStatement()
  if bodyStmt == nil or not (bodyStmt of BlockStatement):
    p.errors.add("Failed to parse lambda body")
    return nil

  result = LambdaExpression(
    captureDefault: captureDefault,
    captures: captures,
    parameters: parameters,
    returnType: returnType,
    body: BlockStatement(bodyStmt),
    line: line, col: col
  )

proc parseThrowExpression*(p: var Parser): Expression = # Exported
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'throw'

  var expr: Expression = nil
  if p.l.currentToken.kind != tkSemicolon:
    expr = p.parseExpression(Precedence.Assign)
    if expr == nil:
      p.errors.add("Failed to parse expression after 'throw'")

  result = ThrowExpression(expression: expr, line: line, col: col)

# --- Main Expression Parsing Logic ---
proc parseExpression*(p: var Parser, precedence: Precedence): Expression = # Exported
  result = nil # ProveInit
  let prefixFn = p.prefixParseFns.getOrDefault(p.l.currentToken.kind)
  if prefixFn == nil:
    p.errors.add("No prefix parse function found for " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    consumeToken(p)
    return nil

  var leftExp = prefixFn(p)
  if leftExp == nil:
      return nil

  # Corrected Pratt loop logic: Check precedence of the *next* token (peek)
  while p.l.currentToken.kind != tkSemicolon and precedence < p.peekPrecedence():
    let infixFn = p.infixParseFns.getOrDefault(p.l.peekToken.kind) # Check infix for the *next* token
    if infixFn == nil:
      return leftExp # No infix operator or lower precedence

    consumeToken(p) # Consume the operator (which is now the current token)
    leftExp = infixFn(p, leftExp) # Call infix function
    if leftExp == nil: # Infix function failed
        return nil
    # Loop continues, checking the *new* peekToken's precedence

  result = leftExp

# --- Type, Declarator, Parameter Parsing ---

proc parseType(p: var Parser): TypeExpr =
  result = nil # ProveInit
  let attributes = p.parseAttributeSpecifierSeq()
  var initialConst = false
  var initialVolatile = false
  var qualifierToken = p.l.currentToken

  while p.l.currentToken.kind == tkConst or p.l.currentToken.kind == tkVolatile:
    if not initialConst and not initialVolatile: qualifierToken = p.l.currentToken
    if p.l.currentToken.kind == tkConst: initialConst = true
    else: initialVolatile = true
    consumeToken(p)

  var baseType: TypeExpr
  let currentKind = p.l.currentToken.kind
  let line = p.l.currentToken.line
  let col = p.l.currentToken.col

  if currentKind == tkIdentifier or
     currentKind == tkVoid or currentKind == tkBool or currentKind == tkChar or
     currentKind == tkInt or currentKind == tkFloat or currentKind == tkDouble or
     currentKind == tkLong or currentKind == tkShort or currentKind == tkSigned or
     currentKind == tkUnsigned or currentKind == tkAuto:
    let typeName = p.l.currentToken.literal
    consumeToken(p)
    baseType = BaseTypeExpr(name: typeName, line: line, col: col)
  elif currentKind == tkDecltype:
    baseType = p.parseDecltypeSpecifier()
    if baseType == nil: return nil
  else:
    let expectedMsg = if initialConst or initialVolatile: "base type name after qualifier(s)" else: "base type name or decltype"
    p.errors.add("Expected " & expectedMsg & ", got " & $p.l.currentToken.kind & " at " & $line & ":" & $col)
    return nil

  var currentType = baseType
  if initialVolatile:
    currentType = VolatileQualifiedTypeExpr(baseType: currentType, line: qualifierToken.line, col: qualifierToken.col)
  if initialConst:
    currentType = ConstQualifiedTypeExpr(baseType: currentType, line: qualifierToken.line, col: qualifierToken.col)

  if attributes.len > 0:
      if currentType != nil:
          # Assuming TypeExpr and subtypes have 'attributes' field
          currentType.attributes = attributes
      else:
          p.errors.add("Attributes parsed but no valid type node created.")

  result = currentType

proc parseDecltypeSpecifier(p: var Parser): TypeExpr =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  if p.l.currentToken.kind != tkDecltype:
    p.errors.add("Internal error: parseDecltypeSpecifier called without tkDecltype")
    return nil
  consumeToken(p) # Consume 'decltype'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('

  let expr = p.parseExpression(Precedence.Lowest)
  if expr == nil:
    p.errors.add("Failed to parse expression inside decltype()")
    return nil

  if not p.expectPeek(tkRParen): return nil
  # expectPeek consumed ')'
  result = DecltypeSpecifier(expression: expr, line: line, col: col)

proc parseDeclarator(p: var Parser, baseType: TypeExpr): (Identifier, TypeExpr) =
  var derivedType = baseType
  var line = p.l.currentToken.line
  var col = p.l.currentToken.col
  var name: Identifier = nil

  # Parse pointer/reference/qualifier prefixes
  while p.l.currentToken.kind == tkStar or
        p.l.currentToken.kind == tkBitwiseAnd or
        p.l.currentToken.kind == tkConst or
        p.l.currentToken.kind == tkVolatile:
    let modifierToken = p.l.currentToken
    consumeToken(p)
    case modifierToken.kind:
    of tkStar: derivedType = PointerTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    of tkBitwiseAnd:
      var isConstRef = false
      if p.l.currentToken.kind == tkConst:
        isConstRef = true
        consumeToken(p)
      derivedType = ReferenceTypeExpr(baseType: derivedType, isConst: isConstRef, line: modifierToken.line, col: modifierToken.col)
    of tkConst: derivedType = ConstQualifiedTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    of tkVolatile: derivedType = VolatileQualifiedTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    else: discard

  # Parse core declarator (name or nested declarator)
  if p.l.currentToken.kind == tkLParen:
    let peekKind = p.l.peekToken.kind
    # Heuristic for nested declarator vs function parameters
    if peekKind == tkStar or peekKind == tkBitwiseAnd or peekKind == tkLParen or peekKind == tkIdentifier:
      consumeToken(p) # Consume '(' for grouping
      var innerType: TypeExpr
      (name, innerType) = p.parseDeclarator(derivedType) # Recursive call
      if name == nil and innerType == nil:
        p.errors.add("Failed to parse inner declarator within parentheses")
        return (nil, nil)
      if p.l.currentToken.kind != tkRParen:
        p.errors.add("Expected ')' after parenthesized declarator, got " & $p.l.currentToken.kind)
        return (nil, nil)
      consumeToken(p) # Consume ')'
      derivedType = innerType
      # Update line/col info if possible
      if name != nil: line = name.line; col = name.col
      elif innerType != nil: line = innerType.line; col = innerType.col
    # else: Assume it's function parameters, handled by the suffix loop below.

  elif p.l.currentToken.kind == tkIdentifier:
    name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    line = name.line; col = name.col
    consumeToken(p)
  else:
    # Abstract declarator (no name)
    if derivedType != nil: line = derivedType.line; col = derivedType.col

  # Parse array/function suffixes
  while p.l.currentToken.kind == tkLBracket or p.l.currentToken.kind == tkLParen:
    if p.l.currentToken.kind == tkLBracket:
      let bracketLine = p.l.currentToken.line; let bracketCol = p.l.currentToken.col
      consumeToken(p) # Consume '['
      var sizeExpr: Expression = nil
      if p.l.currentToken.kind != tkRBracket:
        sizeExpr = p.parseExpression(Precedence.Lowest)
        if sizeExpr == nil:
          p.errors.add("Failed to parse array size expression")
          return (nil, nil)
      if p.l.currentToken.kind != tkRBracket:
         p.errors.add("Expected ']' after array dimension, got " & $p.l.currentToken.kind)
         return (nil, nil)
      consumeToken(p)
      derivedType = ArrayTypeExpr(elementType: derivedType, size: sizeExpr, line: bracketLine, col: bracketCol)
    elif p.l.currentToken.kind == tkLParen:
      # Function declarator suffix
      let parenLine = p.l.currentToken.line; let parenCol = p.l.currentToken.col
      let errorCountBefore = p.errors.len
      let funcParams = p.parseParameterList() # Consumes (...)

      if p.errors.len > errorCountBefore:
          return (nil, nil) # Propagate failure from parameter list parsing

      # Parse trailing qualifiers (const, volatile, &, &&, noexcept)
      var isConstFunc = false
      var isVolatileFunc = false
      var refQual = rqNone
      var isNoexceptFunc = false
      var qualifierParsed = true
      while qualifierParsed:
        qualifierParsed = false
        if p.l.currentToken.kind == tkConst: isConstFunc = true; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkVolatile: isVolatileFunc = true; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkBitwiseAnd:
          if refQual != rqNone: p.errors.add("Multiple ref-qualifiers (&, &&) specified")
          refQual = rqLValue; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkLogicalAnd: # Assuming && for rvalue ref
          if refQual != rqNone: p.errors.add("Multiple ref-qualifiers (&, &&) specified")
          refQual = rqRValue; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkNoexcept: isNoexceptFunc = true; consumeToken(p); qualifierParsed = true
        # else: No more qualifiers found

      derivedType = FunctionTypeExpr(
        returnType: derivedType, parameters: funcParams,
        isConst: isConstFunc, isVolatile: isVolatileFunc,
        refQualifier: refQual, isNoexcept: isNoexceptFunc,
        line: parenLine, col: parenCol
      )
      # After parsing function parameters and qualifiers, this part of the declarator is done.
      # No further suffixes possible after function declarator part.
      break # Exit the while loop for declarator suffixes

  if derivedType == nil: return (nil, nil)
  else: return (name, derivedType)

proc parseParameterList(p: var Parser): seq[Parameter] =
  result = @[]

  if p.l.currentToken.kind != tkLParen:
    p.errors.add("Expected '(' to start parameter list, got " & $p.l.currentToken.kind)
    return result
  consumeToken(p)

  if p.l.currentToken.kind == tkRParen:
    consumeToken(p)
    return result

  block parseFirstParam:
    let baseType = p.parseType()
    if baseType == nil:
      p.errors.add("Failed to parse type for first parameter")
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
      return result # Return empty on error
    let (paramName, paramFinalType) = p.parseDeclarator(baseType)
    if paramFinalType == nil:
        p.errors.add("Failed to parse declarator for first parameter")
        while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
        return result # Return empty on error
    # TODO: Parse default argument
    result.add(Parameter(paramType: paramFinalType, name: paramName, line: paramFinalType.line, col: paramFinalType.col))

  while p.l.currentToken.kind == tkComma:
    consumeToken(p)
    if p.l.currentToken.kind == tkRParen: break # Trailing comma
    block parseNextParam:
        let baseType = p.parseType()
        if baseType == nil:
          p.errors.add("Failed to parse type for subsequent parameter")
          while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
          return result # Return partial list
        let (paramName, paramFinalType) = p.parseDeclarator(baseType)
        if paramFinalType == nil:
            p.errors.add("Failed to parse declarator for subsequent parameter")
            while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
            return result # Return partial list
        # TODO: Parse default argument
        result.add(Parameter(paramType: paramFinalType, name: paramName, line: paramFinalType.line, col: paramFinalType.col))

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' or ',' in parameter list, got " & $p.l.currentToken.kind)
  else:
    consumeToken(p)

  return result

proc parseTemplateParameter(p: var Parser): TemplateParameter =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  var kind: TemplateParameterKind
  var name: Identifier = nil
  var paramType: TypeExpr = nil
  var defaultValue: Expression = nil
  var defaultType: TypeExpr = nil
  var templateParams: seq[TemplateParameter] = @[]

  if p.l.currentToken.kind == tkTypename or p.l.currentToken.kind == tkClass:
    kind = tpTypename
    consumeToken(p)
    if p.l.currentToken.kind == tkIdentifier:
      name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
      consumeToken(p)
    if p.l.currentToken.kind == tkAssign:
      consumeToken(p)
      defaultType = p.parseType()
      if defaultType == nil:
        p.errors.add("Failed to parse default type for template type parameter")
        return nil
  elif p.l.currentToken.kind == tkTemplate:
    kind = tpTemplate
    consumeToken(p)
    if p.l.currentToken.kind != tkLess:
      p.errors.add("Expected '<' after 'template' for template template parameter")
      return nil
    let nestedParamsOpt = p.parseTemplateParameterList()
    if nestedParamsOpt.isNone: return nil
    templateParams = nestedParamsOpt.get()
    if p.l.currentToken.kind != tkClass and p.l.currentToken.kind != tkTypename:
        p.errors.add("Expected 'class' or 'typename' after template template parameter list")
        return nil
    consumeToken(p)
    if p.l.currentToken.kind == tkIdentifier:
        name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
        consumeToken(p)
    else:
        p.errors.add("Expected identifier for template template parameter name")
  else:
    kind = tpType
    paramType = p.parseType()
    if paramType == nil:
      p.errors.add("Failed to parse type for non-type template parameter")
      return nil
    if p.l.currentToken.kind == tkIdentifier:
      name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
      consumeToken(p)
    if p.l.currentToken.kind == tkAssign:
      consumeToken(p)
      defaultValue = p.parseExpression(Precedence.Assign)
      if defaultValue == nil:
        p.errors.add("Failed to parse default value for non-type template parameter")
        return nil

  result = TemplateParameter(
    kind: kind, name: name, paramType: paramType,
    defaultValue: defaultValue, defaultType: defaultType,
    templateParams: templateParams,
    line: line, col: col
  )

proc parseTemplateParameterList(p: var Parser): Option[seq[TemplateParameter]] =
  var resultSeq: seq[TemplateParameter] = @[]
  if p.l.currentToken.kind != tkLess:
    p.errors.add("Expected '<' to start template parameter list")
    return none(seq[TemplateParameter])
  consumeToken(p)

  if p.l.currentToken.kind == tkGreater:
    consumeToken(p)
    return some(resultSeq)

  block parseFirstParam:
    let param = p.parseTemplateParameter()
    if param == nil: return none(seq[TemplateParameter])
    resultSeq.add(param)

  while p.l.currentToken.kind == tkComma:
    consumeToken(p)
    if p.l.currentToken.kind == tkGreater: break
    block parseNextParam:
      let param = p.parseTemplateParameter()
      if param == nil: return none(seq[TemplateParameter])
      resultSeq.add(param)

  if p.l.currentToken.kind != tkGreater:
    p.errors.add("Expected '>' or ',' in template parameter list, got " & $p.l.currentToken.kind)
    return none(seq[TemplateParameter])
  consumeToken(p)
  return some(resultSeq)

proc parseEnumerator(p: var Parser): Enumerator =
  result = nil # ProveInit
  if p.l.currentToken.kind != tkIdentifier:
    p.errors.add("Expected enumerator name, got " & $p.l.currentToken.kind)
    return nil
  let name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
  consumeToken(p)

  var value: Expression = nil
  if p.l.currentToken.kind == tkAssign:
    consumeToken(p) # Consume '='
    value = p.parseExpression(Precedence.Assign) # Parse constant expression
    if value == nil:
      p.errors.add("Failed to parse value for enumerator " & name.name)
      return nil # Return nil if value parsing fails

  result = Enumerator(name: name, value: value, line: name.line, col: name.col)

# --- Statement Parsing Implementations ---

proc parseBreakStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'break'
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after 'break', got " & $p.l.currentToken.kind)
    return nil # Indicate failure
  consumeToken(p) # Consume ';'
  result = BreakStatement(line: line, col: col)

proc parseContinueStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'continue'
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after 'continue', got " & $p.l.currentToken.kind)
    return nil # Indicate failure
  consumeToken(p) # Consume ';'
  result = ContinueStatement(line: line, col: col)

proc parseCaseStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  var value: Expression = nil
  let isDefault = p.l.currentToken.kind == tkDefault
  consumeToken(p) # Consume 'case' or 'default'

  if not isDefault:
    value = p.parseExpression(Precedence.Lowest) # Parse the constant expression
    if value == nil:
      p.errors.add("Expected constant expression after 'case'")
      return nil
  # else: value remains nil for default

  if p.l.currentToken.kind != tkColon:
    p.errors.add("Expected ':' after case/default value")
    return nil
  consumeToken(p) # Consume ':'

  # CaseStatement node itself doesn't contain the body
  result = CaseStatement(value: value, line: line, col: col)

proc parseSwitchStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'switch'

  if not p.expectPeek(tkLParen): return nil
  # expectPeek consumed '('
  let controlExpr = p.parseExpression(Precedence.Lowest)
  if controlExpr == nil:
    p.errors.add("Failed to parse control expression in switch statement")
    return nil
  if p.l.currentToken.kind != tkRParen: # Check current token
    p.errors.add("Expected ')' after switch control expression, got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    return nil
  consumeToken(p) # Consume ')'

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for switch statement body")
    return nil

  let bodyStmt = p.parseBlockStatement()
  if bodyStmt == nil or not (bodyStmt of BlockStatement):
    p.errors.add("Failed to parse switch statement body")
    return nil

  result = SwitchStatement(controlExpr: controlExpr, body: BlockStatement(bodyStmt), line: line, col: col)

proc parseEnumDefinition(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'enum'

  var isClass = false
  if p.l.currentToken.kind == tkClass or p.l.currentToken.kind == tkStruct:
    isClass = true
    consumeToken(p)

  var enumName: Identifier = nil
  if p.l.currentToken.kind == tkIdentifier:
    enumName = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)

  var baseType: TypeExpr = nil
  if p.l.currentToken.kind == tkColon:
    consumeToken(p) # Consume ':'
    baseType = p.parseType()
    if baseType == nil:
      p.errors.add("Failed to parse underlying type for enum")
      return nil

  if p.l.currentToken.kind != tkLBrace:
    if enumName != nil and p.l.currentToken.kind == tkSemicolon:
        consumeToken(p) # Consume ';'
        p.errors.add("Enum forward declaration parsed, but no specific AST node exists yet.")
        return nil
    else:
        p.errors.add("Expected '{' for enum body or ';' for forward declaration, got " & $p.l.currentToken.kind)
        return nil
  consumeToken(p) # Consume '{'

  var enumerators: seq[Enumerator] = @[]
  if p.l.currentToken.kind != tkRBrace: # Handle empty enum
    let firstEnumerator = p.parseEnumerator()
    if firstEnumerator == nil: return nil # Error already added
    enumerators.add(firstEnumerator)

    while p.l.currentToken.kind == tkComma:
      consumeToken(p) # Consume ','
      if p.l.currentToken.kind == tkRBrace: break # Allow trailing comma
      let nextEnumerator = p.parseEnumerator()
      if nextEnumerator == nil: return nil # Error already added
      enumerators.add(nextEnumerator)

  if p.l.currentToken.kind != tkRBrace:
    p.errors.add("Expected '}' or ',' in enumerator list, got " & $p.l.currentToken.kind)
    return nil
  consumeToken(p) # Consume '}'

  if p.l.currentToken.kind == tkSemicolon: consumeToken(p)

  result = EnumDefinition(name: enumName, isClass: isClass, baseType: baseType, enumerators: enumerators, line: line, col: col)

proc parseTypeAliasDeclaration(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let keyword = p.l.currentToken.kind # tkTypedef or tkUsing
  consumeToken(p)

  if keyword == tkTypedef:
    let originalBaseType = p.parseType()
    if originalBaseType == nil:
      p.errors.add("Failed to parse original type in typedef declaration")
      return nil
    let (newName, finalOriginalType) = p.parseDeclarator(originalBaseType)
    if finalOriginalType == nil:
      p.errors.add("Failed to parse declarator (including new name) in typedef declaration")
      return nil
    if newName == nil:
      p.errors.add("Typedef declaration requires a new name")
      return nil

    if p.l.currentToken.kind != tkSemicolon:
      p.errors.add("Expected ';' after typedef declaration")
      return nil
    consumeToken(p) # Consume ';'
    result = TypeAliasDeclaration(name: newName, aliasedType: finalOriginalType, line: line, col: col)

  elif keyword == tkUsing:
    if p.l.currentToken.kind != tkIdentifier:
      p.errors.add("Expected new name identifier after 'using'")
      return nil
    let newName = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)

    if p.l.currentToken.kind != tkAssign:
      p.errors.add("Expected '=' after new name in using declaration")
      return nil
    consumeToken(p) # Consume '='

    let aliasedBaseType = p.parseType()
    if aliasedBaseType == nil:
        p.errors.add("Failed to parse aliased type after '=' in using declaration")
        return nil
    let (abstractName, finalAliasedType) = p.parseDeclarator(aliasedBaseType)
    if finalAliasedType == nil:
        p.errors.add("Failed to parse declarator part of aliased type in using declaration")
        return nil
    if abstractName != nil:
        p.errors.add("Unexpected name found in aliased type declarator for 'using' alias")

    if p.l.currentToken.kind != tkSemicolon:
      p.errors.add("Expected ';' after using declaration")
      return nil
    consumeToken(p) # Consume ';'
    result = TypeAliasDeclaration(name: newName, aliasedType: finalAliasedType, line: line, col: col)
  else:
    p.errors.add("Internal parser error: parseTypeAliasDeclaration called with unexpected token " & $keyword)
    return nil

proc parseBlockStatement(p: var Parser): Statement =
  let blck = BlockStatement(line: p.l.currentToken.line, col: p.l.currentToken.col, statements: @[])
  consumeToken(p) # Consume '{'

  while p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
    let stmt = p.parseStatement()
    if stmt != nil:
      blck.statements.add(stmt)
    else:
      if p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
        var foundErrorForToken = false
        if p.errors.len > 0:
          if strutils.contains(p.errors[^1], "at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col):
            foundErrorForToken = true
        let skippedToken = p.l.currentToken
        if not foundErrorForToken:
          p.errors.add("Skipping token inside block after failed statement parse: " & $skippedToken.kind & " at " & $skippedToken.line & ":" & $skippedToken.col)
        consumeToken(p)

  if p.l.currentToken.kind != tkRBrace:
    p.errors.add("Expected '}' to close block, got " & $p.l.currentToken.kind & " instead.")
  else:
    consumeToken(p) # Consume '}'

  return blck

proc parseExpressionStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line
  let col = p.l.currentToken.col
  let expr = p.parseExpression(Precedence.Lowest)
  if expr == nil:
      # Error likely already reported by parseExpression
      # Attempt recovery by skipping until semicolon
      while p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile and p.l.currentToken.kind != tkRBrace:
          consumeToken(p)
      if p.l.currentToken.kind == tkSemicolon: consumeToken(p)
      return nil

  let stmt = ExpressionStatement(expression: expr, line: line, col: col)

  if p.l.currentToken.kind == tkSemicolon:
    consumeToken(p)
    result = stmt # Return the valid statement
  else:
    p.errors.add("Missing semicolon after expression statement at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    # Return nil on missing semicolon for stricter parsing.
    result = nil

proc parseDeclarationStatement(p: var Parser,
                               parsedType: TypeExpr = nil,
                               requireSemicolon: bool = true,
                               attributes: seq[AttributeGroup] = @[]): Statement =
  let startTokenKind = p.l.currentToken.kind
  var line = p.l.currentToken.line
  var col = p.l.currentToken.col
  result = nil # Initialize result for ProveInit

  var localAttributes = attributes
  line = p.l.currentToken.line; col = p.l.currentToken.col

  var baseType: TypeExpr = parsedType
  if baseType == nil:
    line = p.l.currentToken.line; col = p.l.currentToken.col
    baseType = p.parseType()
    if baseType == nil:
        # This means the sequence didn't start with a recognizable type specifier.
        # It cannot be a declaration statement according to our current logic.
        # Return nil to let parseStatement try parsing as an expression statement.
        return nil
  else:
    line = baseType.line; col = baseType.col

  let (name, finalType) = p.parseDeclarator(baseType)
  if finalType == nil:
      p.errors.add("Failed to parse declarator after type specifiers")
      return nil

  # Check if the token following the declarator indicates a function body
  if p.l.currentToken.kind == tkLBrace: # Function Definition
      if name == nil: p.errors.add("Function definition requires a name."); return nil
      if not (finalType of FunctionTypeExpr): p.errors.add("Expected function declarator '(...)' for function definition."); return nil
      let funcType = FunctionTypeExpr(finalType)
      let bodyStmt = p.parseBlockStatement()
      if bodyStmt == nil or not (bodyStmt of BlockStatement): p.errors.add("Failed to parse function body for " & name.name); return nil
      result = FunctionDefinition(
          returnType: funcType.returnType, name: name, parameters: funcType.parameters,
          body: BlockStatement(bodyStmt), line: line, col: col, attributes: localAttributes # Assign attributes
      )
      # Function definitions don't require a semicolon after the body. Return immediately.
      return result # Explicitly return here

  # If not a function definition, assume it's a variable declaration (or error)
  var initializer: Expression = nil # Initialize here

  # Check for initializer ONLY if a name was parsed (usually required for var decl)
  if name != nil: # Only parse initializer if we have a variable name
      if p.l.currentToken.kind == tkAssign:
        consumeToken(p) # Consume '='
        initializer = p.parseExpression(Precedence.Assign) # Parse initializer expression
        if initializer == nil: p.errors.add("Failed to parse initializer expression after '=' for " & name.name)
      elif p.l.currentToken.kind == tkLBrace: # Brace initialization {}
        initializer = p.parseInitializerListExpression()
        if initializer == nil: p.errors.add("Failed to parse initializer list '{...}' for " & name.name)
      elif p.l.currentToken.kind == tkLParen: # Constructor-style init T var(args...);
        let parenLine = p.l.currentToken.line; let parenCol = p.l.currentToken.col
        consumeToken(p) # Consume '('
        var args: seq[Expression] = @[]
        if p.l.currentToken.kind != tkRParen:
            let firstArg = p.parseExpression(Precedence.Assign)
            if firstArg == nil: return nil # Fail hard if arg parsing fails
            args.add(firstArg)
            while p.l.currentToken.kind == tkComma:
                consumeToken(p) # Consume ','
                let nextArg = p.parseExpression(Precedence.Assign)
                if nextArg == nil: return nil # Fail hard
                args.add(nextArg)
        if p.l.currentToken.kind != tkRParen: p.errors.add("Expected ')' after constructor arguments for " & name.name); return nil
        consumeToken(p) # Consume ')'
        initializer = InitializerListExpr(values: args, line: parenLine, col: parenCol)

  elif name == nil and p.l.currentToken.kind != tkLBrace: # Check again, name is nil and no function body
      p.errors.add("Abstract declarator found where statement was expected.")
      return nil

  # Check for semicolon if required
  if requireSemicolon:
    if p.l.currentToken.kind != tkSemicolon:
       let varName = if name != nil: name.name else: "<abstract>"
       p.errors.add("Missing semicolon after variable declaration for " & varName & ", got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
       return nil # Fail if semicolon is missing when required
    consumeToken(p) # Consume ';'

  # Create the node only if a name was present (standard variable declaration)
  if name != nil:
      result = VariableDeclaration(
        varType: finalType, name: name, initializer: initializer,
        line: line, col: col, attributes: localAttributes # Assign attributes
      )
  else:
      # If name is nil here, it means we parsed an abstract declarator followed by a semicolon (or end of input).
      # This isn't a valid standalone statement.
      result = nil # Ensure result is nil if no valid statement was formed

proc parseIfStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'if'

  if not p.expectPeek(tkLParen): return nil
  # expectPeek consumed '('

  let condition = p.parseExpression(Precedence.Lowest)
  if condition == nil:
    p.errors.add("Failed to parse condition in if statement")
    while p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
    return nil

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after if condition, got " & $p.l.currentToken.kind)
    while p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
    return nil
  consumeToken(p) # Consume ')'

  let thenBranch = p.parseStatement()
  if thenBranch == nil:
    p.errors.add("Failed to parse 'then' branch of if statement")
    return nil

  var elseBranch: Statement = nil
  if p.l.currentToken.kind == tkElse:
    consumeToken(p) # Consume 'else'
    elseBranch = p.parseStatement()
    if elseBranch == nil:
      p.errors.add("Failed to parse 'else' branch of if statement")

  result = IfStatement(condition: condition, thenBranch: thenBranch, elseBranch: elseBranch, line: line, col: col)

proc parseWhileStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'while'

  if not p.expectPeek(tkLParen): return nil
  # expectPeek consumed '('

  let condition = p.parseExpression(Precedence.Lowest)
  if condition == nil:
    p.errors.add("Failed to parse condition in while statement")
    while p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
    return nil

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after while condition, got " & $p.l.currentToken.kind)
    while p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)
    return nil
  consumeToken(p) # Consume ')'

  let body = p.parseStatement()
  if body == nil:
    p.errors.add("Failed to parse body of while statement")
    return nil

  result = WhileStatement(condition: condition, body: body, line: line, col: col)

proc parseReturnStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'return'
  var returnValue: Expression = nil
  let errorCountBefore = p.errors.len

  if p.l.currentToken.kind != tkSemicolon:
    returnValue = p.parseExpression(Precedence.Lowest)
    if returnValue == nil and p.errors.len > errorCountBefore:
        p.errors.add("Failed to parse return value expression. Skipping to find semicolon.")
        while p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile: consumeToken(p)

  if p.l.currentToken.kind != tkSemicolon:
    # Corrected error message to use currentToken's line/col
    p.errors.add("Expected ';' after return statement, got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    return nil # Indicate failure
  consumeToken(p) # Consume ';'
  result = ReturnStatement(returnValue: returnValue, line: line, col: col)

proc parseForStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'for'

  if not p.expectPeek(tkLParen): return nil
  # expectPeek consumed '('

  # Parse initializer (can be declaration or expression)
  var initializer: Statement = nil
  if p.l.currentToken.kind != tkSemicolon:
    # Try parsing as declaration first
    initializer = p.parseDeclarationStatement(requireSemicolon = false)
    if initializer == nil:
      # If declaration failed, try parsing as expression statement
      initializer = p.parseExpressionStatement()
      if initializer == nil:
        p.errors.add("Failed to parse initializer in for loop")
        return nil
      # Expression statement parsing consumes the semicolon, but we need it here
      # This logic needs refinement. For now, assume parseExpressionStatement failed if it wasn't a declaration.
      # Re-evaluate: parseExpressionStatement should NOT consume the semicolon if used here.
      # Let's simplify: parseExpression directly.
      let initExpr = p.parseExpression(Precedence.Lowest)
      if initExpr == nil:
          p.errors.add("Failed to parse initializer expression in for loop")
          return nil
      initializer = ExpressionStatement(expression: initExpr, line: initExpr.line, col: initExpr.col)

  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after for loop initializer")
    return nil
  consumeToken(p) # Consume ';'

  # Parse condition
  var condition: Expression = nil
  if p.l.currentToken.kind != tkSemicolon:
    condition = p.parseExpression(Precedence.Lowest)
    if condition == nil:
      p.errors.add("Failed to parse condition in for loop")
      return nil

  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after for loop condition")
    return nil
  consumeToken(p) # Consume ';'

  # Parse increment
  var increment: Expression = nil
  if p.l.currentToken.kind != tkRParen:
    increment = p.parseExpression(Precedence.Lowest)
    if increment == nil:
      p.errors.add("Failed to parse increment expression in for loop")
      return nil

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after for loop clauses")
    return nil
  consumeToken(p) # Consume ')'

  # Parse body
  let body = p.parseStatement()
  if body == nil:
    p.errors.add("Failed to parse body of for loop")
    return nil

  result = ForStatement(initializer: initializer, condition: condition, increment: increment, body: body, line: line, col: col)

proc parseTryStatement(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'try'

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for try block")
    return nil
  let tryBlock = p.parseBlockStatement()
  if tryBlock == nil or not (tryBlock of BlockStatement):
    p.errors.add("Failed to parse try block")
    return nil

  var catchClauses: seq[CatchClause] = @[]
  while p.l.currentToken.kind == tkCatch:
    let catchClause = p.parseCatchClause()
    if catchClause == nil:
      p.errors.add("Failed to parse catch clause")
      # Attempt recovery? For now, stop parsing catches.
      break
    catchClauses.add(catchClause)

  if catchClauses.len == 0:
    p.errors.add("Expected at least one catch clause after try block")
    return nil

  result = TryStatement(tryBlock: BlockStatement(tryBlock), catchClauses: catchClauses, line: line, col: col)

proc parseCatchClause(p: var Parser): CatchClause =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'catch'

  if not p.expectPeek(tkLParen): return nil
  # expectPeek consumed '('

  var exceptionType: TypeExpr = nil
  var exceptionName: Identifier = nil
  var isEllipsis = false

  if p.l.currentToken.kind == tkEllipsis:
    isEllipsis = true
    consumeToken(p)
  else:
    exceptionType = p.parseType()
    if exceptionType == nil:
      p.errors.add("Failed to parse exception type in catch clause")
      return nil
    # Parse declarator (might be abstract or include a name)
    let (name, finalType) = p.parseDeclarator(exceptionType)
    if finalType == nil:
        p.errors.add("Failed to parse exception declarator in catch clause")
        return nil
    exceptionType = finalType # Update type with pointers/refs etc.
    exceptionName = name # Can be nil for abstract declarator

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after catch parameter, got " & $p.l.currentToken.kind)
    return nil
  consumeToken(p) # Consume ')'

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for catch block")
    return nil
  let catchBlock = p.parseBlockStatement()
  if catchBlock == nil or not (catchBlock of BlockStatement):
    p.errors.add("Failed to parse catch block")
    return nil

  result = CatchClause(
    exceptionType: exceptionType, exceptionName: exceptionName,
    isEllipsis: isEllipsis, body: BlockStatement(catchBlock),
    line: line, col: col
  )

proc parseTemplateDeclaration(p: var Parser): Statement =
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'template'

  let templateParamsOpt = p.parseTemplateParameterList()
  if templateParamsOpt.isNone:
    p.errors.add("Failed to parse template parameter list")
    return nil
  let templateParams = templateParamsOpt.get()

  # Parse the actual declaration/definition that is templated
  # This could be a class, function, variable, alias...
  # We need parseStatement to handle this.
  let declaration = p.parseStatement()
  if declaration == nil:
    p.errors.add("Failed to parse declaration/definition after template parameter list")
    return nil
# Combined C++ Parser Implementation for nifcpp

import std/sequtils
import std/tables
import std/strutils
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
    errors*: seq[string]
    prefixParseFns: Table[TokenType, proc (p: var Parser): Expression]
    infixParseFns: Table[TokenType, proc (p: var Parser, left: Expression): Expression]

# --- Forward Declarations ---
proc parseExpression*(p: var Parser, precedence: Precedence): Expression # Exported
proc parseStatement(p: var Parser): Statement
proc parseBlockStatement(p: var Parser): Statement
proc parseType(p: var Parser): TypeExpr
proc parseDeclarator(p: var Parser, baseType: TypeExpr): (Identifier, TypeExpr)
proc parseParameterList(p: var Parser): seq[Parameter]
proc parseTemplateParameterList(p: var Parser): Option[seq[TemplateParameter]]
proc parseAttributeSpecifierSeq*(p: var Parser): seq[AttributeGroup] # Exported
proc parseEnumerator(p: var Parser): Enumerator
proc parseCatchClause(p: var Parser): CatchClause
proc parseDeclarationStatement(p: var Parser,
                               parsedType: TypeExpr = nil,
                               requireSemicolon: bool = true,
                               attributes: seq[AttributeGroup] = @[]): Statement
proc parseExpressionStatement(p: var Parser): Statement
proc parseIfStatement(p: var Parser): Statement
proc parseWhileStatement(p: var Parser): Statement
proc parseForStatement(p: var Parser): Statement
proc parseReturnStatement(p: var Parser): Statement
proc parseTryStatement(p: var Parser): Statement
proc parseTemplateDeclaration(p: var Parser): Statement
proc parseNamespaceDefinition(p: var Parser): Statement
proc parseClassDefinition(p: var Parser): Statement
proc parseBreakStatement(p: var Parser): Statement
proc parseContinueStatement(p: var Parser): Statement
proc parseSwitchStatement(p: var Parser): Statement
proc parseCaseStatement(p: var Parser): Statement
proc parseEnumDefinition(p: var Parser): Statement
proc parseTypeAliasDeclaration(p: var Parser): Statement
proc parseDecltypeSpecifier(p: var Parser): TypeExpr
proc parseTranslationUnit*(p: var Parser): TranslationUnit # Exported

# --- Helper Procs ---
proc consumeToken*(p: var Parser) =
  nextToken(p.l)

proc peekError*(p: var Parser, expected: TokenType) =
  let msg = "Expected next token to be " & $expected & ", got " & $p.l.peekToken.kind & " instead at " & $p.l.peekToken.line & ":" & $p.l.peekToken.col & "."
  p.errors.add(msg)

proc expectPeek*(p: var Parser, expected: TokenType): bool =
  if p.l.peekToken.kind == expected:
    consumeToken(p)
    return true
  else:
    p.peekError(expected)
    return false # Explicitly return false to satisfy ProveInit

# --- Precedence Helper Procs ---
const precedences: Table[TokenType, Precedence] = {
  tkAssign: Precedence.Assign, tkPlusAssign: Precedence.Assign, tkMinusAssign: Precedence.Assign,
  tkMulAssign: Precedence.Assign, tkDivAssign: Precedence.Assign, tkModAssign: Precedence.Assign,
  tkAndAssign: Precedence.Assign, tkOrAssign: Precedence.Assign, tkXorAssign: Precedence.Assign,
  tkLShiftAssign: Precedence.Assign, tkRShiftAssign: Precedence.Assign,
  tkQuestion: Precedence.Conditional, # Ternary
  tkLogicalOr: Precedence.LogicalOr,
  tkLogicalAnd: Precedence.LogicalAnd,
  tkBitwiseOr: Precedence.BitwiseOr,
  tkBitwiseXor: Precedence.BitwiseXor,
  tkBitwiseAnd: Precedence.BitwiseAnd, # Also used for address-of (Prefix)
  tkEqual: Precedence.Equality, tkNotEqual: Precedence.Equality,
  tkLess: Precedence.Relational, tkGreater: Precedence.Relational,
  tkLessEqual: Precedence.Relational, tkGreaterEqual: Precedence.Relational,
  tkLeftShift: Precedence.Shift, tkRightShift: Precedence.Shift,
  tkPlus: Precedence.Additive, tkMinus: Precedence.Additive,
  tkStar: Precedence.Multiplicative, # Also used for dereference (Prefix)
  tkSlash: Precedence.Multiplicative, tkPercent: Precedence.Multiplicative,
  # Prefix operators have their own precedence level handled by parseExpression
  tkIncrement: Precedence.Postfix, # Also used for prefix
  tkDecrement: Precedence.Postfix, # Also used for prefix
  tkLParen: Precedence.Call, # Also used for grouping (Lowest)
  tkLBracket: Precedence.Index, # Also used for attributes/lambda start (Lowest?)
  tkDot: Precedence.Member, tkArrow: Precedence.Member
}.toTable

proc peekPrecedence*(p: Parser): Precedence = # Changed to non-var Parser
  result = precedences.getOrDefault(p.l.peekToken.kind, Precedence.Lowest)

proc currentPrecedence*(p: Parser): Precedence = # Changed to non-var Parser
  result = precedences.getOrDefault(p.l.currentToken.kind, Precedence.Lowest)

# --- Constructor ---
proc newParser*(l: Lexer): Parser = # Exported
  new(result)
  result.l = l
  result.errors = @[]
  result.prefixParseFns = initTable[TokenType, proc (p: var Parser): Expression]()
  result.infixParseFns = initTable[TokenType, proc (p: var Parser, left: Expression): Expression]()
  # Registration happens in parser.nim

# --- Registration Procs ---
proc registerPrefix*(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser): Expression) = # Exported
  p.prefixParseFns[tokenType] = fn

proc registerInfix*(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser, left: Expression): Expression) = # Exported
  p.infixParseFns[tokenType] = fn

# --- Attribute Implementations ---

proc parseAttributeArgument(p: var Parser): AttributeArgument =
  result = nil # ProveInit
  let line = p.
