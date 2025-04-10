# C++ Type Parsing Implementation for nifcpp

import std/strutils
import std/options
import ./lexer
import ./ast
import ./parser_core
import ./parser_expressions # Needed for parsing default arguments/array sizes and resolving the circular dependency

# --- Forward Declarations ---
# proc parseExpression*(p: var Parser, precedence: Precedence): Expression # Now imported from parser_expressions

# --- Type Parsing Implementations ---

proc parseBaseType*(p: var Parser): TypeExpr =
  # Parses simple type names like int, float, MyClass, etc.
  # Handles potential scope resolution like std::vector later
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col

  # TODO: Handle scope resolution (::)
  if p.l.currentToken.kind == tkIdentifier:
    let typeName = p.l.currentToken.literal
    consumeToken(p) # Consume the identifier
    result = BaseTypeExpr(name: typeName, line: line, col: col)
  elif p.l.currentToken.kind == tkVoid: # Handle 'void' keyword
    consumeToken(p)
    result = BaseTypeExpr(name: "void", line: line, col: col)
  elif p.l.currentToken.kind == tkInt: # Handle 'int' keyword etc.
    consumeToken(p)
    result = BaseTypeExpr(name: "int", line: line, col: col)
  elif p.l.currentToken.kind == tkFloat:
    consumeToken(p)
    result = BaseTypeExpr(name: "float", line: line, col: col)
  elif p.l.currentToken.kind == tkDouble:
    consumeToken(p)
    result = BaseTypeExpr(name: "double", line: line, col: col)
  elif p.l.currentToken.kind == tkChar:
    consumeToken(p)
    result = BaseTypeExpr(name: "char", line: line, col: col)
  elif p.l.currentToken.kind == tkBool:
    consumeToken(p)
    result = BaseTypeExpr(name: "bool", line: line, col: col)
  # Add other built-in types (short, long, unsigned, etc.)
  else:
    p.errors.add("Expected type name (identifier or keyword), got " & $p.l.currentToken.kind & " at " & $line & ":" & $col)

