# src/nifcpp/transformer.nim
# Transforms C++ AST (from parser.nim) directly into NIF using nifbuilder

import std/options # Re-add options import
import std/tables
import std/strutils
import ./ast as cpp_ast         # C++ AST definition
import ./lexer                  # Directly import lexer for TokenType access
import ../nimony/nimony_model # For NIF tags/enums like NimonyStmt, etc.
import ../lib/nifbuilder      # Import the NIF builder

type
  TransformerContext* = ref object # Use ref object for potential shared state
    errors*: seq[string]
    # symTable: SymbolTable # Placeholder for future symbol table integration

proc newTransformerContext*(): TransformerContext =
  new(result)
  result.errors = @[]

# Forward declarations for recursive calls using the new signature
proc transformNode*(node: cpp_ast.CppAstNode, b: var Builder, ctx: TransformerContext, accessPragma: string = "") # Added accessPragma
proc transformType*(typeNode: cpp_ast.TypeExpr, b: var Builder, ctx: TransformerContext)

# --- NEW: Access Specifier Mapping ---
proc mapAccessSpecifier*(spec: cpp_ast.AccessSpecifier): string =
  case spec
  of cpp_ast.asPublic: "public"
  of cpp_ast.asProtected: "protected"
  of cpp_ast.asPrivate: "private"
  of cpp_ast.asDefault: "" # Use asDefault for the unspecified case

# --- Operator Mapping ---
# Use lexer.TokenType directly due to import issues
proc mapBinaryOperator*(op: lexer.TokenType): string = # Use lexer.TokenType
  case op
  of lexer.tkPlus: "+" # Use lexer.tk*
  of lexer.tkMinus: "-" # Use lexer.tk*
  of lexer.tkStar: "*" # Use lexer.tk*
  of lexer.tkSlash: "/" # Use lexer.tk*
  of lexer.tkPercent: "%" # Use lexer.tk*
  of lexer.tkEqual: "==" # Use lexer.tk*
  of lexer.tkNotEqual: "!=" # Use lexer.tk*
  of lexer.tkLess: "<" # Use lexer.tk*
  of lexer.tkLessEqual: "<=" # Use lexer.tk*
  of lexer.tkGreater: ">" # Use lexer.tk*
  of lexer.tkGreaterEqual: ">=" # Use lexer.tk*
  of lexer.tkLogicalAnd: "and" # Use lexer.tk*
  of lexer.tkLogicalOr: "or"  # Use lexer.tk*
  of lexer.tkBitwiseAnd: "and" # Use lexer.tk*
  of lexer.tkBitwiseOr: "or"   # Use lexer.tk*
  of lexer.tkBitwiseXor: "xor" # Use lexer.tk*
  of lexer.tkLeftShift: "shl" # Use lexer.tk*
  of lexer.tkRightShift: "shr" # Use lexer.tk*
  of lexer.tkAssign: "=" # Use lexer.tk*
  of lexer.tkPlusAssign: "+=" # Use lexer.tk*
  of lexer.tkMinusAssign: "-=" # Use lexer.tk*
  of lexer.tkMulAssign: "*=" # Use lexer.tk*
  of lexer.tkDivAssign: "/=" # Use lexer.tk*
  of lexer.tkModAssign: "%=" # Use lexer.tk*
  of lexer.tkAndAssign: "and=" # Use lexer.tk*
  of lexer.tkOrAssign: "or=" # Use lexer.tk*
  of lexer.tkXorAssign: "xor=" # Use lexer.tk*
  of lexer.tkLShiftAssign: "shl=" # Use lexer.tk*
  of lexer.tkRShiftAssign: "shr=" # Use lexer.tk*
  else: "" # Error handled by caller

# Use lexer.TokenType directly due to import issues
proc mapUnaryOperator*(op: lexer.TokenType): string = # Use lexer.TokenType
  case op
  of lexer.tkMinus: "-"          # Use lexer.tk* -> NegX
  of lexer.tkLogicalNot: "not"   # Use lexer.tk* -> NotX
  of lexer.tkBitwiseNot: "bitnot" # Use lexer.tk* -> BitnotX (Distinguish from logical not)
  of lexer.tkIncrement: "inc"     # Use lexer.tk* -> No tag, keep string
  of lexer.tkDecrement: "dec"     # Use lexer.tk* -> No tag, keep string
  of lexer.tkStar: "deref"  # Use lexer.tk*
  of lexer.tkBitwiseAnd: "addr" # Use lexer.tk*
  else: "" # Error handled by caller

