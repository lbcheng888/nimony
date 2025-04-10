<<<<<<< HEAD
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
=======
# C++ Parser - Type, Declarator, Parameter, Attribute, Template Parsing Logic

import std/options
import std/options # Ensure std/options is imported
import ./lexer
import ./ast
import ./parser_core # Import core definitions (Parser type, Precedence, helpers, and forward declarations)
# Removed import ./parser_expressions
# Removed imports for parser_statements to break circular dependency

# --- Forward Declarations for functions implemented in this module ---
proc parseAttribute*(p: var Parser): Attribute # Added forward declaration

# --- Type, Declarator, Parameter Parsing ---
# C++ Parser - Type, Declarator, Parameter, Attribute, Template Parsing Logic

import std/options
import std/options # Ensure std/options is imported
import ./lexer
import ./ast
import ./parser_core # Import core definitions (Parser type, Precedence, helpers, and forward declarations)
# Removed import ./parser_expressions
# Removed imports for parser_statements to break circular dependency

# --- Removed internal forward declarations; should come from parser_core ---

# --- Type, Declarator, Parameter Parsing ---

proc parseType(p: var Parser): TypeExpr =
  # --- NEW: Parse leading attributes ---
  let attributes = p.parseAttributeSpecifierSeq()
  # --- END NEW ---

  var initialConst = false
  var initialVolatile = false
  var qualifierToken = p.l.currentToken # Token for first qualifier

  # Parse const/volatile qualifiers *after* attributes
  while p.l.currentToken.kind == tkConst or p.l.currentToken.kind == tkVolatile:
    if not initialConst and not initialVolatile: qualifierToken = p.l.currentToken
    if p.l.currentToken.kind == tkConst: initialConst = true
    else: initialVolatile = true
    consumeToken(p)

  var baseType: TypeExpr
  let currentKind = p.l.currentToken.kind # Store current kind
  let line = p.l.currentToken.line
  let col = p.l.currentToken.col

  # Check for built-in type keywords OR identifier OR auto
  if currentKind == tkIdentifier or
     currentKind == tkVoid or currentKind == tkBool or currentKind == tkChar or
     currentKind == tkInt or currentKind == tkFloat or currentKind == tkDouble or
     currentKind == tkLong or currentKind == tkShort or currentKind == tkSigned or
     currentKind == tkUnsigned or currentKind == tkAuto:
    let typeName = p.l.currentToken.literal # Use literal which holds the keyword/identifier string
    consumeToken(p)
    baseType = BaseTypeExpr(name: typeName, line: line, col: col)
  elif currentKind == tkDecltype: # NEW: Handle decltype
    baseType = p.parseDecltypeSpecifier()
    if baseType == nil: return nil # Error handled inside parseDecltypeSpecifier
  # TODO: Handle struct/class/enum keywords explicitly if needed
  else:
    let expectedMsg = if initialConst or initialVolatile: "base type name after qualifier(s)" else: "base type name or decltype"
    p.errors.add("Expected " & expectedMsg & ", got " & $p.l.currentToken.kind & " at " & $line & ":" & $col) # Use captured line/col
    return nil

  var currentType = baseType
  # Apply qualifiers
  if initialVolatile:
    currentType = VolatileQualifiedTypeExpr(baseType: currentType, line: qualifierToken.line, col: qualifierToken.col)
  if initialConst:
    currentType = ConstQualifiedTypeExpr(baseType: currentType, line: qualifierToken.line, col: qualifierToken.col)

  # --- NEW: Attach attributes ---
  # Check if the final type node has an 'attributes' field.
  # Assuming BaseTypeExpr, PointerTypeExpr, etc. have an 'attributes: seq[AttributeGroup]' field.
  # If not, this assignment will fail compilation, and we'll need to update ast.nim.
  if attributes.len > 0:
      if currentType != nil:
          # Assign attributes. TypeExpr and its subtypes inherit this field.
          currentType.attributes = attributes
      else:
          # This shouldn't happen if baseType parsing succeeded.
          p.errors.add("Attributes parsed but no valid type node created.")

  # --- END NEW ---

  return currentType

# NEW: Implementation for parseDecltypeSpecifier
proc parseDecltypeSpecifier(p: var Parser): TypeExpr =
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
  consumeToken(p) # Consume ')'

  return DecltypeSpecifier(expression: expr, line: line, col: col)

