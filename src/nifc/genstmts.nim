#
#
#           NIFC Compiler
#        (c) Copyright 2024 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

# included from codegen.nim

proc genEmitStmt(c: var GeneratedCode; n: var Cursor) =
  inc n
  while n.kind != ParRi:
    if n.kind == StringLit:
      c.add pool.strings[n.litId]
      inc n
    else:
      genx c, n
  inc n # ParRi
  c.add NewLine

proc isEmptyThenBranch(n: var Cursor): bool =
  # Check if the 'then' branch is empty (only contains no-op statements)
  var tmp = n
  
  # Check if the branch is empty or only contains empty statements
  if tmp.stmtKind == StmtsS:
    inc tmp
    while tmp.kind != ParRi:
      if tmp.stmtKind != NoStmt:
        return false
      inc tmp
    return true
  
  return tmp.kind == DotToken

proc isEmptyElseBranch(n: var Cursor): bool =
  # Check if the 'else' branch is empty (only contains no-op statements)
  var tmp = n
  
  if tmp.substructureKind == ElseU:
    inc tmp
    return isEmptyThenBranch(tmp)
  
  return false

proc canInvertCondition(n: var Cursor): bool =
  # Check if a condition can be easily inverted
  return n.exprKind in {EqC, NeqC, LtC, LeC}

proc generateInvertedCondition(c: var GeneratedCode; n: var Cursor) =
  # Generate inverted version of the condition
  let exprKind = n.exprKind
  inc n
  
  c.add ParLe
  
  # Invert the operator
  var invertedOp = case exprKind
    of EqC: " != "
    of NeqC: " == "
    of LtC: " >= "
    of LeC: " > "
    else: ""
  
  genx c, n
  c.add invertedOp
  genx c, n
  
  c.add ParRi
  skipParRi n

proc genIf(c: var GeneratedCode; n: var Cursor) =
  var hasElse = false
  var hasElif = false
  inc n
  let first = n
  
  # Analyze if structure for optimizations
  var tmp = n
  var elifCount = 0
  while tmp.kind != ParRi:
    if tmp.substructureKind == ElifU:
      inc elifCount
    skip tmp
  
  # Start generating if statement
  while n.kind != ParRi:
    case n.substructureKind
    of ElifU:
      if hasElse:
        error c.m, "no `elif` allowed after `else` but got: ", n
      else:
        inc n
        
        # Optimization: If this is the only 'if' (no 'elif's) and there's no or empty 'else'
        # and the 'then' part is empty, invert the condition and skip generating empty block
        let condStart = n
        if (elifCount == 1) and isEmptyThenBranch(n.next) and isEmptyElseBranch(n.next.next) and canInvertCondition(condStart):
          # Skip this whole structure as it's effectively a no-op
          skipParRi n
          hasElif = true
          continue
        
        # Optimization: If this is a simple if-else with empty 'then' branch, 
        # invert the condition and just use the 'else' branch
        if (elifCount == 1) and isEmptyThenBranch(n.next) and canInvertCondition(condStart):
          var elseStart = n.next.next
          if elseStart.substructureKind == ElseU:
            if hasElif:
              c.add ElseKeyword
            c.add IfKeyword
            
            # Generate inverted condition
            generateInvertedCondition(c, condStart)
            
            # Use the 'else' branch directly
            c.add CurlyLe
            inc elseStart
            genStmt c, elseStart
            c.add CurlyRi
            skipParRi elseStart
            
            # Set flags and continue
            hasElif = true
            continue
        
        # Regular approach
        if hasElif:
          c.add ElseKeyword
        c.add IfKeyword
        c.genCond n
        c.add CurlyLe
        genStmt c, n
        c.add CurlyRi
        skipParRi n
      hasElif = true
      
    of ElseU:
      hasElse = true
      if not hasElif:
        error c.m, "no `elif` before `else` but got: ", n
      else:
        inc n
        
        # Optimization: Skip generating empty else blocks
        if isEmptyThenBranch(n):
          skipParRi n
          continue
          
        c.add ElseKeyword
        c.add CurlyLe
        genStmt c, n
        c.add CurlyRi
        skipParRi n
        
    else:
      error c.m, "`if` expects `elif` or `else` but got: ", n
      
  skipParRi n
  if not hasElif and not hasElse:
    error c.m, "`if` expects `elif` or `else` but got: ", first

