#
#
#           Hexer Compiler
#        (c) Copyright 2025 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

##[

The destroyer runs after `to_stmts` as it relies on `var tmp = g()`
injections. It only destroys variables and transforms assignments.

Statements
==========

Assignments and var bindings need to use `=dup`. In the first version, we don't
emit `=copy`.

`x = f()` is turned into `=destroy(x); x =bitcopy f()`.
`x = lastUse y` is turned into either

  `=destroy(x); x =bitcopy y; =wasMoved(y)` # no self assignments possible

or

  `let tmp = y; =wasMoved(y); =destroy(x); x =bitcopy tmp`  # safe for self assignments

`x = someUse y` is turned into either

  `=destroy(x); x =bitcopy =dup(y)` # no self assignments possible

or

  `let tmp = x; x =bitcopy =dup(y); =destroy(tmp)` # safe for self assignments

`var x = f()` is turned into `var x = f()`. There is nothing to do because the backend
interprets this `=` as `=bitcopy`.

`var x = lastUse y` is turned into `var x = y; =wasMoved(y)`.
`var x = someUse y` is turned into `var x = =dup(y)`.

]##

import std / assertions
include nifprelude
import nifindexes, symparser, treemangler
import ".." / nimony / [nimony_model, programs, typenav, decls]
import lifter

const
  NoLabel = SymId(0)

type
  ScopeKind = enum
    Other
    WhileOrBlock
  DestructorOp = object
    destroyProc: SymId
    arg: SymId
    hasNearbyAssignment: bool  # Flag for optimization purposes

  Scope = object
    label: SymId
    kind: ScopeKind
    isTopLevel: bool
    destroyOps: seq[DestructorOp]
    info: PackedLineInfo
    parent: ptr Scope
    recentlyAssigned: seq[SymId]  # Track recently assigned variables

  Context = object
    currentScope: Scope
    anonBlock: SymId
    dest: TokenBuf
    lifter: ref LiftingCtx
    stats: DestroyerStats  # Track stats for optimization feedback

  DestroyerStats = object
    totalDestroys: int    # Total number of destructor calls inserted
    optimizedOut: int     # Number of destructor calls that were optimized out
    inlinedDestroys: int  # Number of destructor calls that were inlined

proc createNestedScope(kind: ScopeKind; parent: var Scope; info: PackedLineInfo; label = NoLabel): Scope =
  Scope(label: label,
    kind: kind, destroyOps: @[], recentlyAssigned: @[], info: info, parent: addr(parent),
    isTopLevel: false)

proc createEntryScope(info: PackedLineInfo): Scope =
  Scope(label: NoLabel,
    kind: Other, destroyOps: @[], recentlyAssigned: @[], info: info, parent: nil,
    isTopLevel: true)

proc callDestroy(c: var Context; destroyProc: SymId; arg: SymId) =
  # Check if we can optimize out this destroy call
  for id in c.currentScope.recentlyAssigned:
    if id == arg:
      # This variable was just assigned to, so we can likely skip destruction
      # However, we need to be careful about aliasing
      c.stats.optimizedOut += 1
      return

  # If we're about to call a destructor for a variable that's going to be assigned to soon,
  # no need to destroy it
  if c.lifter[].isImmediatelyReassigned(arg, c.currentScope.info):
    c.stats.optimizedOut += 1
    return
    
  c.stats.totalDestroys += 1
  let info = c.currentScope.info
  copyIntoKind c.dest, CallS, info:
    copyIntoSymUse c.dest, destroyProc, info
    copyIntoSymUse c.dest, arg, info

proc leaveScope(c: var Context; s: var Scope) =
  # Reset the recently assigned list when leaving a scope
  s.recentlyAssigned.setLen(0)
  
  for i in countdown(s.destroyOps.high, 0):
    if not s.destroyOps[i].hasNearbyAssignment:
      callDestroy c, s.destroyOps[i].destroyProc, s.destroyOps[i].arg

proc leaveNamedBlock(c: var Context; label: SymId) =
  #[ Consider:

  var x = f()
  block:
    break # do we want to destroy x here? No.

  ]#
  var it = addr(c.currentScope)
  while it != nil and it.label != label:
    leaveScope(c, it[])
    it = it.parent
  if it != nil and it.label == label:
    leaveScope(c, it[])
  else:
    raiseAssert "do not know which block to leave"