proc parseDeclarator(p: var Parser, baseType: TypeExpr): (Identifier, TypeExpr) =
  var derivedType = baseType
  var line = p.l.currentToken.line
  var col = p.l.currentToken.col
  var name: Identifier = nil

  while p.l.currentToken.kind == tkStar or
        p.l.currentToken.kind == tkBitwiseAnd or
        p.l.currentToken.kind == tkConst or
        p.l.currentToken.kind == tkVolatile:
    let modifierToken = p.l.currentToken
    # Consume the modifier token *before* checking the next one for const& case
    consumeToken(p)
    case modifierToken.kind:
    of tkStar: derivedType = PointerTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    of tkBitwiseAnd:
      var isConstRef = false
      if p.l.currentToken.kind == tkConst:
        isConstRef = true
        consumeToken(p) # Consume the 'const' after '&'
      derivedType = ReferenceTypeExpr(baseType: derivedType, isConst: isConstRef, line: modifierToken.line, col: modifierToken.col)
    of tkConst: derivedType = ConstQualifiedTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    of tkVolatile: derivedType = VolatileQualifiedTypeExpr(baseType: derivedType, line: modifierToken.line, col: modifierToken.col)
    else: discard

  # Check if the '(' is for grouping declarators or starting a function parameter list
  if p.l.currentToken.kind == tkLParen:
    let peekKind = p.l.peekToken.kind
    # Heuristic: If followed by *, &, (, or identifier, assume grouping for precedence
    if peekKind == tkStar or peekKind == tkBitwiseAnd or peekKind == tkLParen or peekKind == tkIdentifier:
      consumeToken(p) # Consume '(' for grouping
      var innerType: TypeExpr
      (name, innerType) = p.parseDeclarator(derivedType) # Recursive call for inner part
      if name == nil and innerType == nil:
        p.errors.add("Failed to parse inner declarator within parentheses")
        return (nil, nil)
      if p.l.currentToken.kind != tkRParen:
        p.errors.add("Expected ')' after parenthesized declarator, got " & $p.l.currentToken.kind)
        return (nil, nil)
      consumeToken(p) # Consume ')'
      derivedType = innerType
      # Update line/col info if possible
      if name != nil:
          line = name.line; col = name.col
      elif innerType != nil:
          line = innerType.line; col = innerType.col
    # else: It's likely the start of a function parameter list.
    #       Do nothing here; let the later while loop handle it.

  elif p.l.currentToken.kind == tkIdentifier:
    name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    line = name.line; col = name.col
    consumeToken(p)
  else:
    discard "Allowing abstract declarator (nil name)"
    if derivedType != nil:
        line = derivedType.line; col = derivedType.col

  # Loop for array dimensions '[]' and function parameters '()'
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
      # Assume this '(' starts a parameter list for a function type/declarator
      let parenLine = p.l.currentToken.line; let parenCol = p.l.currentToken.col
      let errorCountBefore = p.errors.len # Store error count before parsing params
      let funcParams = p.parseParameterList() # This should consume '(' and ')' on success

      # Check if parseParameterList succeeded by seeing if errors increased
      if p.errors.len > errorCountBefore:
          # parseParameterList failed and added errors.
          # We cannot continue parsing this declarator.
          return (nil, nil) # Propagate failure

      # Parameter list parsed successfully (or was empty and parsed correctly).
      # Now parse trailing qualifiers.
      var isConstFunc = false
      var isVolatileFunc = false
      var refQual = rqNone
      var isNoexceptFunc = false
      var qualifierParsed = true
      while qualifierParsed:
        qualifierParsed = false
        if p.l.currentToken.kind == tkConst:
          isConstFunc = true; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkVolatile:
          isVolatileFunc = true; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkBitwiseAnd:
          if refQual != rqNone: p.errors.add("Multiple ref-qualifiers (&, &&) specified")
          refQual = rqLValue; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkLogicalAnd: # Assuming && for rvalue ref
          if refQual != rqNone: p.errors.add("Multiple ref-qualifiers (&, &&) specified")
          refQual = rqRValue; consumeToken(p); qualifierParsed = true
        elif p.l.currentToken.kind == tkNoexcept:
          isNoexceptFunc = true; consumeToken(p); qualifierParsed = true
        # else: No more qualifiers found

      derivedType = FunctionTypeExpr(
        returnType: derivedType, parameters: funcParams,
        isConst: isConstFunc, isVolatile: isVolatileFunc,
        refQualifier: refQual, isNoexcept: isNoexceptFunc,
        line: parenLine, col: parenCol
      )
      # After parsing function parameters and qualifiers, this part of the declarator is done.
      break # Exit the while loop for declarator modifiers

  if derivedType == nil: return (nil, nil)
  # Also check for errors accumulated during declarator parsing
  # (Though specific checks within the loop might be better)
  # For now, let's rely on the parameter list check above.
  else: return (name, derivedType)

proc parseParameterList(p: var Parser): seq[Parameter] =
  result = @[] # Initialize result

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

# --- Exception Handling Clauses ---

# Moved parseCatchClause to parser_statements.nim to resolve circular dependency

# --- Template Implementations ---

