#
#           NIFC Compiler - Common Definitions
#        (c) Copyright 2024 Andreas Rumpf
#
#    Shared types and procedures for NIFC code generation modules.
#

import std / [assertions, syncio, tables, sets, intsets, formatfloat, strutils, packedsets, hashes]
from std / os import changeFileExt, splitFile, extractFilename
from std / sequtils import insert

# Include necessary base definitions
include ".." / lib / nifprelude # Provides Cursor, Literals, FileId, BiTable, PackedSet, pool, etc.
# Symbols from nifprelude should be directly available via include.

# Import and re-export other necessary modules
import mangler, nifc_model, cprelude, noptions, typenav # Provides Module, SymId, NifcPragma, CallConv, etc.
export mangler, nifc_model, cprelude, noptions, typenav
export strutils, formatfloat # Also export needed stdlib modules

# --- Core Token Definitions ---
type
  CodeGenToken* = distinct uint32 # Renamed from Token

proc `==`*(a, b: CodeGenToken): bool {.borrow.}

type
  PredefinedToken* = enum
    IgnoreMe = "<unused>"
    EmptyToken = ""
    CurlyLe = "{"
    CurlyRi = "}"
    ParLe = "("
    ParRi = ")"
    BracketLe = "["
    BracketRi = "]"
    NewLine = "\n"
    Semicolon = ";"
    Comma = ", "
    Space = " "
    Colon = ": "
    Dot = "."
    Arrow = "->"
    Star = "*"
    Amp = "&"
    DoubleQuote = "\""
    AsgnOpr = " = "
    ScopeOpr = "::"
    ConstKeyword = "const "
    StaticKeyword = "static "
    ExternKeyword = "extern "
    WhileKeyword = "while "
    GotoKeyword = "goto "
    IfKeyword = "if "
    ElseKeyword = "else "
    SwitchKeyword = "switch "
    CaseKeyword = "case "
    DefaultKeyword = "default:"
    BreakKeyword = "break"
    NullPtr = "NIM_NIL"
    ReturnKeyword = "return"
    TypedefStruct = "typedef struct " # Keep as enum for easy use
    TypedefUnion = "typedef union "   # Keep as enum
    TypedefKeyword = "typedef "       # Keep as enum
    IncludeKeyword = "#include "
    LineDirKeyword = "#line "
    DiscardToken = "(void) "
    TryKeyword = "try "
    CatchKeyword = "catch ("
    ThrowKeyword = "throw"
    ErrToken = "NIFC_ERR_"
    OvfToken = "NIFC_OVF_"
    ThreadVarToken = "NIM_THREADVAR "

# --- Variable Kind Enum (Moved from codegen.nim) ---
type
  VarKind* = enum # Export VarKind
    IsLocal, IsGlobal, IsThreadlocal, IsConst

# --- Code Generation State ---
type
  GenFlag* = enum
    gfMainModule # isMainModule
    gfHasError   # already generated the error variable
    gfProducesMainProc # needs main proc
    gfInCallImportC # in importC call context

  CurrentProc* = object
    needsOverflowFlag*: bool # Export field
    nextTemp*: int          # Export field

  GeneratedCode* = object
    m*: Module # Export module field
    includes*: seq[CodeGenToken]
    includedHeaders*: IntSet
    data*: seq[CodeGenToken]
    protos*: seq[CodeGenToken]
    code*: seq[CodeGenToken]
    init*: seq[CodeGenToken]
    fileIds*: PackedSet[FileId]
    tokens*: BiTable[CodeGenToken, string]
    headerFile*: seq[CodeGenToken]
    generatedTypes*: HashSet[SymId]
    requestedSyms*: HashSet[string]
    flags*: set[GenFlag]
    inToplevel*: bool
    objConstrNeedsType*: bool
    bits*: int
    currentProc*: CurrentProc

# --- Helper Procedures ---

# Helper to convert PredefinedToken to CodeGenToken using the correct cast
proc toToken*(p: PredefinedToken): CodeGenToken {.inline.} =
  CodeGenToken(uint32(ord(p)))

proc fillTokenTable*(tab: var BiTable[CodeGenToken, string]) =
  for e in EmptyToken..high(PredefinedToken):
    let id = tab.getOrIncl $e # Get or include based on string value
    # Assert that the retrieved/included CodeGenToken matches the expected one
    # Convert CodeGenToken back to uint32 for string conversion in the assert message
    assert id == e.toToken, "CodeGenToken mismatch for " & $e & ": got " & $(uint32(id)) & ", expected " & $(uint32(e.toToken))

proc initGeneratedCode*(m: sink Module, flags: set[GenFlag]; bits: int): GeneratedCode =
  result = GeneratedCode(m: m, code: @[], tokens: initBiTable[CodeGenToken, string](),
      fileIds: initPackedSet[FileId](), flags: flags, inToplevel: true,
      objConstrNeedsType: true, bits: bits)
  fillTokenTable(result.tokens)

proc add*(c: var GeneratedCode; t: PredefinedToken) {.inline.} =
  # Use the helper proc for consistency
  c.code.add t.toToken