proc leaveAnonBlock(c: var Context) =
  var it = addr(c.currentScope)
  while it != nil and it.kind != WhileOrBlock:
    leaveScope(c, it[])
    it = it.parent
  if it != nil and it.kind == WhileOrBlock:
    leaveScope(c, it[])
  else:
    raiseAssert "do not know which block to leave"

proc trBreak(c: var Context; n: var Cursor) =
  let lab = n.firstSon
  if lab.kind == Symbol:
    leaveNamedBlock(c, lab.symId)
  else:
    leaveAnonBlock(c)
  takeTree c.dest, n

proc trReturn(c: var Context; n: var Cursor) =
  var it = addr(c.currentScope)
  while it != nil:
    leaveScope(c, it[])
    it = it.parent
  takeTree c.dest, n

when not defined(nimony):
  proc tr(c: var Context; n: var Cursor)

proc trLocal(c: var Context; n: var Cursor) =
  let info = n.info
  c.dest.add n
  var r = takeLocal(n, SkipFinalParRi)
  copyTree c.dest, r.name
  copyTree c.dest, r.exported
  copyTree c.dest, r.pragmas
  copyTree c.dest, r.typ

  tr c, r.val
  c.dest.addParRi()

  # Add to the recently assigned list to help optimize destructor calls
  if r.kind notin {GvarY, TvarY, GletY, TletY, ConstY}:
    c.currentScope.recentlyAssigned.add r.name.symId

  # Check if we need to register a destructor operation
  let destructor = getDestructor(c.lifter[], r.typ, info)
  if destructor != NoSymId and r.kind notin {CursorY, ResultY, GvarY, TvarY, GletY, TletY, ConstY}:
    # Check if this variable is immediately reassigned - if so, we can mark it for potential optimization
    let hasNearbyAssignment = c.lifter[].isImmediatelyReassigned(r.name.symId, info)
    c.currentScope.destroyOps.add DestructorOp(
      destroyProc: destructor, 
      arg: r.name.symId, 
      hasNearbyAssignment: hasNearbyAssignment
    )

proc trScope(c: var Context; body: var Cursor) =
  copyIntoKind c.dest, StmtsS, body.info:
    if body.stmtKind == StmtsS:
      inc body
      while body.kind != ParRi:
        tr c, body
      inc body
    else:
      tr c, body
    leaveScope(c, c.currentScope)

proc registerSinkParameters(c: var Context; params: Cursor) =
  var p = params
  inc p
  while p.kind != ParRi:
    let r = takeLocal(p, SkipFinalParRi)
    if r.typ.typeKind == SinkT:
      let destructor = getDestructor(c.lifter[], r.typ.firstSon, p.info)
      if destructor != NoSymId:
        c.currentScope.destroyOps.add DestructorOp(
          destroyProc: destructor, 
          arg: r.name.symId,
          hasNearbyAssignment: false
        )

proc trProcDecl(c: var Context; n: var Cursor) =
  c.dest.add n
  var r = takeRoutine(n, SkipFinalParRi)
  copyTree c.dest, r.name
  copyTree c.dest, r.exported
  copyTree c.dest, r.pattern
  copyTree c.dest, r.typevars
  copyTree c.dest, r.params
  copyTree c.dest, r.retType
  copyTree c.dest, r.pragmas
  copyTree c.dest, r.effects
  if r.body.stmtKind == StmtsS and not isGeneric(r):
    if hasPragma(r.pragmas, NodestroyP):
      copyTree c.dest, r.body
    else:
      var s2 = createEntryScope(r.body.info)
      s2.isTopLevel = false
      swap c.currentScope, s2
      registerSinkParameters(c, r.params)
      trScope c, r.body
      swap c.currentScope, s2
  else:
    copyTree c.dest, r.body
  c.dest.addParRi()

proc trNestedScope(c: var Context; body: var Cursor; kind = Other) =
  var oldScope = move c.currentScope
  c.currentScope = createNestedScope(kind, oldScope, body.info)
  trScope c, body
  swap c.currentScope, oldScope

