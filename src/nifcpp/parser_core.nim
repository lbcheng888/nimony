 # Core definitions for the C++ Parser

import std/sequtils
import std/tables
import std/strutils
import std/options
import ./lexer # Reverted import path
import ./ast   # Reverted import path

# --- Parser Type Definition ---
type
  Parser* = ref object # Forward declaration needed for procs below
    l*: Lexer
    errors*: seq[string]
    prefixParseFns: Table[TokenType, proc (p: var Parser): Expression]
    infixParseFns: Table[TokenType, proc (p: var Parser, left: Expression): Expression]

# --- Helper Procs ---
proc consumeToken*(p: var Parser) = # Exported
  nextToken(p.l)

proc peekError*(p: var Parser, expected: TokenType) = # Exported
  let msg = "Expected next token to be " & $expected & ", got " & $p.l.peekToken.kind & " instead at " & $p.l.peekToken.line & ":" & $p.l.peekToken.col & "."
  p.errors.add(msg)

proc expectPeek*(p: var Parser, expected: TokenType): bool = # Exported
  if p.l.peekToken.kind == expected:
    consumeToken(p) # Call the (now exported) consumeToken
    return true
  else:
    p.peekError(expected) # Call the (now exported) peekError
    return false

# --- Precedence Levels ---
type Precedence* = enum
  Lowest, Assign, LogicalOr, LogicalAnd, BitwiseOr, BitwiseXor, BitwiseAnd,
  Equals, LessGreater, Shift, Sum, Product, Prefix, Call, Index, Member, Postfix

const precedences*: Table[TokenType, Precedence] = [
  (tkAssign, Precedence.Assign), (tkPlusAssign, Precedence.Assign), (tkMinusAssign, Precedence.Assign), (tkMulAssign, Precedence.Assign),
  (tkDivAssign, Precedence.Assign), (tkModAssign, Precedence.Assign), (tkAndAssign, Precedence.Assign), (tkOrAssign, Precedence.Assign),
  (tkXorAssign, Precedence.Assign), (tkLShiftAssign, Precedence.Assign), (tkRShiftAssign, Precedence.Assign),
  (tkLogicalOr, Precedence.LogicalOr),
  (tkLogicalAnd, Precedence.LogicalAnd),
  (tkBitwiseOr, Precedence.BitwiseOr),
  (tkBitwiseXor, Precedence.BitwiseXor),
  (tkBitwiseAnd, Precedence.BitwiseAnd),
  (tkEqual, Precedence.Equals), (tkNotEqual, Precedence.Equals),
  (tkLess, Precedence.LessGreater), (tkLessEqual, Precedence.LessGreater), (tkGreater, Precedence.LessGreater), (tkGreaterEqual, Precedence.LessGreater),
  (tkLeftShift, Precedence.Shift), (tkRightShift, Precedence.Shift),
  (tkPlus, Precedence.Sum), (tkMinus, Precedence.Sum),
  (tkStar, Precedence.Product), (tkSlash, Precedence.Product), (tkPercent, Precedence.Product),
  (tkLParen, Precedence.Call),
  (tkLBracket, Precedence.Index),
  (tkDot, Precedence.Member), (tkArrow, Precedence.Member),
  (tkIncrement, Precedence.Postfix),
  (tkDecrement, Precedence.Postfix)
].toTable()

proc peekPrecedence*(p: Parser): Precedence = # Exported
  result = precedences.getOrDefault(p.l.peekToken.kind, Precedence.Lowest)

proc currentPrecedence*(p: Parser): Precedence = # Exported
  result = precedences.getOrDefault(p.l.currentToken.kind, Precedence.Lowest)

# --- Forward declarations for ALL parsing functions ---
# Expressions
# Removed forward declaration for parseExpression (implemented in parser_expressions.nim)
proc parseIdentifier*(p: var Parser): Expression # Exported
# Removed forward declaration for parseIntegerLiteral
# Removed forward declaration for parseFloatLiteral
# Removed forward declaration for parseStringLiteral
proc parseCharLiteral*(p: var Parser): Expression # Exported
proc parsePrefixExpression*(p: var Parser): Expression # Exported
proc parseInfixExpression*(p: var Parser, left: Expression): Expression # Exported
# Removed forward declaration for parseGroupedExpression
proc parseInitializerListExpression*(p: var Parser): Expression # Exported
# Removed forward declaration for parseCallExpression
# Removed forward declaration for parseMemberAccessExpression
proc parseIndexExpression*(p: var Parser, left: Expression): Expression # Exported
proc parsePostfixExpression*(p: var Parser, left: Expression): Expression # Exported
proc parseThrowExpression*(p: var Parser): Expression # Exported
proc parseLambdaExpression*(p: var Parser): Expression # Exported

# Statements
proc parseStatement*(p: var Parser): Statement # Exported
# Removed forward declaration for parseBlockStatement
# Removed forward declaration for parseExpressionStatement
# Removed forward declaration for parseDeclarationStatement
proc parseReturnStatement*(p: var Parser): Statement # Exported
# Removed forward declaration for parseIfStatement
# Removed forward declaration for parseWhileStatement
# Removed forward declaration for parseForStatement
# Removed forward declaration for parseBreakStatement
# Removed forward declaration for parseContinueStatement
proc parseSwitchStatement*(p: var Parser): Statement # Exported
proc parseCaseStatement*(p: var Parser): Statement # Exported
# Removed forward declaration for parseEnumDefinition
proc parseTypeAliasDeclaration*(p: var Parser): Statement # Exported
# Removed forward declaration for parseTryStatement - Moved to parser_statements.nim
# Removed forward declaration for parseTemplateDeclaration
# Removed forward declaration for parseNamespaceDefinition (implemented in parser_statements.nim)
# Removed forward declaration for parseClassDefinition

# Types, Declarators, Parameters, etc.
# Removed forward declaration for parseType (implemented in parser_types.nim)
proc parseDeclarator*(p: var Parser, baseType: TypeExpr): (Identifier, TypeExpr) # Exported
proc parseParameterList*(p: var Parser): seq[Parameter] # Exported
# Removed forward declaration for parseCatchClause
# Removed forward declaration for parseTemplateParameter (implemented in parser_types.nim)
proc parseTemplateParameterList*(p: var Parser): Option[seq[TemplateParameter]] # Exported
# Removed forward declaration for parseDecltypeSpecifier
# Removed forward declaration for parseAttributeArgument (implemented in parser_types.nim)
proc parseAttribute*(p: var Parser): Attribute # Exported - Re-added forward declaration
# Removed forward declaration for parseAttributeGroup (implemented in parser_types.nim)
proc parseAttributeSpecifierSeq*(p: var Parser): seq[AttributeGroup] # Exported
# Removed forward declaration for parseEnumerator

# --- Implementations moved to other files ---

# --- Registration Procs ---
proc registerPrefix(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser): Expression) =
  p.prefixParseFns[tokenType] = fn

proc registerInfix(p: var Parser, tokenType: TokenType, fn: proc (p: var Parser, left: Expression): Expression) =
  p.infixParseFns[tokenType] = fn

# --- Constructor ---
proc newParser*(l: Lexer): Parser =
  new(result)
  result.l = l
  result.errors = @[]
  result.prefixParseFns = initTable[TokenType, proc (p: var Parser): Expression]()
  result.infixParseFns = initTable[TokenType, proc (p: var Parser, left: Expression): Expression]()

  # Registration moved to parser.nim to avoid circular dependencies
  # and ensure all functions are defined before registration.