proc isSimpleIncrementLoop(c: var GeneratedCode; n: var Cursor): bool =
  # Check if this while loop can be optimized into a for loop
  # Returns true if optimization was applied
  
  # Save cursor position
  let startPos = n
  
  # Skip the 'while' token
  inc n
  
  # Check if the condition is a comparison
  if n.exprKind in {LtC, LeC}:
    let cmpOp = n.exprKind
    inc n
    
    # Check for simple indexing variable in LHS
    if n.kind == Symbol:
      let indexVar = n.symId
      let indexVarName = mangle(pool.syms[indexVar])
      inc n
      
      # Check if RHS is a constant
      if n.kind == IntLit:
        let limitVal = pool.integers[n.intId]
        inc n
        
        # Save position for examining the loop body
        let bodyStart = n
        
        # Check if loop body contains increment of index var
        var found = false
        var tmp = n
        if tmp.stmtKind == StmtsS:
          inc tmp
          while tmp.kind != ParRi:
            if tmp.stmtKind == AsgnS:
              var asgnTmp = tmp
              inc asgnTmp
              
              # Check if LHS is our index variable
              if asgnTmp.kind == Symbol and asgnTmp.symId == indexVar:
                inc asgnTmp
                
                # Check if RHS is an increment operation
                if asgnTmp.exprKind == AddC:
                  inc asgnTmp
                  skip asgnTmp # Skip type
                  
                  # Check if first operand is our index variable
                  if asgnTmp.kind == Symbol and asgnTmp.symId == indexVar:
                    inc asgnTmp
                    
                    # Check if second operand is 1
                    if asgnTmp.kind == IntLit and pool.integers[asgnTmp.intId] == 1:
                      found = true
                      break
            skip tmp
        
        if found:
          # We have a simple increment loop, generate optimized for loop
          c.add "for ("
          c.add indexVarName
          c.add " = 0; "
          c.add indexVarName
          
          if cmpOp == LtC:
            c.add " < "
          else: # LeC
            c.add " <= "
            
          c.add $limitVal
          c.add "; ++"
          c.add indexVarName
          c.add ") "
          
          c.add CurlyLe
          
          # Generate optimized body without the increment statement
          var bodyTmp = bodyStart
          if bodyTmp.stmtKind == StmtsS:
            inc bodyTmp
            while bodyTmp.kind != ParRi:
              if bodyTmp.stmtKind == AsgnS:
                var checkTmp = bodyTmp
                inc checkTmp
                
                # Skip the increment statement
                if checkTmp.kind == Symbol and checkTmp.symId == indexVar:
                  skip bodyTmp
                  continue
              
              # Include all other statements
              genStmt(c, bodyTmp)
          
          c.add CurlyRi
          
          # Skip the entire original while structure
          n = startPos
          skip n
          
          return true
  
  # Reset cursor position
  n = startPos
  return false

proc genWhile(c: var GeneratedCode; n: var Cursor) =
  let oldInToplevel = c.inToplevel
  c.inToplevel = false
  
  # Try to optimize simple increment loops into for loops
  if isSimpleIncrementLoop(c, n):
    c.inToplevel = oldInToplevel
    return
    
  # Regular while loop generation
  inc n
  c.add WhileKeyword
  c.genCond n
  c.add CurlyLe
  c.genStmt n
  c.add CurlyRi
  skipParRi n
  c.inToplevel = oldInToplevel

proc genTryCpp(c: var GeneratedCode; n: var Cursor) =
  inc n

  c.add TryKeyword
  c.add CurlyLe
  c.genStmt n
  c.add CurlyRi

  c.add CatchKeyword
  c.add "..."
  c.add ParRi
  c.add Space
  c.add CurlyLe
  if n.kind != DotToken:
    c.genStmt n
  else:
    inc n
  c.add CurlyRi

  if n.kind != DotToken:
    c.add CurlyLe
    c.genStmt n
    c.add CurlyRi
  else:
    inc n
  skipParRi n