proc trWhile(c: var Context; n: var Cursor) =
  #[ while prop(createsObj())
      was turned into `while (let tmp = createsObj(); prop(tmp))` by  `duplifier.nim`
      already and `to_stmts` did turn it into:

      while true:
        let tmp = createsObj()
        if not prop(tmp): break

      For these reasons we don't have to do anything special with `cond`. The same
      reasoning applies to `if` and `case` statements.
  ]#
  copyInto(c.dest, n):
    tr c, n
    trNestedScope c, n, WhileOrBlock

proc trBlock(c: var Context; n: var Cursor) =
  let label = n.firstSon
  let labelId = if label.kind == SymbolDef: label.symId else: c.anonBlock
  var oldScope = move c.currentScope
  c.currentScope = createNestedScope(WhileOrBlock, oldScope, n.info, labelId)
  copyInto(c.dest, n):
    takeTree c.dest, n
    trScope c, n
    swap c.currentScope, oldScope

proc trIf(c: var Context; n: var Cursor) =
  copyInto(c.dest, n):
    while n.kind != ParRi:
      case n.substructureKind
      of ElifU:
        copyInto(c.dest, n):
          tr c, n
          trNestedScope c, n
      of ElseU:
        copyInto(c.dest, n):
          trNestedScope c, n
      else:
        takeTree c.dest, n

proc trCase(c: var Context; n: var Cursor) =
  copyInto(c.dest, n):
    tr c, n
    while n.kind != ParRi:
      case n.substructureKind
      of OfU:
        copyInto(c.dest, n):
          takeTree c.dest, n
          trNestedScope c, n
      of ElseU:
        copyInto(c.dest, n):
          trNestedScope c, n
      else:
        takeTree c.dest, n

proc trAssignment(c: var Context; n: var Cursor) =
  # Handle assignment specially to track variables that have been assigned to
  # This information helps us optimize out unnecessary destructor calls
  let lhs = n.firstSon
  
  if lhs.kind == Symbol:
    # Add to recently assigned list
    c.currentScope.recentlyAssigned.add lhs.symId
  
  # Continue with regular transformation
  takeTree c.dest, n

proc tr(c: var Context; n: var Cursor) =
  if isAtom(n) or isDeclarative(n):
    takeTree c.dest, n
  else:
    case n.stmtKind
    of RetS:
      trReturn(c, n)
    of BreakS:
      trBreak(c, n)
    of IfS:
      trIf c, n
    of CaseS:
      trCase c, n
    of BlockS:
      trBlock c, n
    of LocalDecls:
      trLocal c, n
    of WhileS:
      trWhile c, n
    of AsgnS:
      trAssignment(c, n)  # Special handling for assignments
    of ProcS, FuncS, MacroS, MethodS, ConverterS:
      trProcDecl c, n
    else:
      if n.kind == ParLe:
        c.dest.add n
        inc n
        while n.kind != ParRi:
          tr(c, n)
        takeParRi(c.dest, n)
      else:
        c.dest.add n
        inc n

proc injectDestructors*(n: Cursor; lifter: ref LiftingCtx): TokenBuf =
  var c = Context(
    lifter: lifter, 
    currentScope: createEntryScope(n.info),
    anonBlock: pool.syms.getOrIncl("`anonblock.0"),
    dest: createTokenBuf(400),
    stats: DestroyerStats(totalDestroys: 0, optimizedOut: 0, inlinedDestroys: 0)
  )
  var n = n
  assert n.stmtKind == StmtsS
  c.dest.add n
  inc n
  while n.kind != ParRi:
    tr(c, n)

  leaveScope c, c.currentScope
  takeParRi(c.dest, n)
  genMissingHooks lifter[]
  
  when defined(arcStats):
    # Output optimization statistics for debugging
    echo "ARC statistics:"
    echo "  Total destructor calls: ", c.stats.totalDestroys
    echo "  Optimized out calls: ", c.stats.optimizedOut
    echo "  Inlined calls: ", c.stats.inlinedDestroys
  
  result = ensureMove c.dest

# This additional procedure is used by arcoptimizer.nim
proc isImmediatelyReassigned*(lifter: LiftingCtx; symId: SymId; info: PackedLineInfo): bool =
  # This is a placeholder implementation - a real implementation would analyze the control flow
  # to determine if a variable is immediately reassigned after the given location
  return false  # Conservative default: assume it's not immediately reassigned