# --- Type Transformation ---
# Note: Now takes 'var Builder' and doesn't return a node
proc transformType*(typeNode: cpp_ast.TypeExpr, b: var Builder, ctx: TransformerContext) =
  if typeNode == nil:
    b.addEmpty() # Add empty node representation
    return
  b.addLineInfo(typeNode.col.int32, typeNode.line.int32) # Attempt to add line info, cast to int32

  # Use if/elif/else with 'of' for runtime type checking
  if typeNode of cpp_ast.BaseTypeExpr:
    let cppBase = cpp_ast.BaseTypeExpr(typeNode)
    let nimTypeName = case cppBase.name.normalize(): # Inner case for string value is fine
      of "void": "void" # NIF might use '.' for void? Check nimony conventions. Using "void" ident for now.
      of "bool": "bool"
      of "char": "char"
      of "int": "cint"
      of "short": "cshort"
      of "long": "clong"
      of "long long": "clonglong" # Added
      of "unsigned char": "cuint8" # Added (assuming cuint8 is appropriate)
      of "unsigned short": "cushort" # Added
      of "unsigned int": "cuint" # Added
      of "unsigned long": "culong" # Added
      of "unsigned long long": "culonglong" # Added
      of "float": "cfloat"
      of "double": "cdouble"
      of "long double": "clongdouble" # Added
      of "wchar_t": "cwchar" # Added
      of "char8_t": "char8" # Added (Mapping to Nim's char8)
      of "char16_t": "char16" # Added (Mapping to Nim's char16)
      of "char32_t": "char32" # Added (Mapping to Nim's char32)
      else: cppBase.name # Use original name for other types (structs, classes, etc.)
    b.addIdent(nimTypeName) # Add identifier directly to builder
  elif typeNode of cpp_ast.PointerTypeExpr:
    let cppPtr = cpp_ast.PointerTypeExpr(typeNode)
    # Doc suggests PtrT, using $NimonyType.PtrT
    b.withTree($NimonyType.PtrT):
      transformType(cppPtr.baseType, b, ctx)
  elif typeNode of cpp_ast.ReferenceTypeExpr:
    let cppRef = cpp_ast.ReferenceTypeExpr(typeNode)
    # Map to "var" type
    b.withTree("var"):
      # Add const pragma if it's a reference to const (const T&)
      if cppRef.isConst:
        b.withTree("pragma"): b.addIdent("const")
      transformType(cppRef.baseType, b, ctx)
  elif typeNode of cpp_ast.ConstQualifiedTypeExpr:
    let cppConst = cpp_ast.ConstQualifiedTypeExpr(typeNode)
    # Nim doesn't have top-level const qualifiers in the same way.
    # Transform the base type and add a 'const' pragma.
    transformType(cppConst.baseType, b, ctx)
    b.withTree("pragma"): b.addIdent("const") # Add const pragma
  elif typeNode of cpp_ast.VolatileQualifiedTypeExpr:
    let cppVolatile = cpp_ast.VolatileQualifiedTypeExpr(typeNode)
    # Nim doesn't have a direct 'volatile' type qualifier. Add a pragma.
    transformType(cppVolatile.baseType, b, ctx)
    b.withTree("pragma"): b.addIdent("volatile") # Add volatile pragma
    ctx.errors.add("Warning: C++ 'volatile' qualifier mapped to pragma; review semantics. At " & $typeNode.line & ":" & $typeNode.col)
  elif typeNode of cpp_ast.ArrayTypeExpr:
    let cppArray = cpp_ast.ArrayTypeExpr(typeNode)
    # Use NIF tag for array type (e.g., ArrayT from nimony_model)
    # Doc suggests ArrayT, using $NimonyType.ArrayT
    b.withTree($NimonyType.ArrayT):
      transformType(cppArray.elementType, b, ctx)
      if cppArray.size != nil:
        transformNode(cppArray.size, b, ctx) # Size is an expression
      else:
        b.addEmpty() # Placeholder for unspecified size
  elif typeNode of cpp_ast.FunctionTypeExpr:
    let cppFuncType = cpp_ast.FunctionTypeExpr(typeNode)
    # Doc suggests ParamsT?, using $NimonyType.ParamsT
    b.withTree($NimonyType.ParamsT): # Use string representation of the enum value
      # Return type first
      transformType(cppFuncType.returnType, b, ctx)
      # Then parameters
      for param in cppFuncType.parameters:
        # Doc suggests ParamU, which exists in NimonyOther. Using ParamU.
        # Structure: (ParamU name type default)
        b.withTree($NimonyOther.ParamU):
          transformNode(param.name, b, ctx) # Parameter name might be optional in type
          transformType(param.paramType, b, ctx)
          # defaultValue is commented out in ast.nim, so always add empty for now
          b.addEmpty() # Placeholder for no default value
          # if param.defaultValue != nil:
          #    transformNode(param.defaultValue, b, ctx)
          # else:
          #    b.addEmpty()
      # Add PragmasU node *after* parameters
      b.withTree($NimonyOther.PragmasU):
        if cppFuncType.isConst: b.addIdent("constFunc")
        if cppFuncType.isVolatile: b.addIdent("volatileFunc")
        if cppFuncType.refQualifier == cpp_ast.rqLValue: b.addIdent("refLValueFunc")
        elif cppFuncType.refQualifier == cpp_ast.rqRValue: b.addIdent("refRValueFunc")
        if cppFuncType.isNoexcept: b.addIdent("noexcept")

  elif typeNode of cpp_ast.DecltypeSpecifier:
    let cppDecltype = cpp_ast.DecltypeSpecifier(typeNode)
    b.withTree($NimonyExpr.TypeOfX):
      transformNode(cppDecltype.expression, b, ctx) # Use transformNode
  else:
    # Use repr() for a detailed string representation including type
    ctx.errors.add("Unsupported C++ TypeExpr kind: " & repr(typeNode) & " at " & $typeNode.line & ":" & $typeNode.col)
    b.addEmpty() # Add empty node on error/unsupported