proc add*(c: var GeneratedCode; s: string) {.inline.} =
  c.code.add c.tokens.getOrIncl(s)

# Templates for moving code sections (Moved from codegen.nim)
template moveToDataSection*(body: untyped) =
  # Uses GeneratedCode fields: code, data
  let oldLen = c.code.len
  body
  for i in oldLen ..< c.code.len:
    c.data.add c.code[i]
  setLen c.code, oldLen

template moveToInitSection*(body: untyped) =
  # Uses GeneratedCode fields: code, init
  let oldLen = c.code.len
  body
  for i in oldLen ..< c.code.len:
    c.init.add c.code[i]
  setLen c.code, oldLen

# Line directive generation (Moved from codegen.nim)
proc genCLineDir*(c: var GeneratedCode; info: PackedLineInfo) = # Export proc
  if optLineDir in c.m.config.options and info.isValid: # Uses optLineDir from noptions
    let rawInfo = unpack(pool.man, info) # Uses pool
    let id = rawInfo.file
    let line = rawInfo.line
    let name = "FX_" & $(int id)
    c.add LineDirKeyword # Uses PredefinedToken
    c.add $line
    c.add Space # Uses PredefinedToken
    c.add name
    c.add NewLine # Uses PredefinedToken
    if id.isValid:
      c.fileIds.incl id # Uses PackedSet field from GeneratedCode

# Error reporting (copied from codegen.nim)
proc error*(m: Module; msg: string; n: Cursor) {.noreturn.} =
  let info = n.info
  if info.isValid:
    let rawInfo = unpack(pool.man, info)
    if rawInfo.file.isValid:
      write stdout, pool.files[rawInfo.file]
      write stdout, "(" & $rawInfo.line & ", " & $(rawInfo.col+1) & ") "
  write stdout, "[Error] "
  write stdout, msg
  # Avoid calling toString on cursor directly, print kind instead
  writeLine stdout, "got node kind: ", n.kind
  when defined(debug):
    echo getStackTrace()
  quit 1

# Overload for error without cursor context
proc error*(m: Module; msg: string) {.noreturn.} =
  # Cannot print file/line info without cursor
  write stdout, "[Error] "
  writeLine stdout, msg
  when defined(debug):
    echo getStackTrace()
  quit 1

# Literal generation (copied from codegen.nim)
proc genIntLit*(c: var GeneratedCode; litId: IntId) =
  let i = pool.integers[litId]
  if i > low(int32) and i <= high(int32) and c.bits != 64:
    c.add $i
  elif i == low(int32) and c.bits != 64:
    # Nim has the same bug for the same reasons :-)
    c.add "(-2147483647 -1)"
  elif i > low(int64):
    c.add "IL64("
    c.add $i
    c.add ")"
  else:
    c.add "(IL64(-9223372036854775807) - IL64(1))"

proc genUIntLit*(c: var GeneratedCode; litId: UIntId) =
  let i = pool.uintegers[litId]
  if i <= high(uint32) and c.bits != 64:
    c.add $i
    c.add "u"
  else:
    c.add $i
    c.add "ull"

# Calling convention helper (copied from codegen.nim)
proc callingConvToStr*(cc: CallConv): string =
  case cc
  of NoCallConv: ""
  of Cdecl: "N_CDECL"
  of Stdcall: "N_STDCALL"
  of Safecall: "N_SAFECALL"
  of Syscall: "N_SYSCALL"
  of Fastcall: "N_FASTCALL"
  of Thiscall: "N_THISCALL"
  of Noconv: "N_NOCONV"
  of Member: "N_NOCONV"
  of Nimcall: "N_NIMCALL"

# Common Pragma Handling (copied from gentypes.nim, needs GeneratedCode)
proc genWasPragma*(c: var GeneratedCode; n: var Cursor; literals: Literals) = # Added literals
  inc n
  c.add "/* " & toString(n, false) & " */" # toString likely doesn't need literals
  skip n
  skipParRi n

# Template for adding optional name (copied from gentypes.nim)
template maybeAddName*(c: var GeneratedCode; name: string) =
  if name != "":
    c.add Space
    c.add name

# Template for adding atomic C type (copied from gentypes.nim)
template atom*(c: var GeneratedCode; s, name: string) =
  c.add s
  maybeAddName(c, name)

# --- Header Inclusion Helper (Moved from codegen.nim) ---
proc inclHeader*(c: var GeneratedCode; name: string) = # Exported
  # Use getOrIncl for BiTable with common_defs.CodeGenToken
  let headerToken: common_defs.CodeGenToken = c.tokens.getOrIncl(name) # Explicit type: common_defs.CodeGenToken
  # Convert header CodeGenToken to int for the IntSet (PackedSet[int])
  let headerInt = int(uint32(headerToken))
  # Use contains and incl from std/packedsets for IntSet
  if not packedsets.contains(c.includedHeaders, headerInt):
    packedsets.incl(c.includedHeaders, headerInt)
    # Add the include directive to the 'includes' sequence
    if name.len > 0 and name[0] == '#':
      discard "skip the #include keyword"
    else:
      c.includes.add IncludeKeyword.toToken # Qualify PredefinedToken
    if name.len > 0 and name[0] == '<':
      # Use standard seq add for common_defs.CodeGenToken
      let tokToAdd: common_defs.CodeGenToken = headerToken # Explicit type
      c.includes.add(tokToAdd) # Correct add for seq[common_defs.CodeGenToken]
    else:
      c.includes.add DoubleQuote.toToken # Qualify PredefinedToken
      # Use standard seq add for common_defs.CodeGenToken
      let tokToAdd: common_defs.CodeGenToken = headerToken # Explicit type
      c.includes.add(tokToAdd) # Correct add for seq[common_defs.CodeGenToken]
      c.includes.add DoubleQuote.toToken # Qualify PredefinedToken

    c.includes.add NewLine.toToken # Qualify PredefinedToken