proc genScope(c: var GeneratedCode; n: var Cursor) =
  c.add CurlyLe
  inc n
  c.m.openScope()
  while n.kind != ParRi:
    c.genStmt n
  skipParRi n
  c.add CurlyRi
  c.m.closeScope()

proc genBranchValue(c: var GeneratedCode; n: var Cursor) =
  if n.kind in {IntLit, UIntLit, CharLit, Symbol} or n.exprKind in {TrueC, FalseC}:
    c.genx n
  else:
    error c.m, "expected valid `of` value but got: ", n

proc genCaseCond(c: var GeneratedCode; n: var Cursor) =
  # BranchValue ::= Number | CharLiteral | Symbol | (true) | (false)
  # BranchRange ::= BranchValue | (range BranchValue BranchValue)
  # BranchRanges ::= (ranges BranchRange+)
  if n.substructureKind == RangesU:
    inc n
    while n.kind != ParRi:
      c.add CaseKeyword
      if n.substructureKind == RangeU:
        inc n
        genBranchValue c, n
        c.add " ... "
        genBranchValue c, n
        skipParRi n
      else:
        genBranchValue c, n
      c.add ":"
      c.add NewLine
    skipParRi n
  else:
    error c.m, "`ranges` expected but got: ", n

proc genLabel(c: var GeneratedCode; n: var Cursor) =
  inc n
  if n.kind == SymbolDef:
    let name = mangle(pool.syms[n.symId])
    c.add name
    c.add Colon
    c.add Semicolon
    inc n
  else:
    error c.m, "expected SymbolDef but got: ", n
  skipParRi n

proc genGoto(c: var GeneratedCode; n: var Cursor) =
  inc n
  if n.kind == Symbol:
    let name = mangle(pool.syms[n.symId])
    c.add GotoKeyword
    c.add name
    c.add Semicolon
    inc n
  else:
    error c.m, "expected Symbol but got: ", n
  skipParRi n

proc genSwitch(c: var GeneratedCode; n: var Cursor) =
  # (case Expr (of BranchRanges StmtList)* (else StmtList)?) |
  c.add SwitchKeyword
  inc n
  let first = n
  c.genCond n
  c.add CurlyLe

  var hasElse = false
  var hasElif = false
  while n.kind != ParRi:
    case n.substructureKind
    of OfU:
      if hasElse:
        error c.m, "no `of` allowed after `else` but got: ", n
      else:
        inc n
        c.genCaseCond n
        c.add CurlyLe
        genStmt c, n
        c.add CurlyRi
        c.add BreakKeyword
        c.add Semicolon
        skipParRi n
      hasElif = true
    of ElseU:
      hasElse = true
      if not hasElif:
        error c.m, "no `of` before `else` but got: ", n
      else:
        c.add DefaultKeyword
        c.add NewLine
        c.add CurlyLe
        inc n
        genStmt c, n
        skipParRi n
        c.add CurlyRi
        c.add BreakKeyword
        c.add Semicolon
    else:
      error c.m, "`case` expects `of` or `else` but got: ", n
  if not hasElif and not hasElse:
    error c.m, "`case` expects `of` or `else` but got: ", first
  c.add CurlyRi
  skipParRi n

proc genVar(c: var GeneratedCode; n: var Cursor; vk: VarKind; toExtern = false) =
  case vk
  of IsLocal:
    genVarDecl c, n, IsLocal, toExtern
  of IsGlobal:
    moveToDataSection:
      genVarDecl c, n, IsGlobal, toExtern
  of IsThreadlocal:
    moveToDataSection:
      genVarDecl c, n, IsThreadlocal, toExtern
  of IsConst:
    moveToDataSection:
      genVarDecl c, n, IsConst, toExtern