proc parseTemplateParameter(p: var Parser): TemplateParameter =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  var kind: TemplateParameterKind
  var name: Identifier = nil
  var paramType: TypeExpr = nil
  var defaultValue: Expression = nil
  var defaultType: TypeExpr = nil # NEW: For default type
  var templateParams: seq[TemplateParameter] = @[] # For template template params

  if p.l.currentToken.kind == tkTypename or p.l.currentToken.kind == tkClass:
    kind = tpTypename
    consumeToken(p) # Consume 'typename' or 'class'
    if p.l.currentToken.kind == tkIdentifier:
      name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
      consumeToken(p)
    # Optional default type
    if p.l.currentToken.kind == tkAssign:
      consumeToken(p) # Consume '='
      defaultType = p.parseType() # Default for typename is a type
      if defaultType == nil:
        p.errors.add("Failed to parse default type for template type parameter")
        return nil
  elif p.l.currentToken.kind == tkTemplate:
    # Template template parameter: template <...> class Name
    kind = tpTemplate
    consumeToken(p) # Consume 'template'
    if p.l.currentToken.kind != tkLess:
      p.errors.add("Expected '<' after 'template' for template template parameter")
      return nil
    # Parse nested template parameters
    let nestedParamsOpt = p.parseTemplateParameterList()
    if nestedParamsOpt.isNone: # Check if parsing failed
        return nil
    templateParams = nestedParamsOpt.get() # Assign if successful
    # Expect 'class' or 'typename' after '>'
    if p.l.currentToken.kind != tkClass and p.l.currentToken.kind != tkTypename:
        p.errors.add("Expected 'class' or 'typename' after template template parameter list")
        return nil
    consumeToken(p) # Consume 'class' or 'typename'
    # Expect the name of the template template parameter
    if p.l.currentToken.kind == tkIdentifier:
        name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
        consumeToken(p)
    else:
        p.errors.add("Expected identifier for template template parameter name")
        # Allow anonymous? Maybe not standard.
    # TODO: Default for template template parameter?
  else:
    # Non-type template parameter (e.g., int N, auto V)
    kind = tpType
    paramType = p.parseType()
    if paramType == nil:
      p.errors.add("Failed to parse type for non-type template parameter")
      return nil
    # Name is optional for non-type parameters too (though less common)
    if p.l.currentToken.kind == tkIdentifier:
      name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
      consumeToken(p)
    # Optional default value
    if p.l.currentToken.kind == tkAssign:
      consumeToken(p) # Consume '='
      defaultValue = p.parseExpression(Precedence.Assign) # Default is an expression
      if defaultValue == nil:
        p.errors.add("Failed to parse default value for non-type template parameter")
        return nil

  return TemplateParameter(
    kind: kind, name: name, paramType: paramType,
    defaultValue: defaultValue, defaultType: defaultType, # Use new field
    templateParams: templateParams,
    line: line, col: col
  )

# Changed return type to Option
proc parseTemplateParameterList(p: var Parser): Option[seq[TemplateParameter]] =
  var resultSeq: seq[TemplateParameter] = @[] # Use a local seq
  if p.l.currentToken.kind != tkLess:
    p.errors.add("Expected '<' to start template parameter list")
    return none(seq[TemplateParameter]) # Indicate failure
  consumeToken(p) # Consume '<'

  if p.l.currentToken.kind == tkGreater: # Empty list <>
    consumeToken(p) # Consume '>'
    return some(resultSeq) # Return empty seq wrapped in Option

  block parseFirstParam:
    let param = p.parseTemplateParameter()
    if param == nil: return none(seq[TemplateParameter]) # Error already added, propagate failure
    resultSeq.add(param)

  while p.l.currentToken.kind == tkComma:
    consumeToken(p) # Consume ','
    if p.l.currentToken.kind == tkGreater: break # Trailing comma? (Unlikely for templates)
    block parseNextParam:
      let param = p.parseTemplateParameter()
      if param == nil: return none(seq[TemplateParameter]) # Error already added, propagate failure
      resultSeq.add(param)

  if p.l.currentToken.kind != tkGreater:
    p.errors.add("Expected '>' or ',' in template parameter list, got " & $p.l.currentToken.kind)
    return none(seq[TemplateParameter]) # Indicate failure
  consumeToken(p) # Consume '>'
  return some(resultSeq) # Return populated seq wrapped in Option

# --- Attribute Implementations (NEW) ---

proc parseAttributeArgument(p: var Parser): AttributeArgument =
  # Simplified: Parse as a general expression for now.
  # C++ attribute arguments can be complex (pack expansion, etc.)
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let expr = p.parseExpression(Precedence.Assign) # Use Assign precedence? Lowest?
  if expr == nil:
    p.errors.add("Failed to parse attribute argument expression")
    return nil
  return AttributeArgument(value: expr, line: line, col: col)

proc parseAttribute(p: var Parser): Attribute =
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

  return Attribute(scope: scope, name: name, arguments: arguments, line: line, col: col)

proc parseAttributeGroup(p: var Parser): AttributeGroup =
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

  return AttributeGroup(attributes: attributes, line: line, col: col)

# --- Enum Helper ---
proc parseEnumerator(p: var Parser): Enumerator =
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

  return Enumerator(name: name, value: value, line: name.line, col: name.col)

# Removed duplicate implementations that were added below
>>>>>>> 8f2d5ac (update nifcpp)
