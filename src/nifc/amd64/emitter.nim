#
#
#           NIFC Compiler
#        (c) Copyright 2024 Andreas Rumpf
#
#    See the file "copying.txt", included in this
#    distribution, for details about the copyright.
#

## Emits real ASM code from the NIF based format.

import std / [assertions, syncio, formatfloat]
import nifreader, nifstreams, nifcursors, bitabs, lineinfos
from std / strutils import escape

import asm_model
import ".." / mangler

type
  Context = object
    current: Cursor
    input: TokenBuf
    dest: string

proc fatal*(msg: string) =
  quit msg

proc setupInput(c: var Context; buffer: string) =
  var s = nifstreams.openFromBuffer(buffer)
  let res = processDirectives(s.r)
  assert res == Success
  c.input = default(TokenBuf)
  while true:
    let t = next(s)
    if t.kind == EofToken: break
    c.input.add t

proc matchIntLit(c: var Context): bool =
  if c.current.kind == IntLit:
    c.dest.addInt pool.integers[c.current.intId]
    inc c.current
    result = true
  else:
    result = false

proc matchUIntLit(c: var Context): bool =
  if c.current.kind == UIntLit:
    c.dest.add $pool.uintegers[c.current.uintId]
    inc c.current
    result = true
  else:
    result = false

proc matchFloatLit(c: var Context): bool =
  if c.current.kind == FloatLit:
    c.dest.add $pool.floats[c.current.floatId]
    inc c.current
    result = true
  else:
    result = false

proc matchStringLit(c: var Context): bool =
  if c.current.kind == StringLit:
    c.dest.add escape(pool.strings[c.current.litId])
    inc c.current
    result = true
  else:
    result = false

proc matchIdent(c: var Context): bool =
  if c.current.kind == Ident:
    c.dest.add pool.strings[c.current.litId]
    inc c.current
    result = true
  else:
    result = false

proc isTag(c: var Context; tag: TagId): bool =
  if c.current.kind == ParLe and c.current.tagId == tag:
    inc c.current
    result = true
  else:
    result = false

proc error(c: var Context; msg: string) {.noreturn.} =
  if c.current.info.isValid:
    let (f, line, col) = unpack(pool.man, c.current.info)
    if f.isValid:
      echo "[Error] ", pool.files[f] & "(" & $line & ", " & $col & "): " & msg
    else:
      echo "[Error] ???: " & msg
  else:
    echo "[Error] ???: " & msg
  when defined(debug):
    writeStackTrace()
  quit 1

proc matchParRi(c: var Context): bool =
  if c.current.kind == ParRi:
    inc c.current
    result = true
  else:
    result = false

proc peekParRi(c: var Context): bool {.inline.} =
  c.current.kind == ParRi

# Instruction optimization tracker for AMD64 peephole optimizations
type
  InstructionKind = enum
    iUnknown, iMove, iLoad, iAdd, iSub, iMul, iCmp, iJmp, iPush, iPop
  
  PeepholeState = object
    lastInstr: InstructionKind
    lastDest: string
    lastSrc: string
    lastImmVal: int
    inOptimizableSequence: bool

var peepholeState = PeepholeState(
  lastInstr: iUnknown,
  lastDest: "",
  lastSrc: "",
  lastImmVal: 0,
  inOptimizableSequence: false
)

proc resetPeepholeState() =
  peepholeState = PeepholeState(
    lastInstr: iUnknown,
    lastDest: "",
    lastSrc: "",
    lastImmVal: 0,
    inOptimizableSequence: false
  )

