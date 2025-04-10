# C++ Parser - Statement Parsing Logic

import std/strutils # Added for contains
import ./lexer
import ./ast
import ./parser_core # Import core definitions (Parser type, Precedence, helpers, and forward declarations)
# Removed imports for parser_expressions and parser_types to break circular dependency

# --- Statement Parsing Implementations ---

proc parseBreakStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'break'
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after 'break', got " & $p.l.currentToken.kind)
    return nil # Indicate failure
  consumeToken(p) # Consume ';'
  return BreakStatement(line: line, col: col)

proc parseContinueStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'continue'
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after 'continue', got " & $p.l.currentToken.kind)
    return nil # Indicate failure
  consumeToken(p) # Consume ';'
  return ContinueStatement(line: line, col: col)

proc parseCaseStatement(p: var Parser): Statement =
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
  return CaseStatement(value: value, line: line, col: col)

proc parseSwitchStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'switch'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('
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

  # Parse the body as a normal block statement.
  # The block statement will contain CaseStatement nodes interspersed with other statements.
  let bodyStmt = p.parseBlockStatement()
  if bodyStmt == nil or not (bodyStmt of BlockStatement):
    p.errors.add("Failed to parse switch statement body")
    return nil

  return SwitchStatement(controlExpr: controlExpr, body: BlockStatement(bodyStmt), line: line, col: col)

proc parseEnumDefinition(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'enum'

  var isClass = false
  if p.l.currentToken.kind == tkClass or p.l.currentToken.kind == tkStruct: # Allow 'enum struct' too
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
    # Allow enum forward declaration (e.g., enum Color : int;)
    if enumName != nil and p.l.currentToken.kind == tkSemicolon:
        consumeToken(p) # Consume ';'
        # TODO: Need a specific AST node for forward declarations?
        # For now, return a partial EnumDefinition or nil? Returning nil.
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

  # Optional semicolon after enum definition
  if p.l.currentToken.kind == tkSemicolon:
    consumeToken(p)

  return EnumDefinition(name: enumName, isClass: isClass, baseType: baseType, enumerators: enumerators, line: line, col: col)

proc parseTypeAliasDeclaration(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let keyword = p.l.currentToken.kind # tkTypedef or tkUsing
  consumeToken(p)

  if keyword == tkTypedef:
    # typedef <original_type> <new_name>;
    # The original type parsing is complex because it includes declarators
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
    return TypeAliasDeclaration(name: newName, aliasedType: finalOriginalType, line: line, col: col)

  elif keyword == tkUsing:
    # using <new_name> = <original_type>;
    if p.l.currentToken.kind != tkIdentifier:
      p.errors.add("Expected new name identifier after 'using'")
      return nil
    let newName = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)

    if p.l.currentToken.kind != tkAssign:
      p.errors.add("Expected '=' after new name in using declaration")
      return nil
    consumeToken(p) # Consume '='

    # --- Refined approach for 'using' ---
    # We consumed 'using' and 'newName'. Current token is '='. Consume it.
    # Now parse the full type specification.
    let aliasedBaseType = p.parseType()
    if aliasedBaseType == nil:
        p.errors.add("Failed to parse aliased type after '=' in using declaration")
        return nil
    # Now parse the declarator part, but *without* expecting a name
    let (abstractName, finalAliasedType) = p.parseDeclarator(aliasedBaseType)
    if finalAliasedType == nil:
        p.errors.add("Failed to parse declarator part of aliased type in using declaration")
        return nil
    if abstractName != nil:
        p.errors.add("Unexpected name found in aliased type declarator for 'using' alias")
        # Continue anyway?

    if p.l.currentToken.kind != tkSemicolon:
      p.errors.add("Expected ';' after using declaration")
      return nil
    consumeToken(p) # Consume ';'
    return TypeAliasDeclaration(name: newName, aliasedType: finalAliasedType, line: line, col: col)
  else:
    # Should not happen if called correctly
    p.errors.add("Internal parser error: parseTypeAliasDeclaration called with unexpected token " & $keyword)
    return nil

# --- Main Statement Parsing Logic ---
proc parseStatement(p: var Parser): Statement =
  result = nil
  while p.l.currentToken.kind == tkNewline: consumeToken(p)
  let attributes = p.parseAttributeSpecifierSeq()
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let currentKind = p.l.currentToken.kind # Store kind *after* attributes

  case currentKind
  # Explicit statement keywords first
  of tkLBrace: result = p.parseBlockStatement()
  of tkIf: result = p.parseIfStatement()
  of tkWhile: result = p.parseWhileStatement()
  of tkFor: result = p.parseForStatement()
  of tkReturn: result = p.parseReturnStatement()
  of tkTry: result = p.parseTryStatement()
  of tkTemplate: result = p.parseTemplateDeclaration()
  of tkNamespace: result = p.parseNamespaceDefinition()
  of tkClass, tkStruct: result = p.parseClassDefinition()
  of tkSemicolon: consumeToken(p); result = nil # Empty statement
  of tkBreak: result = p.parseBreakStatement()
  of tkContinue: result = p.parseContinueStatement()
  of tkSwitch: result = p.parseSwitchStatement()
  of tkCase, tkDefault: result = p.parseCaseStatement()
  of tkEnum: result = p.parseEnumDefinition()
  of tkTypedef, tkUsing: result = p.parseTypeAliasDeclaration()

  # Now, differentiate between declaration and expression
  else:
    # --- Heuristic: Try parsing as declaration ONLY if it starts with common type keywords/qualifiers ---
    let looksLikeDeclarationStart = case currentKind
      of tkVoid, tkBool, tkChar, tkInt, tkFloat, tkDouble, tkLong, tkShort, tkSigned, tkUnsigned, tkConst, tkVolatile, tkTypename, tkDecltype, tkClass, tkStruct, tkEnum: true
      of tkIdentifier: true # Identifier could be a type name starting a declaration
      else: false

    if looksLikeDeclarationStart:
      # Save state for potential backtrack if declaration parse fails early
      let initialCurrentToken = p.l.currentToken
      let initialPeekToken = p.l.peekToken
      let initialErrorsLen = p.errors.len

      let declStmt = p.parseDeclarationStatement(requireSemicolon = true, attributes = attributes)

      if declStmt != nil:
        result = declStmt # Successfully parsed as declaration
      else:
        # Declaration parsing failed.
        # Simplified Backtrack: If no *new* errors were added during the attempt,
        # try parsing as an expression statement. Reset state first.
        if p.errors.len == initialErrorsLen:
           # Reset state
           p.l.currentToken = initialCurrentToken
           p.l.peekToken = initialPeekToken
           # Now try parsing as expression statement
           if attributes.len > 0: p.errors.add("Attributes ignored before expression statement at " & $line & ":" & $col)
           result = p.parseExpressionStatement()
        else:
           # Declaration parsing failed AND added errors. Trust the errors.
           result = nil
    else:
      # Doesn't look like a declaration start, assume expression statement
      if attributes.len > 0: p.errors.add("Attributes ignored before expression statement at " & $line & ":" & $col)
      result = p.parseExpressionStatement()

  # Assign attributes (if result is not nil and not a type that handles its own attributes)
  if result != nil and not (result of VariableDeclaration or result of FunctionDefinition or result of ClassDefinition or result of EnumDefinition or result of TypeAliasDeclaration or result of NamespaceDef or result of TemplateDecl):
      # Ensure the node type actually *has* an attributes field before assigning.
      # This requires checking the AST definition or using dynamic checks/casts.
      # For simplicity, we assume common statement types might have attributes.
      # A more robust solution would involve type introspection or specific checks.
      try:
          result.attributes = attributes
      except FieldDefect:
          # The specific statement type doesn't have an 'attributes' field. Ignore.
          discard

  # If result is still nil after all attempts, report generic error if none exists for current token
  if result == nil and p.l.currentToken.kind != tkEndOfFile and p.l.currentToken.kind != tkRBrace: # Don't error on EOF or closing brace
      var foundErrorForToken = false
      if p.errors.len > 0:
          # Check if the *last* error corresponds to the *current* token's position
          if strutils.contains(p.errors[^1], "at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col): # Use strutils.contains
              foundErrorForToken = true
      if not foundErrorForToken:
          p.errors.add("Failed to parse statement starting with " & $p.l.currentToken.kind & " at " & $line & ":" & $col)

  return result

proc parseBlockStatement(p: var Parser): Statement =
  let blck = BlockStatement(line: p.l.currentToken.line, col: p.l.currentToken.col, statements: @[])
  # echo "[DEBUG parseBlockStmt] Start. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  consumeToken(p) # Consume '{'
  # echo "[DEBUG parseBlockStmt] After consuming '{'. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind

  while p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
    # echo "[DEBUG parseBlockStmt] Loop start. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    let stmt = p.parseStatement()
    if stmt != nil:
      blck.statements.add(stmt)
      # echo "[DEBUG parseBlockStmt] Added statement: ", repr(stmt), ". current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    else:
      # If parseStatement returned nil, consume the current token to prevent infinite loops.
      if p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
        # Add a generic error if no specific one was added by parseStatement for this token
        var foundErrorForToken = false
        if p.errors.len > 0:
          if strutils.contains(p.errors[^1], "at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col): # Use strutils.contains
            foundErrorForToken = true
        # Declare skippedToken *inside* this block
        let skippedToken = p.l.currentToken
        if not foundErrorForToken:
          # Corrected indentation (level 4)
          p.errors.add("Skipping token inside block after failed statement parse: " & $skippedToken.kind & " at " & $skippedToken.line & ":" & $skippedToken.col)
        # Corrected indentation (level 3)
        # echo "[DEBUG parseBlockStmt] Skipping token in recovery: ", $skippedToken.kind
        # Corrected indentation (level 3)
        consumeToken(p) # Always consume to advance within the block

  # Correct indentation for the rest of the block
  # echo "[DEBUG parseBlockStmt] Loop finished. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
  if p.l.currentToken.kind != tkRBrace:
    p.errors.add("Expected '}' to close block, got " & $p.l.currentToken.kind & " instead.")
    # echo "[DEBUG parseBlockStmt] Error: Expected '}'. Returning."
  else:
    # Correct indentation and complete the echo statement
    # echo "[DEBUG parseBlockStmt] Consuming '}'. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind
    consumeToken(p) # Consume '}'
    # Correct indentation and complete the echo statement
    # echo "[DEBUG parseBlockStmt] After consuming '}'. current=", $p.l.currentToken.kind, " peek=", $p.l.peekToken.kind

  # Correct indentation for the final echo and return
  # echo "[DEBUG parseBlockStmt] End. Returning block."
  return blck

# Implementation for parseExpressionStatement (Added)
proc parseExpressionStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line
  let col = p.l.currentToken.col
  let expr = p.parseExpression(Precedence.Lowest) # Use qualified Lowest
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
    return stmt # Return the valid statement
  else:
    p.errors.add("Missing semicolon after expression statement at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    # Return nil on missing semicolon for stricter parsing.
    return nil

# Parses a variable declaration OR function definition
proc parseDeclarationStatement(p: var Parser,
                               parsedType: TypeExpr = nil,
                               requireSemicolon: bool = true,
                               attributes: seq[AttributeGroup] = @[]): Statement = # Added attributes param
  let startTokenKind = p.l.currentToken.kind
  # echo "[DEBUG parseDeclStmt] Start. Token=", startTokenKind
  var line = p.l.currentToken.line
  var col = p.l.currentToken.col
  result = nil # Initialize result for ProveInit

  # --- NEW: Use passed-in attributes or parse them ---
  # Note: parseStatement already parses attributes and passes them in.
  # This function assumes 'attributes' contains the relevant attributes.
  var localAttributes = attributes
  # Update line/col based on where we start parsing the *type* or *declarator*,
  # which happens *after* attributes have been parsed by parseStatement.
  line = p.l.currentToken.line; col = p.l.currentToken.col
  # --- END NEW ---

  var baseType: TypeExpr = parsedType
  if baseType == nil:
    # Update line/col again before parsing type
    line = p.l.currentToken.line; col = p.l.currentToken.col
    baseType = p.parseType() # parseType needs to handle attributes too
    # If parseType returned nil because it saw an identifier (or other non-type),
    # baseType will be nil here.
    if baseType == nil:
        # This means the sequence didn't start with a recognizable type specifier.
        # It cannot be a declaration statement according to our current logic.
        # Return nil to let parseStatement try parsing as an expression statement.
        # Do NOT add an error here, as it might just be an expression statement.
        return nil
  else:
    # If type was pre-parsed, use its location
    line = baseType.line; col = baseType.col

  let (name, finalType) = p.parseDeclarator(baseType)
  if finalType == nil:
      p.errors.add("Failed to parse declarator after type specifiers")
      # echo "[DEBUG parseDeclStmt] End (Declarator Failed). Returning nil."
      return nil

  # Check if the token following the declarator indicates a function body
  if p.l.currentToken.kind == tkLBrace: # Function Definition
      if name == nil:
          p.errors.add("Function definition requires a name.")
          return nil
      if not (finalType of FunctionTypeExpr):
          p.errors.add("Expected function declarator '(...)' for function definition.")
          return nil
      let funcType = FunctionTypeExpr(finalType)
      let bodyStmt = p.parseBlockStatement()
      if bodyStmt == nil or not (bodyStmt of BlockStatement):
          p.errors.add("Failed to parse function body for " & name.name)
          return nil
      result = FunctionDefinition(
          returnType: funcType.returnType, name: name, parameters: funcType.parameters,
          body: BlockStatement(bodyStmt), line: line, col: col, attributes: localAttributes # Assign attributes
      )
      # Function definitions don't require a semicolon after the body. Return immediately.
      # Attributes are already assigned.
      return result # Explicitly return here

  # If not a function definition, assume it's a variable declaration (or error)
  # TODO: Handle '= delete', '= default' for functions here?

  # --- Variable Declaration Logic ---
  # This part should execute if it's NOT a function definition (no '{' follows declarator)
  var initializer: Expression = nil # Initialize here

  # Check for initializer ONLY if a name was parsed (usually required for var decl)
  if name != nil: # Only parse initializer if we have a variable name
      if p.l.currentToken.kind == tkAssign:
        consumeToken(p) # Consume '='
        initializer = p.parseExpression(Precedence.Assign) # Parse initializer expression
        if initializer == nil:
          p.errors.add("Failed to parse initializer expression after '=' for " & name.name)
          # Don't return nil immediately, let semicolon check handle final failure
      elif p.l.currentToken.kind == tkLBrace: # Brace initialization {}
        initializer = p.parseInitializerListExpression()
        if initializer == nil:
          p.errors.add("Failed to parse initializer list '{...}' for " & name.name)
          # Don't return nil immediately
      elif p.l.currentToken.kind == tkLParen: # Constructor-style init T var(args...);
        # Need to be careful not to misinterpret function pointer declarations
        # Let's assume constructor init only happens if name is not nil
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
        if p.l.currentToken.kind != tkRParen:
            p.errors.add("Expected ')' after constructor arguments for " & name.name)
            return nil # Fail hard
        consumeToken(p) # Consume ')'
        # Represent constructor call using InitializerListExpr for now
        initializer = InitializerListExpr(values: args, line: parenLine, col: parenCol)
          # p.errors.add("Constructor-style initialization T(...) parsed, AST representation is placeholder.")

  # If name was nil, but it wasn't a function definition, it's an error unless allowed contextually (e.g., function param)
  elif name == nil and p.l.currentToken.kind != tkLBrace: # Check again, name is nil and no function body
      # This path shouldn't be reached if called from parseStatement for a top-level/block statement
      # because parseStatement expects a declaration or expression. An abstract declarator isn't a statement.
      p.errors.add("Abstract declarator found where statement was expected.")
      # echo "[DEBUG parseDeclStmt] End (Abstract Declarator Error). Returning nil."
      return nil

  # echo "[DEBUG parseDeclStmt] Before semicolon check. Token=", p.l.currentToken.kind, " requireSemicolon=", requireSemicolon

  # Check for semicolon if required
  if requireSemicolon:
    if p.l.currentToken.kind != tkSemicolon:
       let varName = if name != nil: name.name else: "<abstract>"
       p.errors.add("Missing semicolon after variable declaration for " & varName & ", got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
       # echo "[DEBUG parseDeclStmt] End (Missing Semicolon). Returning nil."
       return nil # Fail if semicolon is missing when required
    consumeToken(p) # Consume ';'
    # echo "[DEBUG parseDeclStmt] Consumed semicolon. Token=", p.l.currentToken.kind

  # Create the node only if a name was present (standard variable declaration)
  if name != nil:
      result = VariableDeclaration(
        varType: finalType, name: name, initializer: initializer,
        line: line, col: col, attributes: localAttributes # Assign attributes
      )
      # echo "[DEBUG parseDeclStmt] End (VarDecl Success). Returning VarDecl node."
  else:
      # If name is nil here, it means we parsed an abstract declarator followed by a semicolon (or end of input).
      # This isn't a valid standalone statement.
      # We already added an error above if name was nil and no function body followed.
      result = nil # Ensure result is nil if no valid statement was formed
      # echo "[DEBUG parseDeclStmt] End (Abstract Declarator). Returning nil."

  # Return the result (VariableDeclaration or nil)
  # Corrected indentation: This should be at the same level as the 'if name != nil:' block
  return result

# Implementation for parseIfStatement
proc parseIfStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'if'

  # TODO: Handle 'if constexpr'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('

  # TODO: Handle condition with initializer (e.g., if (int x = foo(); x > 0))

  let condition = p.parseExpression(Precedence.Lowest)
  if condition == nil:
    p.errors.add("Failed to parse condition in if statement")
    # Attempt recovery: skip until ')' or '{' or ';'
    while p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    return nil

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after if condition, got " & $p.l.currentToken.kind)
    # Attempt recovery: skip until '{' or ';'
    while p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    return nil
  consumeToken(p) # Consume ')'

  let thenBranch = p.parseStatement()
  if thenBranch == nil:
    p.errors.add("Failed to parse 'then' branch of if statement")
    # Recovery might be complex here, maybe just return nil
    return nil

  var elseBranch: Statement = nil
  if p.l.currentToken.kind == tkElse:
    consumeToken(p) # Consume 'else'
    elseBranch = p.parseStatement()
    if elseBranch == nil:
      p.errors.add("Failed to parse 'else' branch of if statement")
      # Don't necessarily fail the whole if statement if else fails,
      # but report the error. The node will have a nil elseBranch.

  return IfStatement(condition: condition, thenBranch: thenBranch, elseBranch: elseBranch, line: line, col: col)

# Implementation for parseWhileStatement (Added)
proc parseWhileStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'while'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('

  # TODO: Handle condition with declaration (C++17)

  let condition = p.parseExpression(Precedence.Lowest)
  if condition == nil:
    p.errors.add("Failed to parse condition in while statement")
    # Attempt recovery: skip until ')' or '{' or ';'
    while p.l.currentToken.kind != tkRParen and p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    return nil

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after while condition, got " & $p.l.currentToken.kind)
    # Attempt recovery: skip until '{' or ';'
    while p.l.currentToken.kind != tkLBrace and p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkEndOfFile:
        consumeToken(p)
    return nil
  consumeToken(p) # Consume ')'

  let body = p.parseStatement()
  if body == nil:
    p.errors.add("Failed to parse body of while statement")
    # Recovery might be complex here, maybe just return nil
    return nil

  return WhileStatement(condition: condition, body: body, line: line, col: col)

proc parseReturnStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'return'
  var returnValue: Expression = nil
  let errorCountBefore = p.errors.len # Declare error count *before* parsing expression

  # Check if there's an expression to parse before the semicolon
  if p.l.currentToken.kind != tkSemicolon:
    # let errorCountBefore = p.errors.len # Moved declaration outside
    returnValue = p.parseExpression(Precedence.Lowest)

    # Check if parseExpression itself failed and added errors
    if returnValue == nil and p.errors.len > errorCountBefore:
        # Expression parsing failed. Don't trust currentToken.
        # Attempt recovery by skipping until a semicolon or brace.
        p.errors.add("Failed to parse return value expression. Skipping to find semicolon.")
        while p.l.currentToken.kind != tkSemicolon and p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile: # Complete the condition
            consumeToken(p)
        # Now currentToken is either ;, }, EOF, or something unexpected if recovery failed.
    # else: Expression parsed successfully (or was absent),

  # Check for semicolon after the (potentially absent) return value
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after return statement, got " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
    # Attempt recovery: If we successfully parsed a value, return the node anyway?
    # Or return nil to indicate failure? Let's return nil for stricter parsing.
    return nil
  consumeToken(p) # Consume ';'

  return ReturnStatement(returnValue: returnValue, line: line, col: col) # Corrected field name

proc parseForStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'for'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('

  var init: Statement = nil
  if p.l.currentToken.kind != tkSemicolon:
    # Could be declaration or expression statement
    # Try parsing as declaration first
    init = p.parseDeclarationStatement(requireSemicolon = false) # Semicolon is part of 'for' syntax
    if init == nil:
      # If declaration failed, try expression statement
      init = p.parseExpressionStatement() # This expects a semicolon, which we don't have yet
      # Hacky fix: If parseExpressionStatement returned nil because of missing semicolon,
      # but the expression itself was parsed, extract the expression.
      # This is fragile. A better approach might be needed.
      if init == nil and p.errors.len > 0 and p.errors[^1].contains("Missing semicolon"):
          discard p.errors.pop() # Remove the semicolon error and discard the string
          # Need to reconstruct the expression statement without the semicolon check
          # This requires backtracking or a dedicated parseExpressionList function
          p.errors.add("Parsing expression-based initializer in for loop needs refinement.")
          # Attempt to re-parse just the expression part
          # Resetting state might be needed here if parseExpressionStatement consumed tokens
          # For now, let's assume it didn't consume much on semicolon error
          let exprInit = p.parseExpression(Precedence.Lowest)
          if exprInit != nil:
              init = ExpressionStatement(expression: exprInit, line: exprInit.line, col: exprInit.col)
          else:
              p.errors.add("Failed to parse initializer in for loop")
              return nil # Give up if expression also fails
      elif init == nil:
          # Both declaration and expression failed for other reasons
          p.errors.add("Failed to parse initializer in for loop")
          return nil

  # Now expect the first semicolon
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after for loop initializer")
    return nil
  consumeToken(p) # Consume ';'

  var condition: Expression = nil
  if p.l.currentToken.kind != tkSemicolon:
    condition = p.parseExpression(Precedence.Lowest)
    if condition == nil:
      p.errors.add("Failed to parse condition in for loop")
      return nil

  # Now expect the second semicolon
  if p.l.currentToken.kind != tkSemicolon:
    p.errors.add("Expected ';' after for loop condition")
    return nil
  consumeToken(p) # Consume ';'

  var update: Expression = nil
  if p.l.currentToken.kind != tkRParen:
    update = p.parseExpression(Precedence.Lowest)
    if update == nil:
      p.errors.add("Failed to parse update expression in for loop")
      # Don't fail immediately, update is optional

  # Now expect the closing parenthesis
  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after for loop clauses")
    return nil
  consumeToken(p) # Consume ')'

  let body = p.parseStatement()
  if body == nil:
    p.errors.add("Failed to parse body of for loop")
    return nil

  return ForStatement(initializer: init, condition: condition, update: update, body: body, line: line, col: col)

# --- Exception Handling Implementations ---

proc parseTryStatement(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'try'

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for try block body")
    return nil
  let tryBlock = p.parseBlockStatement()
  if tryBlock == nil or not (tryBlock of BlockStatement):
    p.errors.add("Failed to parse try block body")
    return nil

  var catchClauses: seq[CatchClause] = @[]
  while p.l.currentToken.kind == tkCatch:
    let catchClause = p.parseCatchClause()
    if catchClause == nil:
      # Error already added by parseCatchClause. Stop parsing catches.
      # We might have a valid try block even if a catch fails.
      break
    catchClauses.add(catchClause)

  if catchClauses.len == 0:
    p.errors.add("Expected at least one catch clause after try block")
    # C++ requires at least one catch, but we can still return the partial node
    # return nil

  return TryStatement(tryBlock: BlockStatement(tryBlock), catchClauses: catchClauses, line: line, col: col)

# Implementation for parseCatchClause (Moved from parser_types.nim)
proc parseCatchClause(p: var Parser): CatchClause =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  if p.l.currentToken.kind != tkCatch:
    p.errors.add("Expected 'catch' keyword")
    return nil
  consumeToken(p) # Consume 'catch'

  if not p.expectPeek(tkLParen): return nil
  consumeToken(p) # Consume '('

  var exceptionDecl: VariableDeclaration = nil
  var isCatchAll = false

  if p.l.currentToken.kind == tkEllipsis:
    isCatchAll = true
    consumeToken(p) # Consume '...'
  else:
    # Parse the exception declaration (e.g., const std::exception& e)
    # We can reuse parseDeclarationStatement, but it expects a statement context.
    # Let's parse type and declarator manually here.
    let baseType = p.parseType()
    if baseType == nil:
      p.errors.add("Failed to parse exception type in catch clause")
      return nil
    let (name, finalType) = p.parseDeclarator(baseType)
    if finalType == nil:
        p.errors.add("Failed to parse exception declarator in catch clause")
        return nil
    # Create a VariableDeclaration node to represent the exception declaration
    exceptionDecl = VariableDeclaration(varType: finalType, name: name, line: finalType.line, col: finalType.col)

  if p.l.currentToken.kind != tkRParen:
    p.errors.add("Expected ')' after catch declaration, got " & $p.l.currentToken.kind)
    return nil
  consumeToken(p) # Consume ')'

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for catch block body")
    return nil
  # Need to call parseBlockStatement from the main parser or statement parser
  # This requires including parser_statements or having it in parser.nim
  # Assuming parseBlockStatement is available via includes in parser.nim
  let bodyStmt = p.parseBlockStatement() # Forward declaration needed in parser_core?
  if bodyStmt == nil or not (bodyStmt of BlockStatement):
    p.errors.add("Failed to parse catch block body")
    return nil

  return CatchClause(
    exceptionDecl: exceptionDecl,
    isCatchAll: isCatchAll,
    body: BlockStatement(bodyStmt),
    line: line, col: col
  )

# --- Template Implementations ---

proc parseTemplateDeclaration(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'template'

  let parametersOpt = p.parseTemplateParameterList()
  if parametersOpt.isNone: # Check if list parsing failed
      p.errors.add("Failed to parse template parameter list")
      return nil
  let parameters = parametersOpt.get() # Assign if successful

  # Parse the actual declaration (class, function, variable C++14, alias C++11)
  # Need to handle attributes potentially *before* the declaration keyword
  let attributes = p.parseAttributeSpecifierSeq()
  let declaration = p.parseStatement() # Re-use statement parsing
  # TODO: parseStatement might need adjustments to handle template context better,
  # e.g., knowing that a following '<' might be template arguments, not 'less than'.
  # This is a major complexity of C++ parsing.

  if declaration == nil:
    p.errors.add("Expected class, function, variable, or alias declaration after template<...>")
    return nil

  # Assign attributes to the inner declaration if appropriate
  if attributes.len > 0:
      if declaration of VariableDeclaration: VariableDeclaration(declaration).attributes.add(attributes)
      elif declaration of FunctionDefinition: FunctionDefinition(declaration).attributes.add(attributes)
      elif declaration of ClassDefinition: ClassDefinition(declaration).attributes.add(attributes)
      elif declaration of TypeAliasDeclaration: TypeAliasDeclaration(declaration).attributes.add(attributes)
      # else: Ignore attributes for other statement types?

  return TemplateDecl(parameters: parameters, declaration: declaration, line: line, col: col)

# --- Namespace Implementation ---

proc parseNamespaceDefinition(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  consumeToken(p) # Consume 'namespace'

  var name: Identifier = nil
  if p.l.currentToken.kind == tkIdentifier:
    name = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)
  # else: anonymous namespace

  # TODO: Handle inline namespaces

  if p.l.currentToken.kind != tkLBrace:
    p.errors.add("Expected '{' for namespace body")
    return nil
  consumeToken(p) # Consume '{'

  var declarations: seq[Statement] = @[]
  while p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
    let decl = p.parseStatement() # Parse declarations/definitions within the namespace
    if decl != nil:
      declarations.add(decl)
    else:
      # Error recovery within namespace body
      if p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
        p.errors.add("Skipping token inside namespace body after failed parse: " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
        consumeToken(p)

  if p.l.currentToken.kind != tkRBrace:
    p.errors.add("Expected '}' to close namespace definition")
    # Allow returning partial result?
  else:
    consumeToken(p) # Consume '}'

  return NamespaceDef(name: name, declarations: declarations, line: line, col: col)

# Implementation for parseClassDefinition
proc parseClassDefinition(p: var Parser): Statement =
  let line = p.l.currentToken.line; let col = p.l.currentToken.col
  let keyword = p.l.currentToken.kind # tkClass or tkStruct
  consumeToken(p) # Consume 'class' or 'struct'

  # --- NEW: Parse attributes after keyword ---
  let attributes = p.parseAttributeSpecifierSeq()
  # --- END NEW ---

  var className: Identifier = nil
  if p.l.currentToken.kind == tkIdentifier:
    className = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
    consumeToken(p)

  # TODO: Parse final specifier

  # Parse base class specifiers (inheritance)
  var bases: seq[InheritanceInfo] = @[] # Use InheritanceInfo
  if p.l.currentToken.kind == tkColon:
    consumeToken(p) # Consume ':'
    block parseFirstBase:
      var access = AccessSpecifier.asDefault # Default access depends on class/struct
      if p.l.currentToken.kind == tkPublic or p.l.currentToken.kind == tkProtected or p.l.currentToken.kind == tkPrivate:
        case p.l.currentToken.kind:
          of tkPublic: access = AccessSpecifier.asPublic
          of tkProtected: access = AccessSpecifier.asProtected
          of tkPrivate: access = AccessSpecifier.asPrivate
          else: discard
        consumeToken(p)

      # TODO: Parse 'virtual' keyword for base class

      if p.l.currentToken.kind != tkIdentifier: # Assuming base class is an identifier for now
        p.errors.add("Expected base class name after access specifier (or directly after ':')")
        return nil
      let baseNameIdent = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
      consumeToken(p)
      # TODO: Handle template arguments for base class name?
      bases.add(InheritanceInfo(baseName: baseNameIdent, access: access, line: baseNameIdent.line, col: baseNameIdent.col)) # Use InheritanceInfo

    while p.l.currentToken.kind == tkComma:
      consumeToken(p) # Consume ','
      block parseNextBase:
        var access = AccessSpecifier.asDefault
        if p.l.currentToken.kind == tkPublic or p.l.currentToken.kind == tkProtected or p.l.currentToken.kind == tkPrivate:
          case p.l.currentToken.kind:
            of tkPublic: access = AccessSpecifier.asPublic
            of tkProtected: access = AccessSpecifier.asProtected
            of tkPrivate: access = AccessSpecifier.asPrivate
            else: discard
          consumeToken(p)

        if p.l.currentToken.kind != tkIdentifier:
          p.errors.add("Expected base class name after access specifier (or ',')")
          return nil
        let baseNameIdent = newIdentifier(p.l.currentToken.literal, p.l.currentToken.line, p.l.currentToken.col)
        consumeToken(p)
        bases.add(InheritanceInfo(baseName: baseNameIdent, access: access, line: baseNameIdent.line, col: baseNameIdent.col)) # Use InheritanceInfo

  # Check for class body or forward declaration
  if p.l.currentToken.kind == tkLBrace:
    consumeToken(p) # Consume '{'
    var members: seq[Statement] = @[] # Class members are statements (declarations, definitions, etc.)
    var currentAccess = if keyword == tkClass: AccessSpecifier.asPrivate else: AccessSpecifier.asPublic # Default access

    while p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
      # Check for access specifiers
      if p.l.currentToken.kind == tkPublic or p.l.currentToken.kind == tkProtected or p.l.currentToken.kind == tkPrivate:
        case p.l.currentToken.kind:
          of tkPublic: currentAccess = AccessSpecifier.asPublic
          of tkProtected: currentAccess = AccessSpecifier.asProtected
          of tkPrivate: currentAccess = AccessSpecifier.asPrivate
          else: discard
        consumeToken(p) # Consume access specifier
        if p.l.currentToken.kind != tkColon:
          p.errors.add("Expected ':' after access specifier")
          # Attempt recovery: skip token and continue parsing members
          consumeToken(p)
          continue
        consumeToken(p) # Consume ':'
        continue # Continue to parse next member with updated access

      # Parse member statement (declaration, definition, etc.)
      # Use parseStatement which handles various kinds of statements/declarations
      let memberStmt = p.parseStatement()
      if memberStmt != nil:
        # Assign access specifier to the member
        # Assuming VariableDeclaration and FunctionDefinition have 'access' field
        if memberStmt of VariableDeclaration:
          VariableDeclaration(memberStmt).access = currentAccess
        elif memberStmt of FunctionDefinition:
          FunctionDefinition(memberStmt).access = currentAccess
        # TODO: Add cases for other member types (nested classes, enums, etc.)
        # else:
        #   p.errors.add("Assigning access specifier to member type " & $memberStmt.kind & " not implemented yet.")
        members.add(memberStmt)
      else:
        # parseStatement failed, attempt recovery within the class body
        if p.l.currentToken.kind != tkRBrace and p.l.currentToken.kind != tkEndOfFile:
          p.errors.add("Skipping token inside class body after failed member parse: " & $p.l.currentToken.kind & " at " & $p.l.currentToken.line & ":" & $p.l.currentToken.col)
          consumeToken(p)

    if p.l.currentToken.kind != tkRBrace:
      p.errors.add("Expected '}' to close class definition")
      return nil
    consumeToken(p) # Consume '}'

    # Optional semicolon after class definition
    if p.l.currentToken.kind == tkSemicolon:
      consumeToken(p)

    # Use 'kind: keyword' instead of 'isStruct'
    return ClassDefinition(
      kind: keyword, name: className, isFinal: false, # TODO: Parse 'final'
      bases: bases, members: members,
      line: line, col: col, attributes: attributes # Assign attributes
    )
  elif p.l.currentToken.kind == tkSemicolon:
    # Forward declaration: class MyClass; or struct MyStruct;
    if className == nil:
      p.errors.add("Forward declaration requires a class/struct name")
      return nil
    consumeToken(p) # Consume ';'
    # TODO: Need a specific AST node for forward declarations?
    # Returning ClassDefinition with empty members/bases for now.
    p.errors.add("Class forward declaration parsed, using ClassDefinition node as placeholder.")
    # Use 'kind: keyword' instead of 'isStruct'
    return ClassDefinition(
        kind: keyword, name: className, isFinal: false,
        bases: @[], members: @[],
        line: line, col: col, attributes: attributes # Assign attributes
    )
  else:
    p.errors.add("Expected '{' for class body or ';' for forward declaration, got " & $p.l.currentToken.kind)
    return nil
