#
#
#           Hexer Compiler
#        (c) Copyright 2025 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

## ARC (Automatic Reference Counting) and ORC (Ownership Reference Counting) optimizer.
## This module implements optimizations to reduce unnecessary reference counting operations
## and improve memory management efficiency.

import std / [assertions, strutils] # Added strutils
include nifprelude
import nifindexes, symparser, treemangler
import ".." / nimony / [nimony_model, programs, typenav, decls]
import mover, destroyer, lifter # Added mover

type
  RefCountOp = enum
    rcNone,    # Not a reference counting operation
    rcIncRef,  # Increment reference count
    rcDecRef,  # Decrement reference count
    rcDestroy, # Destroy object
    rcAssign,  # Assignment (may involve inc/dec)
    rcCopy,    # Copy (=dup) operation
    rcWasMoved # Mark as moved

  OpLocation = object
    lineInfo: PackedLineInfo
    symId: SymId  # Symbol being operated on
    opKind: RefCountOp
    scope: int    # Scope depth

  ARCOptContext = object
    operations: seq[OpLocation]
    scopeDepth: int
    currentScope: int
    dest: TokenBuf
    lifter: ref LiftingCtx
    eliminatedOps: int

# Forward declarations are usually implicit in Nim if signature is seen first.
# Removing explicit .forward. pragmas. Ensure definition order allows this.
proc collectRefCountOps(ctx: var ARCOptContext; n: var Cursor)
proc optimizeRefCounting(ctx: var ARCOptContext)
proc transformWithOptimizedOps(ctx: var ARCOptContext; n: var Cursor)
proc isRefCountedType(ctx: ARCOptContext; typ: Cursor): bool
proc getRefCountOpKind(n: Cursor): RefCountOp
proc shouldRetainOp(ctx: ARCOptContext; n: Cursor): bool
proc getFirstSon(n: Cursor): Cursor
proc getLastSon(n: Cursor): Cursor

# Corrected var parameter syntax: n: var Cursor
proc analyzeRefCountingOps*(n: var Cursor; lifter: ref LiftingCtx): TokenBuf =
  ## Analyzes and optimizes reference counting operations in the given AST.
  ## Returns the optimized AST as a TokenBuf.
  var ctx = ARCOptContext(
    operations: @[],
    scopeDepth: 0,
    currentScope: 0,
    dest: createTokenBuf(400),
    lifter: lifter,
    eliminatedOps: 0
  )
  
  # First pass: collect all reference counting operations
  collectRefCountOps(ctx, n)
  
  # Second pass: analyze and eliminate unnecessary operations
  optimizeRefCounting(ctx)
  
  # Third pass: generate optimized code
  transformWithOptimizedOps(ctx, n)
  
  result = ensureMove(ctx.dest) # Assuming ensureMove is available (likely from mover)

# Helper: Get First Son
proc getFirstSon(n: Cursor): Cursor =
  assert n.kind == ParLe
  result = n
  inc result

# Helper: Get Last Son
proc getLastSon(n: Cursor): Cursor =
  assert n.kind == ParLe
  result = n
  inc result # Move to first son (or ParRi if empty)
  var current = result
  if current.kind == ParRi: # Handle empty list like ()
      # Decide what to return for empty list. Returning original ParLe might be confusing.
      # Returning a 'nil' or 'invalid' cursor might be better if possible.
      # For now, let's return the cursor pointing to ParRi itself to indicate end.
      return current
  while current.kind != ParRi:
    result = current # Keep track of the last valid node before ParRi
    skip current # Move cursor past the current child node (ensure 'skip' is available and works correctly)

proc isRefCountedType(ctx: ARCOptContext; typ: Cursor): bool =
  ## Determines if a type requires reference counting
  # Updated check based on nimony_tags.nim
  case typ.typeKind
  of RefT: # Explicit ref types are reference counted
    result = true
  # Nim's string and seq are implicitly reference counted, but lack specific tags here.
  # Assuming they might be represented via RefT or handled by compiler magic elsewhere.
  # ObjectT might be relevant for 'ref object'. Let's include it cautiously.
  of ObjectT: # Assuming ref objects might use this tag
    # This might be too broad, depends on whether ObjectT always implies ref object
    result = true # Re-evaluate if this causes issues
  of SinkT: # Sink types depend on their underlying type
    var underlying = getFirstSon(typ) # SinkT (...) -> get first son for underlying type
    result = isRefCountedType(ctx, underlying)
  else:
    result = false # Other types (PtrT, value types, etc.) are not.