# --- Main Transformation Logic ---

# Basic implementation for transformNode using the new signature
proc transformNode*(node: cpp_ast.CppAstNode, b: var Builder, ctx: TransformerContext, accessPragma: string = "") = # Added accessPragma
  if node == nil:
    # Don't add anything for nil nodes unless context requires an empty placeholder
    # b.addEmpty() # Optional: add empty if needed
    return
  b.addLineInfo(node.col.int32, node.line.int32) # Attempt to add line info, cast to int32

  # Dispatch based on the C++ AST node type using if/elif/else
  if node of cpp_ast.Identifier:
    let cppIdent = cpp_ast.Identifier(node)
    b.addIdent(cppIdent.name)
  elif node of cpp_ast.IntegerLiteral:
    let cppLit = cpp_ast.IntegerLiteral(node)
    b.addIntLit(cppLit.value)
  elif node of cpp_ast.FloatLiteral:
    let cppLit = cpp_ast.FloatLiteral(node)
    b.addFloatLit(cppLit.value)
  elif node of cpp_ast.StringLiteral:
    let cppLit = cpp_ast.StringLiteral(node)
    b.addStrLit(cppLit.value)
  elif node of cpp_ast.CharLiteral:
     let cppLit = cpp_ast.CharLiteral(node)
     b.addCharLit(cppLit.value) # Builder handles escaping

  # Expressions
  elif node of cpp_ast.InitializerListExpr:
    let cppList = cpp_ast.InitializerListExpr(node)
    # Use NIF tag for untyped array/aggregate constructor (e.g., BracketX)
    b.withTree($NimonyExpr.BracketX): # Use string representation of the enum value
      for value in cppList.values:
        transformNode(value, b, ctx)

  elif node of cpp_ast.BinaryExpression:
    let cppBin = cpp_ast.BinaryExpression(node)
    # Use lexer.TokenType directly here as well
    let opStr = mapBinaryOperator(lexer.TokenType(cppBin.op)) # Cast might be needed if op is not already lexer.TokenType
    if opStr.len == 0:
      ctx.errors.add("Unsupported binary operator: " & $cppBin.op & " at " & $node.line & ":" & $node.col)
      b.withTree("error"): b.addStrLit("Unsupported binary op: " & $cppBin.op) # Add error node
      return

    # Handle simple assignment separately as it's a statement (AsgnS)
    if cppBin.op == lexer.tkAssign:
      b.withTree($NimonyStmt.AsgnS):
        transformNode(cppBin.left, b, ctx)
        transformNode(cppBin.right, b, ctx)
    else:
      # Map other binary operators to NimonyExpr tags
      # Note: Compound assignments are handled separately below. Logical vs Bitwise is handled.
      let exprTag: string = case cppBin.op # Use cppBin.op directly
                      of lexer.tkPlus: $NimonyExpr.AddX
                      of lexer.tkMinus: $NimonyExpr.SubX
                      of lexer.tkStar: $NimonyExpr.MulX
                      of lexer.tkSlash: $NimonyExpr.DivX
                      of lexer.tkPercent: $NimonyExpr.ModX
                      of lexer.tkEqual: $NimonyExpr.EqX
                      of lexer.tkNotEqual: $NimonyExpr.NeqX # Use NeqX
                      of lexer.tkLess: $NimonyExpr.LtX
                      of lexer.tkLessEqual: $NimonyExpr.LeX
                      of lexer.tkGreater: $NimonyExpr.LtX # Map > to < with swapped operands
                      of lexer.tkGreaterEqual: $NimonyExpr.LeX # Map >= to <= with swapped operands
                      of lexer.tkLogicalAnd: $NimonyExpr.AndX
                      of lexer.tkLogicalOr: $NimonyExpr.OrX
                      of lexer.tkBitwiseAnd: $NimonyExpr.BitandX # Use BitandX
                      of lexer.tkBitwiseOr: $NimonyExpr.BitorX   # Use BitorX
                      of lexer.tkBitwiseXor: $NimonyExpr.BitxorX # Use BitxorX
                      of lexer.tkLeftShift: $NimonyExpr.ShlX
                      of lexer.tkRightShift: $NimonyExpr.ShrX
                      # Compound assignments are handled below, this case shouldn't be reached for them.
                      else: opStr # Fallback for any unexpected cases

      # Handle compound assignments by transforming into (AsgnS a (opX a b))
      case cppBin.op
      of lexer.tkPlusAssign, lexer.tkMinusAssign, lexer.tkMulAssign, lexer.tkDivAssign,
         lexer.tkModAssign, lexer.tkAndAssign, lexer.tkOrAssign, lexer.tkXorAssign,
         lexer.tkLShiftAssign, lexer.tkRShiftAssign:
        b.withTree($NimonyStmt.AsgnS): # Outer assignment
          transformNode(cppBin.left, b, ctx) # Left operand (target of assignment)
          # Inner binary operation
          let innerOpTag = case cppBin.op
                           of lexer.tkPlusAssign: $NimonyExpr.AddX
                           of lexer.tkMinusAssign: $NimonyExpr.SubX
                           of lexer.tkMulAssign: $NimonyExpr.MulX
                           of lexer.tkDivAssign: $NimonyExpr.DivX
                           of lexer.tkModAssign: $NimonyExpr.ModX
                           of lexer.tkAndAssign: $NimonyExpr.BitandX
                           of lexer.tkOrAssign: $NimonyExpr.BitorX
                           of lexer.tkXorAssign: $NimonyExpr.BitxorX
                           of lexer.tkLShiftAssign: $NimonyExpr.ShlX
                           of lexer.tkRShiftAssign: $NimonyExpr.ShrX
                           else: "" # Should not happen
          if innerOpTag.len > 0:
            b.withTree(innerOpTag):
              transformNode(cppBin.left, b, ctx)  # Left operand (used in operation)
              transformNode(cppBin.right, b, ctx) # Right operand
          else:
            # Error case - should have been caught earlier or is an internal error
            b.withTree("error"): b.addStrLit("Internal error mapping compound assignment")
      else:
        # Handle regular binary expressions (non-assignment)
        b.withTree(exprTag):
          if cppBin.op == lexer.tkGreater or cppBin.op == lexer.tkGreaterEqual:
            # Swap operands for > and >=
            transformNode(cppBin.right, b, ctx)
            transformNode(cppBin.left, b, ctx)
          else:
            transformNode(cppBin.left, b, ctx)
            transformNode(cppBin.right, b, ctx)

  elif node of cpp_ast.UnaryExpression:
    let cppUnary = cpp_ast.UnaryExpression(node)
    # Use lexer.TokenType directly here as well
    let opStr = mapUnaryOperator(lexer.TokenType(cppUnary.op)) # Cast might be needed
    if opStr.len == 0:
      ctx.errors.add("Unsupported unary operator: " & $cppUnary.op & " at " & $node.line & ":" & $node.col)
      b.withTree("error"): b.addStrLit("Unsupported unary op: " & $cppUnary.op)
      return

    # Map operator string to NimonyExpr tag
    let exprTag = case opStr
      of "-": $NimonyExpr.NegX
      of "not": $NimonyExpr.NotX
      of "bitnot": $NimonyExpr.BitnotX # Use specific tag for bitwise not
      of "inc": "inc" # Keep as string, no specific NIF tag
      of "dec": "dec" # Keep as string, no specific NIF tag
      of "deref": $NimonyExpr.DerefX
      of "addr": $NimonyExpr.AddrX
      else: opStr # Fallback (shouldn't happen for valid unary ops)

    # Handle inc/dec as calls, others as direct tags
    if exprTag == "inc" or exprTag == "dec":
      # Map prefix ++/-- to (call inc/dec operand)
      b.withTree($NimonyExpr.CallX):
        b.addIdent(exprTag) # Add "inc" or "dec" identifier
        transformNode(cppUnary.operand, b, ctx)
    else:
      # Handle other unary operators like NegX, NotX, DerefX, AddrX
      b.withTree(exprTag):
        transformNode(cppUnary.operand, b, ctx) # THIS IS LINE 223 - Ensure 2 spaces indent

    # Add warning for postfix ++/-- regardless of mapping, as semantics differ
    if not cppUnary.isPrefix and (cppUnary.op == lexer.tkIncrement or cppUnary.op == lexer.tkDecrement):
      ctx.errors.add("Warning: Postfix ++/-- transformed like prefix operation; value semantics differ. At " & $node.line & ":" & $node.col)

  elif node of cpp_ast.CallExpression:
    let cppCall = cpp_ast.CallExpression(node)
    # Doc suggests CallX
    b.withTree($NimonyExpr.CallX): # Use NIF tag for call
      transformNode(cppCall.function, b, ctx)
      for arg in cppCall.arguments:
        transformNode(arg, b, ctx)

  elif node of cpp_ast.MemberAccessExpression:
    let cppMember = cpp_ast.MemberAccessExpression(node)
    let memberIdent = cpp_ast.Identifier(cppMember.member) # Member is always an Identifier
    # Doc suggests DotX
    if cppMember.isArrow:
        # obj->member becomes (DotX (DerefX obj) member)
        b.withTree($NimonyExpr.DotX): # NIF tag for dot access
          b.withTree($NimonyExpr.DerefX): # NIF tag for dereference (assuming DerefX)
             transformNode(cppMember.targetObject, b, ctx)
          b.addIdent(memberIdent.name) # Add member name
        ctx.errors.add("Warning: C++ '->' operator mapped to Nim '(dot (deref ... ) ...)'; review semantics. At " & $node.line & ":" & $node.col)
    else:
        # obj.member becomes (DotX obj member)
        b.withTree($NimonyExpr.DotX):
          transformNode(cppMember.targetObject, b, ctx)
          b.addIdent(memberIdent.name)

  elif node of cpp_ast.ArraySubscriptExpression:
    let cppSub = cpp_ast.ArraySubscriptExpression(node)
    # Doc suggests BracketX
    b.withTree($NimonyExpr.BracketX): # Use NIF tag for bracket access
      transformNode(cppSub.array, b, ctx)
      transformNode(cppSub.index, b, ctx)

  # Statements
  elif node of cpp_ast.ThrowExpression: # Correct indentation for elif
    let cppThrow = cpp_ast.ThrowExpression(node)
    # Use NIF tag for raise statement (e.g., RaiseS)
    b.withTree($NimonyStmt.RaiseS): # Use string representation of the enum value
      if cppThrow.expression != nil:
        transformNode(cppThrow.expression, b, ctx) # Transform the thrown expression
      else:
        b.addEmpty() # Placeholder for rethrow (throw;)

  elif node of cpp_ast.ExpressionStatement:
    let cppStmt = cpp_ast.ExpressionStatement(node)
    # Directly transform the expression. NIF structure might handle expr-as-stmt.
    transformNode(cppStmt.expression, b, ctx)

  elif node of cpp_ast.BlockStatement:
    let cppBlock = cpp_ast.BlockStatement(node)
    # Use NIF tag for statement list (e.g., "stmts")
    # Check nimony_model for the correct tag (e.g., StmtsS)
    b.withTree($NimonyStmt.StmtsS): # Use string representation of the enum value
      for stmt in cppBlock.statements:
        transformNode(stmt, b, ctx) # Pass empty accessPragma for nested blocks

  elif node of cpp_ast.VariableDeclaration:
     let cppDecl = cpp_ast.VariableDeclaration(node)
     # Doc suggests VarS / LetS. Using VarS for now.
     b.withTree($NimonyStmt.VarS): # Use string representation of the enum value
       # Add access pragma if provided
       if accessPragma.len > 0:
         b.withTree("pragma"): b.addIdent(accessPragma) # Using string "pragma"

       transformNode(cppDecl.name, b, ctx)
       transformType(cppDecl.varType, b, ctx)
       if cppDecl.initializer != nil:
         transformNode(cppDecl.initializer, b, ctx)
       else:
         b.addEmpty() # Placeholder for no initializer

  elif node of cpp_ast.ReturnStatement:
     let cppRet = cpp_ast.ReturnStatement(node)
     # Use NIF tag for return statement (e.g., RetS)
     b.withTree($NimonyStmt.RetS): # Use string representation of the enum value
       if cppRet.returnValue != nil:
         transformNode(cppRet.returnValue, b, ctx)
       else:
         b.addEmpty() # Placeholder for void return

  elif node of cpp_ast.IfStatement:
    let cppIf = cpp_ast.IfStatement(node)
    # Use NIF tag for if statement (e.g., IfS)
    b.withTree($NimonyStmt.IfS): # Use string representation of the enum value
      # Condition
      b.withTree("branch"): # Assuming structure like (if (branch cond then) (branch . else))
        transformNode(cppIf.condition, b, ctx)
        transformNode(cppIf.thenBranch, b, ctx)
      # Else branch
      if cppIf.elseBranch != nil:
         b.withTree("branch"):
           b.addEmpty() # Placeholder for condition in else
           transformNode(cppIf.elseBranch, b, ctx)

  elif node of cpp_ast.WhileStatement:
    let cppWhile = cpp_ast.WhileStatement(node)
    # Use NIF tag for while statement (e.g., WhileS)
    b.withTree($NimonyStmt.WhileS): # Use string representation of the enum value
      transformNode(cppWhile.condition, b, ctx)
      transformNode(cppWhile.body, b, ctx)

  elif node of cpp_ast.ForStatement:
    let cppFor = cpp_ast.ForStatement(node)
    # Transform C++ for into Nim block + while loop structure
    b.withTree($NimonyStmt.BlockS): # Outer block for initializer scope
      # Initializer statement
      if cppFor.initializer != nil:
        transformNode(cppFor.initializer, b, ctx)
      else:
        b.addEmpty() # Placeholder if no initializer

      # While loop
      b.withTree($NimonyStmt.WhileS):
        # Condition (use 'true' literal if condition is empty)
        if cppFor.condition != nil:
          transformNode(cppFor.condition, b, ctx)
        else:
          b.addIdent("true") # Assuming 'true' identifier for empty condition

        # Body of the while loop (original body + update)
        b.withTree($NimonyStmt.StmtsS):
          # Original body
          if cppFor.body != nil: # Body might be empty statement
             transformNode(cppFor.body, b, ctx)
          else:
             b.addEmpty() # Placeholder for empty body statement

          # Update expression (transformed as a statement)
          if cppFor.update != nil:
            transformNode(cppFor.update, b, ctx) # Transform update expr as statement
          else:
            b.addEmpty() # Placeholder if no update expression

  elif node of cpp_ast.RangeForStatement: # NEW: Handle Range-based For
    let cppRangeFor = cpp_ast.RangeForStatement(node)
    # Map to ForS: (ForS decl range body)
    b.withTree($NimonyStmt.ForS): # Assuming ForS tag exists
      # Declaration (is a VariableDeclaration statement)
      transformNode(cppRangeFor.declaration, b, ctx)
      # Range Expression
      transformNode(cppRangeFor.rangeExpr, b, ctx)
      # Body
      transformNode(cppRangeFor.body, b, ctx)

  elif node of cpp_ast.BreakStatement: # NEW: Handle Break
    b.addIdent($NimonyStmt.BreakS) # Simple tag, use addIdent

  elif node of cpp_ast.ContinueStatement: # NEW: Handle Continue
    b.addIdent($NimonyStmt.ContinueS) # Simple tag, use addIdent

  elif node of cpp_ast.SwitchStatement: # NEW: Handle Switch
    let cppSwitch = cpp_ast.SwitchStatement(node)
    # Map to CaseS: (CaseS expr (OfU val body)... (OfU . defaultBody)?)
    b.withTree($NimonyStmt.CaseS):
      transformNode(cppSwitch.controlExpr, b, ctx)
      # Process the body block to extract case/default labels and group statements
      if cppSwitch.body != nil and cppSwitch.body of cpp_ast.BlockStatement:
        let bodyBlock = cpp_ast.BlockStatement(cppSwitch.body)
        var currentCaseValue: cpp_ast.Expression = nil # Track current case value (nil for default)
        var currentStatements: seq[cpp_ast.Statement] = @[]
        var isDefaultCase = false

        # Pass b and ctx explicitly to avoid capture issues
        proc flushCase(b: var Builder, ctx: TransformerContext) =
          if currentStatements.len > 0:
            b.withTree($NimonyOther.OfU): # Use OfU tag
              if isDefaultCase:
                b.addEmpty() # Placeholder for default case value
              elif currentCaseValue != nil:
                transformNode(currentCaseValue, b, ctx)
              else:
                b.addEmpty() # Should not happen if not default? Error case.
              # Add the grouped statements
              b.withTree($NimonyStmt.StmtsS):
                for stmt in currentStatements:
                  transformNode(stmt, b, ctx)
            currentStatements = @[] # Reset for next case

        for stmt in bodyBlock.statements:
          if stmt of cpp_ast.CaseStatement:
            flushCase(b, ctx) # Pass b and ctx
            let caseStmt = cpp_ast.CaseStatement(stmt)
            currentCaseValue = caseStmt.value
            isDefaultCase = (currentCaseValue == nil)
          else:
            currentStatements.add(stmt)
        flushCase(b, ctx) # Flush the last case
      else:
        ctx.errors.add("Switch statement body is not a valid block at " & $node.line & ":" & $node.col)
        b.addEmpty() # Add empty placeholder for body

  # Note: CaseStatement itself is handled within SwitchStatement logic

  elif node of cpp_ast.FunctionDefinition:
    let cppFunc = cpp_ast.FunctionDefinition(node)
    b.withTree($NimonyStmt.ProcS): # Use string representation of the enum value
      # Add access pragma if provided
      if accessPragma.len > 0:
        b.withTree("pragma"): b.addIdent(accessPragma) # Using string "pragma"

      transformNode(cppFunc.name, b, ctx) # Add function name
      # Parameters and Return Type
      b.withTree($NimonyOther.ParamsU):
        transformType(cppFunc.returnType, b, ctx) # Add return type first
        for param in cppFunc.parameters:
           b.withTree($NimonyOther.ParamU):
             transformNode(param.name, b, ctx)
             transformType(param.paramType, b, ctx)
             # Handle default value
             if param.defaultValue != nil:
                transformNode(param.defaultValue, b, ctx)
             else:
                b.addEmpty()
      # Pragmas (Add empty PragmasU node as placeholder)
      b.withTree($NimonyOther.PragmasU): discard # Empty pragmas for now
      # Body
      transformNode(cppFunc.body, b, ctx)

  # --- NEW: Template Handling ---
  elif node of cpp_ast.TemplateDecl:
    let cppTmpl = cpp_ast.TemplateDecl(node)
    # Map to TemplateS: (TemplateS (TypevarU ...) ... declaration)
    b.withTree($NimonyStmt.TemplateS): # Assuming TemplateS tag exists
      # Transform template parameters first
      for param in cppTmpl.parameters:
        transformNode(param, b, ctx) # Recursively call transformNode for TemplateParameter
      # Then transform the actual declaration (class, function, etc.)
      transformNode(cppTmpl.declaration, b, ctx)

  elif node of cpp_ast.TemplateParameter:
    let cppParam = cpp_ast.TemplateParameter(node)
    # Map to TypevarU: (TypevarU name type default?)
    b.withTree($NimonyOther.TypevarU): # Assuming TypevarU tag exists
      # Name (might be nil for non-type params in some contexts?)
      if cppParam.name != nil:
        transformNode(cppParam.name, b, ctx)
      else:
        b.addEmpty() # Placeholder for no name

      # Type (only for tpType kind) or Kind indicator
      case cppParam.kind
      of cpp_ast.tpType: # Non-type parameter
        if cppParam.paramType != nil:
          transformType(cppParam.paramType, b, ctx)
        else:
          ctx.errors.add("Missing type for non-type template parameter at " & $node.line & ":" & $node.col)
          b.addEmpty()
      of cpp_ast.tpTypename: # Type parameter (class/typename)
        # Represent 'typename' or 'class' conceptually. Maybe add a pragma?
        # For now, add an empty node as the 'type' placeholder for type params.
        b.addEmpty()
        b.withTree("pragma"): b.addIdent("typename") # Indicate it's a type parameter
      of cpp_ast.tpTemplate: # Template template parameter
        # No specific 'type' node for the template itself.
        # Add pragma to indicate kind.
        b.withTree("pragma"): b.addIdent("templateTemplate")
        # Recursively transform inner template parameters
        # Check if the sequence has elements using .len > 0
        if cppParam.templateParams.len > 0:
          for innerParam in cppParam.templateParams:
            transformNode(innerParam, b, ctx)
        else:
          # Add an empty node if there are no inner params, maybe?
          # Or just omit? Let's omit for now.
          discard

      # Default value/type (Applies to the outer template parameter)
      if cppParam.defaultValue != nil:
        transformNode(cppParam.defaultValue, b, ctx)
      else:
        b.addEmpty()

  # --- NEW: Namespace Handling ---
  elif node of cpp_ast.NamespaceDef:
    let cppNs = cpp_ast.NamespaceDef(node)
    # Map to ModuleY: (ModuleY name? declarations...)
    b.withTree($ModuleY): # Use the correct tag directly
      # Add name if not anonymous
      if cppNs.name != nil:
        transformNode(cppNs.name, b, ctx)
      else:
        b.addEmpty() # Placeholder for anonymous namespace name
      # Transform declarations within the namespace
      for decl in cppNs.declarations:
        transformNode(decl, b, ctx) # Pass empty accessPragma

  # --- NEW: Translation Unit Handling ---
  elif node of cpp_ast.TranslationUnit:
    let cppUnit = cpp_ast.TranslationUnit(node)
    # Top-level NIF structure might be implicit or a specific root node.
    # For now, just transform all declarations sequentially.
    # A root 'stmts' or similar might be needed depending on NIF conventions.
    # Using StmtsS for now as a container.
    b.withTree($NimonyStmt.StmtsS):
      for decl in cppUnit.declarations:
        transformNode(decl, b, ctx) # Pass empty accessPragma for top-level

  # --- NEW: Class/Struct Definition Handling ---
  elif node of cpp_ast.ClassDefinition:
    let cppClass = cpp_ast.ClassDefinition(node)
    # Map to TypeS: (TypeS name (ObjectT pragmas bases members...))
    b.withTree($NimonyStmt.TypeS):
      transformNode(cppClass.name, b, ctx) # Class name
      b.withTree($NimonyType.ObjectT): # Object definition
        # Add PragmasU for kind and final
        b.withTree($NimonyOther.PragmasU):
          if cppClass.kind == lexer.tkClass: b.addIdent("class") # Use lexer.tkClass
          elif cppClass.kind == lexer.tkStruct: b.addIdent("struct") # Use lexer.tkStruct
          # Add other kinds like union if needed
          if cppClass.isFinal: b.addIdent("final")

        # Add base classes if any
        if cppClass.bases.len > 0:
          b.withTree("bases"): # Assuming a 'bases' node container
            for baseInfo in cppClass.bases:
              # Structure: (kv access baseName)
              b.withTree("kv"): # Assuming 'kv' for key-value pair
                let accessStr = mapAccessSpecifier(baseInfo.access)
                if accessStr.len > 0:
                  b.addIdent(accessStr)
                else:
                  # Default access depends on class/struct kind
                  let defaultAccess = if cppClass.kind == lexer.tkClass: "private" else: "public" # Use lexer.tkClass
                  b.addIdent(defaultAccess)
                transformNode(baseInfo.baseName, b, ctx) # Base class name

        # Add members. Assume parser sets the 'access' field on member nodes correctly.
        # The transformation logic for VariableDeclaration, FunctionDefinition, etc.,
        # should read the 'access' field from the node itself.
        for member in cppClass.members:
            # Removed the check for non-existent AccessSpecifierNode and tracking of currentAccess.
            # Rely on individual member node transformations to handle their access level.
            transformNode(member, b, ctx) # Pass default accessPragma

  # --- NEW: Enum Definition Handling ---
  elif node of cpp_ast.EnumDefinition:
    let cppEnum = cpp_ast.EnumDefinition(node)
    # Map to TypeS: (TypeS name? (EnumT pragmas members...))
    b.withTree($NimonyStmt.TypeS):
      # Add name if it exists
      if cppEnum.name != nil:
        transformNode(cppEnum.name, b, ctx)
      else:
        b.addEmpty() # Placeholder for anonymous enum name

      # Add EnumT node
      b.withTree($NimonyType.EnumT):
        # Add PragmasU for isClass and baseType
        b.withTree($NimonyOther.PragmasU):
          if cppEnum.isClass: b.addIdent("enumClass")
          if cppEnum.baseType != nil:
            b.withTree("baseType"): # Add a specific pragma/kv for base type?
              transformType(cppEnum.baseType, b, ctx)

        # Add enumerators
        for enumerator in cppEnum.enumerators:
          transformNode(enumerator, b, ctx)

  # --- NEW: Enumerator Handling ---
  elif node of cpp_ast.Enumerator:
    let cppEnumerator = cpp_ast.Enumerator(node)
    # Map to EfldU: (EfldU name value?) - Use the correct tag EfldU
    b.withTree($NimonyOther.EfldU): # Use the correct tag EfldU
      transformNode(cppEnumerator.name, b, ctx) # Enumerator name
      # Add value if it exists
      if cppEnumerator.value != nil:
        transformNode(cppEnumerator.value, b, ctx)
      else:
        b.addEmpty() # Placeholder for no explicit value

  # --- NEW: Type Alias Declaration Handling (typedef/using) ---
  elif node of cpp_ast.TypeAliasDeclaration:
    let cppAlias = cpp_ast.TypeAliasDeclaration(node)
    # Map to TypeS: (TypeS aliasName aliasedType (PragmasU))
    b.withTree($NimonyStmt.TypeS):
      transformNode(cppAlias.name, b, ctx) # The new alias name
      transformType(cppAlias.aliasedType, b, ctx) # The original type being aliased
      b.withTree($NimonyOther.PragmasU): discard # Add empty pragmas node

  # --- NEW: Try-Catch Statement Handling ---
  elif node of cpp_ast.TryStatement:
    let cppTry = cpp_ast.TryStatement(node)
    # Map to TryS: (TryS tryBody (ExceptU type? body)... (FinU body)?)
    b.withTree($NimonyStmt.TryS):
      # Transform the main try block
      transformNode(cppTry.tryBlock, b, ctx)
      # Transform each catch clause into an ExceptU
      for catchClause in cppTry.catchClauses:
        # Structure: (ExceptU type? body)
        b.withTree($NimonyOther.ExceptU):
          if catchClause.isCatchAll:
            b.addEmpty() # Placeholder for catch(...) type
          elif catchClause.exceptionDecl != nil:
            # Transform the exception declaration (VariableDeclaration)
            # We only need the type part for the ExceptU structure
            let decl = cpp_ast.VariableDeclaration(catchClause.exceptionDecl)
            transformType(decl.varType, b, ctx)
            # Note: The variable name (e.g., 'e' in catch(const std::exception& e))
            # is declared within the catch body's scope in C++,
            # but NIF's ExceptU might not directly support this.
            # We might need to inject the declaration into the body transformation.
            # For now, just transform the body directly.
            # Consider adding a warning or adjusting the body transformation later.
            ctx.errors.add("Warning: C++ catch variable declaration scope might differ in NIF. Review catch block for: " & $decl.name.name & " at " & $catchClause.line & ":" & $catchClause.col)
          else:
             # This case (non-catch-all but nil declaration) shouldn't happen based on parser logic?
             ctx.errors.add("Error: Invalid CatchClause structure (non-catch-all, nil declaration) at " & $catchClause.line & ":" & $catchClause.col)
             b.addEmpty() # Error placeholder

          # Transform the catch block body
          transformNode(catchClause.body, b, ctx)
      # Note: C++ doesn't have a direct 'finally' equivalent that maps cleanly here.
      # 'finally' in Nim is usually handled by 'defer' or RAII.

  # Note: CatchClause is handled within TryStatement, no separate top-level transform needed usually.
  # Adding a basic handler for completeness, though it might not be directly called.
  elif node of cpp_ast.CatchClause:
     let cppCatch = cpp_ast.CatchClause(node)
     ctx.errors.add("Warning: Direct transformation of CatchClause encountered outside TryStatement at " & $node.line & ":" & $node.col)
     # Generate a placeholder or error representation
     b.withTree("error"): b.addStrLit("Orphaned CatchClause")

  # --- NEW: Lambda Expression Handling ---
  elif node of cpp_ast.LambdaExpression:
    let cppLambda = cpp_ast.LambdaExpression(node)
    # Map to ProcS: (ProcS . (ParamsU retType (ParamU name type default)...) (PragmasU) body)
    b.withTree($NimonyStmt.ProcS):
      b.addEmpty() # Anonymous proc name

      # Parameters and Return Type
      b.withTree($NimonyOther.ParamsU):
        # Return type (might be nil if deduced)
        if cppLambda.returnType != nil:
          transformType(cppLambda.returnType, b, ctx)
        else:
          b.addEmpty() # Placeholder for deduced return type

        # Parameters
        for param in cppLambda.parameters:
           b.withTree($NimonyOther.ParamU):
             transformNode(param.name, b, ctx)
             transformType(param.paramType, b, ctx)
             # Handle default value (Lambdas usually don't have default args, but AST supports it)
             if param.defaultValue != nil:
                transformNode(param.defaultValue, b, ctx)
             else:
                b.addEmpty()

      # Pragmas (Empty for now, captures are not directly mapped)
      b.withTree($NimonyOther.PragmasU): discard

      # Body
      transformNode(cppLambda.body, b, ctx)

      # Add warning about captures
      if cppLambda.captureDefault.isSome or cppLambda.captures.len > 0:
        ctx.errors.add("Warning: C++ lambda captures are not directly translated to NIF structure. Review lambda body semantics at " & $node.line & ":" & $node.col)