proc genKeepOverflow(c: var GeneratedCode; n: var Cursor) =
  inc n # keepovf
  let op = n.exprKind
  var gcc = ""
  var prefix = "__builtin_"
  case op
  of AddC:
    gcc.add "add"
  of SubC:
    gcc.add "sub"
  of MulC:
    gcc.add "mul"
  of DivC:
    gcc.add "div_"
    prefix = "_Qnifc_"
  of ModC:
    gcc.add "mod_"
    prefix = "_Qnifc_"
  else:
    error c.m, "expected arithmetic operation but got: ", n
  inc n # operation
  if n.typeKind == IT:
    gcc = prefix & "s" & gcc
  elif n.typeKind == UT:
    gcc = prefix & "u" & gcc
  else:
    error c.m, "expected integer type but got: ", n
  inc n # type
  var isLongLong = false
  if n.kind == IntLit:
    let bits = pool.integers[n.intId]
    if bits == 64 or (bits == -1 and c.bits == 64):
      gcc.add "ll"
      isLongLong = true
    else:
      gcc.add "l"
    inc n
  else:
    error c.m, "expected integer literal but got: ", n
  c.currentProc.needsOverflowFlag = true
  skipParRi n # end of type
  c.add IfKeyword
  c.add ParLe
  gcc.add "_overflow"
  c.add gcc
  c.add ParLe
  genx c, n
  c.add Comma
  genx c, n
  skipParRi n
  c.add Comma
  if isLongLong:
    c.add "(long long int*)"
    c.add ParLe
  c.add Amp
  genLvalue c, n
  if isLongLong:
    c.add ParRi
  c.add ParRi
  c.add ParRi # end of condition
  c.add CurlyLe
  c.add OvfToken
  c.add AsgnOpr
  c.add OvfToken
  c.add " || "
  c.add "NIM_TRUE"
  c.add Semicolon
  c.add CurlyRi
  skipParRi n

proc genStmt(c: var GeneratedCode; n: var Cursor) =
  case n.stmtKind
  of NoStmt:
    if n.kind == DotToken:
      inc n
    else:
      error c.m, "expected statement but got: ", n
  of StmtsS:
    inc n
    while n.kind != ParRi:
      genStmt(c, n)
    inc n # ParRi
  of ScopeS:
    let oldInToplevel = c.inToplevel
    c.inToplevel = false
    genScope c, n
    c.inToplevel = oldInToplevel
  of CallS:
    genCall c, n
    c.add Semicolon
  of VarS:
    genVar c, n, IsLocal
  of GvarS:
    genVar c, n, IsGlobal
  of TvarS:
    genVar c, n, IsThreadlocal
  of ConstS:
    genVar c, n, IsConst
  of EmitS:
    genEmitStmt c, n
  of AsgnS:
    genCLineDir(c, info(n))
    inc n
    genLvalue c, n
    c.add AsgnOpr
    genx c, n
    c.add Semicolon
    skipParRi n
  of IfS: genIf c, n
  of WhileS: genWhile c, n
  of BreakS:
    inc n
    c.add BreakKeyword
    c.add Semicolon
    skipParRi n
  of CaseS: genSwitch c, n
  of LabS: genLabel c, n
  of JmpS: genGoto c, n
  of RetS:
    c.add ReturnKeyword
    inc n
    if n.kind != DotToken:
      c.add Space
      c.genx n
    else:
      inc n
    c.add Semicolon
    skipParRi n
  of DiscardS:
    inc n
    c.add DiscardToken
    c.genx n
    c.add Semicolon
    skipParRi n
  of TryS:
    genTryCpp c, n
  of RaiseS:
    c.add ThrowKeyword
    inc n
    if n.kind != DotToken:
      c.add Space
      c.genx n
    else:
      inc n
    c.add Semicolon
    skipParRi n
  of OnErrS:
    var onErrAction = n
    inc onErrAction
    genCallCanRaise c, n
    c.add Semicolon
    if onErrAction.kind != DotToken:
      genOnError(c, onErrAction)
  of ProcS, TypeS, ImpS, InclS:
    error c.m, "expected statement but got: ", n
  of KeepovfS:
    genKeepOverflow c, n