proc canOptimize(dest: var string; instrKind: InstructionKind; instr, op1, op2: string): bool =
  # Detect instruction patterns that can be optimized
  
  # Pattern 1: mov reg, X followed by mov reg, Y -> eliminate first mov
  if instrKind == iMove and peepholeState.lastInstr == iMove and 
     op1 == peepholeState.lastDest:
    # Skip generating this instruction as it would immediately be overwritten
    return true
  
  # Pattern 2: mov reg, 0 -> xor reg, reg (smaller instruction)
  if instrKind == iMove and op2 == "0":
    # Replace with XOR
    dest.add "xor "
    dest.add op1
    dest.add ", "
    dest.add op1
    dest.add "\n"
    return true
    
  # Pattern 3: add reg, 1 -> inc reg (smaller instruction)
  if instrKind == iAdd and op2 == "1":
    dest.add "inc "
    dest.add op1
    dest.add "\n"
    return true
  
  # Pattern 4: sub reg, 1 -> dec reg (smaller instruction)
  if instrKind == iSub and op2 == "1":
    dest.add "dec "
    dest.add op1
    dest.add "\n"
    return true
    
  # No optimization applicable
  return false

proc trackInstruction(instrKind: InstructionKind; op1, op2: string; immVal = 0) =
  peepholeState.lastInstr = instrKind
  peepholeState.lastDest = op1
  peepholeState.lastSrc = op2
  peepholeState.lastImmVal = immVal

proc emitTag(c: var Context; tag: string) =
  # Check for known instruction patterns that we can optimize
  var instrKind = iUnknown
  var op1, op2 = ""
  
  # Extract the instruction and operands for potential optimization
  if tag.startsWith("mov "):
    instrKind = iMove
    let parts = tag[4..^1].split(',')
    if parts.len >= 2:
      op1 = parts[0].strip()
      op2 = parts[1].strip()
  elif tag.startsWith("add "):
    instrKind = iAdd
    let parts = tag[4..^1].split(',')
    if parts.len >= 2:
      op1 = parts[0].strip()
      op2 = parts[1].strip()
  elif tag.startsWith("sub "):
    instrKind = iSub
    let parts = tag[4..^1].split(',')
    if parts.len >= 2:
      op1 = parts[0].strip()
      op2 = parts[1].strip()
  
  # Try to apply peephole optimizations
  if instrKind != iUnknown:
    # If optimization is applied, the output is already added to c.dest
    if canOptimize(c.dest, instrKind, tag, op1, op2):
      # Track the instruction we actually emitted instead
      trackInstruction(instrKind, op1, op2)
      return
    
    # Track this instruction for future optimization opportunities
    trackInstruction(instrKind, op1, op2)
  
  # Regular instruction emission
  c.dest.add tag
  c.dest.add " "

proc emit(c: var Context; token: string) =
  # At function boundary, reset peephole state
  if token == "proc" or token == ":":
    resetPeepholeState()
  
  c.dest.add token

proc matchAndEmitTag(c: var Context; tag: TagId; asStr: string): bool =
  if c.current.kind == ParLe and c.current.tagId == tag:
    emit c, asStr
    inc c.current
    result = c.current.kind == ParRi
    if result:
      inc c.current
  else:
    result = false

proc matchAny(c: var Context): bool =
  result = false

  while true:
    case c.current.kind
    of UnknownToken, DotToken, Ident, Symbol, SymbolDef, StringLit, CharLit, IntLit, UIntLit, FloatLit:
      inc c.current
      result = true
    of EofToken:
      result = false
      break
    of ParRi:
      result = true
      break
    of ParLe:
      var nested = 0
      while true:
        let k = c.current.kind
        inc c.current
        if k == ParLe: inc nested
        elif k == ParRi:
          dec nested
          if nested == 0: break

proc nl(c: var Context) = c.dest.add "\n"

proc lookupSym(c: var Context): bool =
  if c.current.kind == Symbol:
    c.dest.add mangle(pool.syms[c.current.symId])
    inc c.current
    result = true
  else:
    result = false

proc declareSym(c: var Context): bool =
  if c.current.kind == SymbolDef:
    c.dest.add mangle(pool.syms[c.current.symId])
    inc c.current
    result = true
  else:
    result = false

template success(b: bool): bool = b

include asm_grammar

proc produceAsmCode*(buffer, outp: string) =
  #registerTags()
  var c = Context()
  setupInput c, buffer
  c.current = beginRead(c.input)
  if not genModule(c):
    error(c, "(stmts) expected")
  endRead(c.input)
  writeFile outp, c.dest