# --- Literal Handling Functions (Moved from codegen.nim) ---

# Note: isLiteral needs to be defined *before* evaluateLiteral*Expr if they call it,
# but here isLiteral calls itself recursively and is used by genVarDecl in codegen.nim,
# while evaluateLiteral*Expr are used by genx in genexprs.nim.
# Placing them here should be fine as long as imports are correct.

proc isLiteral*(n: var Cursor): bool = # Exported for genexprs, genVarDecl
  # Create a copy to avoid modifying the original cursor during check
  # This proc needs access to pool from nifprelude (included above)
  # and NifcExpr enums from nifc_model (imported and exported above)
  var checkCursor = n
  case checkCursor.kind
  of IntLit, UIntLit, FloatLit, CharLit, StringLit, DotToken:
    result = true
    inc checkCursor # Advance the copy
  else:
    case checkCursor.exprKind # From nifc_model
    of FalseC, TrueC, InfC, NegInfC, NanC, SufC, NilC: # NifcExpr enums
      result = true
      skip checkCursor # Advance the copy
    of AconstrC, OconstrC, CastC, ConvC: # NifcExpr enums
      result = true
      inc checkCursor # Advance past the constructor/cast tag
      skip checkCursor # Advance past the type node
      while checkCursor.kind != ParRi:
        if checkCursor.substructureKind == KvU: # From nifc_model
          inc checkCursor # Advance past KvU tag
          skip checkCursor # Advance past key node
          # Recursively check the value node using the copy
          if not isLiteral(checkCursor): return false
          # isLiteral advances checkCursor past the value node
          skipParRi checkCursor # Skip closing parenthesis of KvU
        else:
          # Recursively check the element node using the copy
          if not isLiteral(checkCursor): return false
          # isLiteral advances checkCursor past the element node
      skipParRi checkCursor # Advance past the closing parenthesis of the constructor/cast
    # New: also consider binary operations between literals as literals
    of AddC, SubC, MulC, DivC, ModC,
       BitandC, BitorC, BitxorC, ShlC, ShrC: # NifcExpr enums
      inc checkCursor # Advance past the operator tag
      skip checkCursor # Advance past the type node
      # Check left operand
      if not isLiteral(checkCursor): return false
      # Check right operand
      if not isLiteral(checkCursor): return false
      # If both were literals, the whole expression is considered literal
      result = true
      # isLiteral calls advanced checkCursor past both operands and the closing parenthesis
    # New: also consider unary operations on literals as literals
    of NegC, BitnotC: # NifcExpr enums
      inc checkCursor # Advance past the operator tag
      skip checkCursor # Advance past the type node
      # Check the operand
      if not isLiteral(checkCursor): return false
      # If the operand was literal, the whole expression is considered literal
      result = true
      # isLiteral call advanced checkCursor past the operand and the closing parenthesis
    else:
      result = false
  # IMPORTANT: After checking, update the original cursor 'n' to the position
  # reached by the 'checkCursor' copy. This ensures the main generation
  # process continues from the correct position after the literal check.
  n = checkCursor

proc evaluateLiteralBinExpr*(left, right: int64; op: NifcExpr): int64 = # Exported for genexprs
  # Perform constant folding for binary operations between integer literals
  # Needs NifcExpr enums from nifc_model (imported and exported above)
  case op
  of AddC: result = left + right
  of SubC: result = left - right
  of MulC: result = left * right
  of DivC:
    if right != 0: result = left div right
    else: result = 0 # Prevent division by zero
  of ModC:
    if right != 0: result = left mod right
    else: result = 0 # Prevent division by zero
  of BitandC: result = left and right
  of BitorC: result = left or right
  of BitxorC: result = left xor right
  of ShlC:
    if right >= 0 and right < 64: result = left shl right.int
    else: result = 0
  of ShrC:
    if right >= 0 and right < 64: result = left shr right.int
    else: result = 0
  else: result = 0 # Not a supported operation

proc evaluateLiteralUnExpr*(val: int64; op: NifcExpr): int64 = # Exported for genexprs
  # Perform constant folding for unary operations on integer literals
  # Needs NifcExpr enums from nifc_model (imported and exported above)
  case op
  of NegC: result = -val
  of BitnotC: result = not val
  else: result = 0 # Not a supported operation

# --- Functions moved from codegen.nim are REMOVED from here ---