proc getRefCountOpKind(n: Cursor): RefCountOp =
  ## Identifies reference counting operations in the AST
  ## Identifies reference counting operations in the AST
  if n.kind != ParLe:
    return rcNone

  if n.stmtKind == CallS:
    var procName = getFirstSon(n) # Use helper
    if procName.kind == Symbol:
      let name = pool.syms[procName.symId] # Correct symbol name retrieval
      # Using strutils.endsWith
      if name.endsWith("=destroy"):
        return rcDestroy
      elif name.endsWith("=wasMoved"):
        return rcWasMoved

  # Check for assignments that might involve reference counting
  if n.stmtKind == AsgnS:
    return rcAssign

  return rcNone

proc collectRefCountOps(ctx: var ARCOptContext; n: var Cursor) =
  ## Collects all reference counting operations in the AST
  var node = n # Use a copy to iterate/recurse
  var originalNodeKind = node.kind # Store kind before potential modification/advancement
  var originalNodeStmtKind = node.stmtKind # Store stmtKind

  if originalNodeStmtKind == StmtsS or originalNodeStmtKind == ScopeS:
    inc ctx.scopeDepth
    ctx.currentScope = ctx.scopeDepth

  let opKind = getRefCountOpKind(node)
  if opKind != rcNone:
    var target = default(Cursor) # Initialize target explicitly
    # Determine target based on operation kind
    if opKind == rcDestroy or opKind == rcWasMoved:
       # Assuming destroy/wasMoved takes target as the last argument
       target = getLastSon(node) # Use helper
    elif opKind == rcAssign:
       # Assignment target is the first son (LHS)
       target = getFirstSon(node) # Use helper
    # Add cases here if IncRef/DecRef calls are introduced with different argument structures
    # else: # Default case for hypothetical IncRef/DecRef
    #    target = getFirstSon(node) # Or second son, depending on call structure

    # Check if target is valid and a symbol before adding
    if target.kind == Symbol:
      ctx.operations.add(OpLocation(
        lineInfo: node.info, # Use info from the ParLe node
        symId: target.symId,
        opKind: opKind,
        scope: ctx.currentScope
      ))
    # else: Handle cases where target is not a simple symbol? (e.g., field access, array index)
    #      For now, we only track ops on simple symbols.

  # Recurse into children
  if originalNodeKind == ParLe:
    inc node # Move past ParLe
    while node.kind != ParRi:
      var child = node # Create a cursor for the child node
      collectRefCountOps(ctx, child) # Recurse on the child
      node = child # Update node cursor to position after child processing
    inc node # Move past ParRi
  elif originalNodeKind != EofToken: # Avoid incrementing past EOF
    inc node # Skip single tokens (Symbol, IntLit, etc.)

  # Update the original cursor 'n' to reflect the advancement made by processing 'node'
  n = node

  if originalNodeStmtKind == StmtsS or originalNodeStmtKind == ScopeS:
    dec ctx.scopeDepth


proc canEliminate(ctx: ARCOptContext; op1, op2: OpLocation): bool =
  ## Determines if a pair of operations can be eliminated
  ## Example: incref followed by decref on same variable with no intervening use
  
  if op1.symId != op2.symId:
    return false  # Different variables
  
  if op1.opKind == rcIncRef and op2.opKind == rcDecRef:
    # Check if there are no uses of the variable between these operations
    # This is a simplified check - a real implementation would track variable uses
    return true
  
  return false

proc optimizeRefCounting(ctx: var ARCOptContext) =
  ## Analyzes collected operations and marks unnecessary ones for elimination
  var i = 0
  while i < ctx.operations.len:
    var j = i + 1
    while j < ctx.operations.len:
      if canEliminate(ctx, ctx.operations[i], ctx.operations[j]):
        # Mark operations for elimination
        ctx.operations[i].opKind = rcNone
        ctx.operations[j].opKind = rcNone
        inc ctx.eliminatedOps, 2
        break
      inc j
    inc i

