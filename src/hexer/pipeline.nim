#
#
#           Hexer Compiler
#        (c) Copyright 2024 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

import std/assertions
include nifprelude

import ".." / nimony / [nimony_model, programs, decls]
<<<<<<< HEAD
import hexer_context, iterinliner, desugar, xelim, duplifier, lifter, destroyer,
  constparams, vtables
=======
import hexer_context, iterinliner, desugar, xelim, duplifier, lifter, destroyer, constparams, lambdalift, arcoptimizer, orcycle
>>>>>>> bf15d5a (update)

proc publishHooks*(n: var Cursor) =
  var nested = 0
  while true:
    case n.kind
    of ParLe:
      case n.stmtKind
      of ProcS, FuncS, MacroS, MethodS, ConverterS:
        let decl = asRoutine(n)
        var dest = createTokenBuf()
        takeTree(dest, n)
        let sym = decl.name.symId
        publish sym, dest
      else:
        inc n
        inc nested
    of ParRi:
      inc n
      dec nested
    else:
      inc n
    if nested == 0: break

proc transform*(c: var EContext; n: Cursor; moduleSuffix: string): TokenBuf =
  var n = n
  elimForLoops(c, n)

  var n0 = move c.dest
  var c0 = beginRead(n0)

  # Apply lambda lifting - new step
  var n1 = lambdaLift(c0, moduleSuffix)
  endRead(n0)
<<<<<<< HEAD

  var c1 = beginRead(n1)
  var nx = lowerExprs(c1, moduleSuffix)
  endRead(n1)

  var c2 = beginRead(nx)
  let ctx = createLiftingCtx(moduleSuffix, c.bits)
  var n2 = injectDups(c2, nx, ctx)
  endRead(nx)

  var c3 = beginRead(n2)
  var n3 = lowerExprs(c3, moduleSuffix)
=======
  
  var c1 = beginRead(n1)
  var n2 = desugar(c1, moduleSuffix)
  endRead(n1)

  var c2 = beginRead(n2)
  let ctx = createLiftingCtx(moduleSuffix)
  var n3 = injectDups(c2, n2, ctx)
>>>>>>> bf15d5a (update)
  endRead(n2)

  var c3 = beginRead(n3)
  var n4 = lowerExprs(c3, moduleSuffix)
  endRead(n3)

  var c4 = beginRead(n4)
  var n5 = injectDestructors(c4, ctx)
  endRead(n4)
  
  # Optimize ARC operations by analyzing and eliminating unnecessary reference counting
  var c5_arc = beginRead(n5)
  var n5_opt = analyzeRefCountingOps(c5_arc, ctx)
  endRead(n5)
  
  # Detect and break reference cycles with ORC
  var c5_orc = beginRead(n5_opt)
  var n5_final = detectAndBreakCycles(c5_orc, ctx)
  endRead(n5_opt)
  
  # Use the optimized version for further processing
  n5 = move n5_final

  assert n5[n5.len-1].kind == ParRi
  shrink(n5, n5.len-1)

  if ctx[].dest.len > 0:
    var hookCursor = beginRead(ctx[].dest)
    #echo "HOOKS: ", toString(hookCursor)
    publishHooks hookCursor
    endRead(ctx[].dest)

  n5.add move(ctx[].dest)
  n5.addParRi()

  var needsXelimAgain = false

<<<<<<< HEAD
  var c5 = beginRead(n4)
  var nwithvtables = transformVTables(c5, moduleSuffix, needsXelimAgain)
  endRead(n4)
=======
  var c5 = beginRead(n5)
  var n6 = injectConstParamDerefs(c5, c.bits div 8, needsXelimAgain)
  endRead(n5)
>>>>>>> bf15d5a (update)

  var c6 = beginRead(nwithvtables)
  var n5 = injectConstParamDerefs(c6, c.bits div 8, needsXelimAgain)
  endRead(nwithvtables)

  if needsXelimAgain:
<<<<<<< HEAD
    var c7 = beginRead(n5)
    var n6 = lowerExprs(c7, moduleSuffix)
    endRead(n5)
    result = move n6
=======
    var c6 = beginRead(n6)
    var n7 = lowerExprs(c6, moduleSuffix)
    endRead(n6)
    result = move n7
>>>>>>> bf15d5a (update)
  else:
    result = move n6