proc parseType*(p: var Parser): TypeExpr = # Exported
  # Parses a potentially complex type, including const/volatile, pointers, references, arrays, functions
  result = nil # ProveInit
  var isConst = false
  var isVolatile = false

  # 1. Handle leading const/volatile qualifiers
  if p.l.currentToken.kind == tkConst:
    isConst = true
    consumeToken(p)
  if p.l.currentToken.kind == tkVolatile:
    isVolatile = true
    consumeToken(p)
  # Re-check for const after volatile (e.g., volatile const int)
  if p.l.currentToken.kind == tkConst and not isConst:
      isConst = true
      consumeToken(p)

  # 2. Parse the base type (identifier or keyword)
  var baseType = p.parseBaseType()
  if baseType == nil:
    p.errors.add("Failed to parse base type specifier at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    return nil

  # Apply leading const/volatile if present
  if isConst:
    baseType = ConstQualifiedTypeExpr(baseType: baseType, line: baseType.line, col: baseType.col) # Use baseType location
    isConst = false # Mark as applied
  if isVolatile:
    baseType = VolatileQualifiedTypeExpr(baseType: baseType, line: baseType.line, col: baseType.col) # Use baseType location
    isVolatile = false # Mark as applied

  # 3. Handle trailing modifiers (*, &, [], const, volatile) in a loop
  while true:
    case p.l.currentToken.kind
    of tkStar: # Pointer
      consumeToken(p)
      baseType = PointerTypeExpr(baseType: baseType, line: baseType.line, col: baseType.col)
    of tkBitwiseAnd: # LValue Reference
      consumeToken(p)
      # Check for const immediately after & (e.g., T& const - pointer-to-const-ref is not a thing, but const ref is)
      if p.l.currentToken.kind == tkConst:
          consumeToken(p)
          baseType = ReferenceTypeExpr(baseType: baseType, isConst: true, line: baseType.line, col: baseType.col)
      else:
          baseType = ReferenceTypeExpr(baseType: baseType, isConst: false, line: baseType.line, col: baseType.col)
    # TODO: Handle RValue Reference (&&) - tkLogicalAnd? Need careful lexer handling
    of tkLBracket: # Array
      let line = p.l.currentToken.line; let col = p.l.currentToken.col
      consumeToken(p) # Consume '['
      var sizeExpr: Expression = nil
      if p.l.currentToken.kind != tkRBracket:
        sizeExpr = p.parseExpression(Precedence.Assign) # Parse size expression
        if sizeExpr == nil:
          p.errors.add("Failed to parse array size expression at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
          # Attempt recovery: consume until ']'?
          while p.l.currentToken.kind != tkRBracket and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile:
              consumeToken(p)
          if p.l.currentToken.kind != tkRBracket: return nil # Give up if no ']' found
      if p.l.currentToken.kind != tkRBracket:
        p.errors.add("Expected ']' after array dimension, got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
        return nil
      consumeToken(p) # Consume ']'
      baseType = ArrayTypeExpr(elementType: baseType, size: sizeExpr, line: line, col: col)
    of tkConst: # Trailing const (e.g., int* const)
      consumeToken(p)
      baseType = ConstQualifiedTypeExpr(baseType: baseType, line: baseType.line, col: baseType.col)
    of tkVolatile: # Trailing volatile
      consumeToken(p)
      baseType = VolatileQualifiedTypeExpr(baseType: baseType, line: baseType.line, col: baseType.col)
    # TODO: Handle function types (parentheses for parameters)
    else:
      break # No more type modifiers

  result = baseType

proc parseParameter*(p: var Parser): Parameter =
  # Parses a single function parameter (e.g., const int& x = 0)
  result = nil # ProveInit
  let line = p.l.currentToken.line; let col = p.l.currentToken.col

  let paramType = p.parseType()
  if paramType == nil:
    p.errors.add("Failed to parse parameter type at " & $line & ":" & $col)
    return nil

  var paramName: Identifier = nil
  if p.l.currentToken.kind == tkIdentifier:
    paramName = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p) # Consume parameter name

  var defaultValue: Expression = nil
  if p.l.currentToken.kind == tkAssign:
    consumeToken(p) # Consume '='
    defaultValue = p.parseExpression(Precedence.Assign)
    if defaultValue == nil:
      p.errors.add("Failed to parse default value for parameter '" & (if paramName != nil: paramName.name else: "<unnamed>") & "' at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
      # Don't return nil here, maybe the rest of the list is okay

  result = Parameter(paramType: paramType, name: paramName, defaultValue: defaultValue, line: line, col: col)

proc parseParameterList*(p: var Parser): seq[Parameter] = # Exported
  # Parses a comma-separated list of parameters within parentheses ()
  result = @[]
  if p.l.currentToken.kind != tkLParen:
    p.errors.add("Expected '(' to start parameter list, got " & $p.l.currentToken.kind)
    return result # Return empty list on error
  consumeToken(p) # Consume '('

  if p.l.currentToken.kind == tkRParen:
    consumeToken(p) # Consume ')' for empty list
    return result

  # Parse first parameter
  let firstParam = p.parseParameter()
  if firstParam == nil:
    p.errors.add("Failed to parse first parameter")
    # Attempt recovery: consume until ',' or ')'?
    while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    if p.l.currentToken.kind == tkEndOfFile: return @[] # Give up
    if p.l.currentToken.kind == tkRParen:
        consumeToken(p)
        return @[] # Give up but consume ')'
    # If comma, loop might recover
  else:
    result.add(firstParam)

  # Parse subsequent parameters
  while p.l.currentToken.kind == tkComma:
    consumeToken(p) # Consume ','
    if p.l.currentToken.kind == tkRParen: # Handle trailing comma before ')'
        p.errors.add("Unexpected trailing comma in parameter list before ')'")
        break
    let nextParam = p.parseParameter()
    if nextParam == nil:
      p.errors.add("Failed to parse parameter after ','")
      # Attempt recovery
      while p.l.currentToken.kind != tkComma and p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkEndOfFile:
          consumeToken(p)
      if p.l.currentToken.kind == tkEndOfFile: return @[] # Give up
      if p.l.currentToken.kind == tkRParen: break # Let the final check handle ')'
      # If comma, loop might recover
    else:
      result.add(nextParam)

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' to end parameter list, got " & $p.l.currentToken.kind)
    # Attempt recovery: consume until ')' or likely end?
    while p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    if p.l.currentToken.kind != tkRParen: return @[] # Give up if no ')' found
  consumeToken(p) # Consume ')'

# TODO: Add parseTemplateParameter, parseTemplateParameterList