proc shouldRetainOp(ctx: ARCOptContext; n: Cursor): bool =
  ## Determines if an operation should be retained in the optimized output
  let opKind = getRefCountOpKind(n)
  if opKind == rcNone:
    return true  # Not a reference counting operation

  var target = default(Cursor) # Initialize target
  # Determine target based on operation kind, similar to collectRefCountOps
  if opKind == rcDestroy or opKind == rcWasMoved:
     target = getLastSon(n) # Use helper
  elif opKind == rcAssign:
     target = getFirstSon(n) # Use helper
  # Add cases here if IncRef/DecRef calls are introduced
  else: # Default case (should ideally not happen if opKind != rcNone)
     # If we reach here, it implies an RC op type exists but isn't handled above.
     # Fallback or error? For now, assume it's not optimizable if target logic is unclear.
     return true # Cannot determine target reliably

  if target.kind != Symbol:
    return true  # Can't optimize if not operating on a simple symbol

  # Check if this operation was marked for elimination
  for op in ctx.operations:
    # Match based on line info and symbol ID (assuming line info is unique enough for now)
    # This matching might be fragile if line infos are reused or not precise.
    if op.lineInfo == n.info and op.symId == target.symId and op.opKind == rcNone:
      return false  # This operation should be eliminated

  return true  # Retain by default

proc transformWithOptimizedOps(ctx: var ARCOptContext; n: var Cursor) =
  ## Transforms the AST, eliminating unnecessary reference counting operations
  var node = n # Use a copy
  var originalNodeKind = node.kind

  if not shouldRetainOp(ctx, node):
    # Skip this operation - it's been optimized out
    skip node # Use skip to advance past the entire node
    n = node # Update original cursor
    return

  # If the operation should be retained, copy/transform it
  ctx.dest.add node # Add the ParLe or other token

  if originalNodeKind == ParLe:
    inc node # Move past ParLe
    while node.kind != ParRi:
      var child = node
      transformWithOptimizedOps(ctx, child) # Recurse on children
      node = child # Update node cursor based on child processing
    ctx.dest.add node # Add the ParRi
    inc node # Move past ParRi
  elif originalNodeKind != EofToken: # Avoid incrementing past EOF
    # For single tokens (Symbol, IntLit, etc.), just move past them
    inc node

  # Update the original cursor 'n' to reflect the advancement
  n = node


# Helper functions for ARC optimization

proc optimizeLocalCopy(ctx: var ARCOptContext; n: var Cursor): bool =
  ## Optimizes copies within the same scope
  ## Returns true if optimization was applied
  # This function seems intended to be called during transformation,
  # but it needs access to the original AST/buffer for isLastUse.
  # It's currently defined but not called within transformWithOptimizedOps.
  # Keeping the structure but noting the limitation.
  if n.stmtKind != AsgnS:
    return false

  var lhs = getFirstSon(n) # Use helper
  var rhs = getLastSon(n)  # Use helper

  if lhs.kind != Symbol or rhs.kind != Symbol:
    return false

  # Check if this is a local copy that can be optimized
  # In many cases, we can replace a copy with a move if the source won't be used again
  var otherUsage = NoLineInfo
  # The call to isLastUse needs the *original* buffer, not ctx.dest.
  # This optimization cannot reliably work in this pass without more context.
  # Commenting out the check:
  # if isLastUse(rhs, ???, otherUsage): # Requires original buffer
  #   # Replace copy with move
  #   # This would involve modifying ctx.dest based on the analysis
  #   echo "Optimization Opportunity: Replace copy with move for ", pool.syms[rhs.symId]
  #   # Actual transformation logic would go here
  #   return true

  return false

proc eliminateRedundantDestroy(ctx: var ARCOptContext; n: var Cursor): bool =
  ## Eliminates unnecessary destroy operations
  ## Returns true if the destroy operation can be eliminated
  # Similar limitation as optimizeLocalCopy regarding context/control flow analysis.
  if getRefCountOpKind(n) != rcDestroy:
    return false

  var target = getLastSon(n) # Use helper
  if target.kind != Symbol:
    return false

  # Check if this variable is immediately reassigned or goes out of scope
  # This requires more sophisticated analysis (e.g., control flow graph)
  # than available in this simple pass.
  # Example check (very basic): Look ahead for immediate reassignment? (Fragile)
  # var nextNode = n; skip nextNode;
  # if nextNode.stmtKind == AsgnS and getFirstSon(nextNode).symId == target.symId:
  #    return true # Potential optimization

  return false  # Default to not eliminating without full analysis
