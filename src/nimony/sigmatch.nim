#       Nimony
# (c) Copyright 2024 Andreas Rumpf
#
# See the file "license.txt", included in this
# distribution, for details about the copyright.

import std / [sets, tables, assertions]

import bitabs, nifreader, nifstreams, nifcursors, lineinfos

import nimony_model, decls, programs, semdata, typeprops, xints, builtintypes

type
  Item* = object
    n*, typ*: Cursor
    kind*: SymKind

  FnCandidate* = object
    kind*: SymKind
    sym*: SymId
    typ*: Cursor

  MatchErrorKind* = enum
    InvalidMatch
    InvalidRematch
    ConstraintMismatch
    FormalTypeNotAtEndBug
    FormalParamsMismatch
    CallConvMismatch
    UnavailableSubtypeRelation
    NotImplementedConcept
    ImplicitConversionNotMutable
    UnhandledTypeBug
    MismatchBug
    MissingExplicitGenericParameter
    ExtraGenericParameter
    RoutineIsNotGeneric
    CouldNotInferTypeVar
    TooManyArguments
    TooFewArguments

  MatchError* = object
    info: PackedLineInfo
    #msg: string
    kind: MatchErrorKind
    typeVar: SymId
    expected, got: TypeCursor
    pos: int

  Match* = object
    inferred*: Table[SymId, Cursor]
    tvars: HashSet[SymId]
    fn*: FnCandidate
    args*, typeArgs*: TokenBuf
    err*, flipped*: bool
    skippedMod: TypeKind
    argInfo: PackedLineInfo
    pos, opened: int
    inheritanceCosts, intCosts: int
    returnType*: Cursor
    context: ptr SemContext
    error: MatchError
    firstVarargPosition*: int
    genericConverter*: bool

proc createMatch*(context: ptr SemContext): Match = Match(context: context, firstVarargPosition: -1)

proc concat(a: varargs[string]): string =
  result = a[0]
  for i in 1..high(a): result.add a[i]

proc typeToString*(n: Cursor): string =
  result = toString(n, false)

proc error(m: var Match; k: MatchErrorKind; expected, got: Cursor) =
  if m.err: return # first error is the important one
  m.err = true
  m.error = MatchError(info: m.argInfo, kind: k,
                       expected: expected, got: got, pos: m.pos+1)
  #writeStackTrace()
  #echo "ERROR: ", msg

proc error0(m: var Match; k: MatchErrorKind) =
  if m.err: return # first error is the important one
  m.err = true
  m.error = MatchError(info: m.argInfo, kind: k, pos: m.pos+1)

proc getErrorMsg(m: Match): string =
  case m.error.kind
  of InvalidMatch:
    concat("expected: ", typeToString(m.error.expected), " but got: ", typeToString(m.error.got))
  of InvalidRematch:
    concat("Could not match again: ", pool.syms[m.error.typeVar], " expected ",
      typeToString(m.error.expected), " but got ", typeToString(m.error.got))
  of ConstraintMismatch:
    concat(typeToString(m.error.got), " does not match constraint ",
      typeToString(m.error.expected))
  of FormalTypeNotAtEndBug:
    "BUG: formal type not at end!"
  of FormalParamsMismatch:
    "parameter lists do not match"
  of CallConvMismatch:
    "calling conventions do not match"
  of UnavailableSubtypeRelation:
    "subtype relation not available for `out` parameters"
  of NotImplementedConcept:
    "'concept' is not implemented"
  of ImplicitConversionNotMutable:
    concat("implicit conversion to ", typeToString(m.error.expected), " is not mutable")
  of UnhandledTypeBug:
    concat("BUG: unhandled type: ", pool.tags[m.error.expected.tagId])
  of MismatchBug:
    concat("BUG: expected: ", typeToString(m.error.expected), " but got: ", typeToString(m.error.got))
  of MissingExplicitGenericParameter:
    concat("missing explicit generic parameter for ", pool.syms[m.error.typeVar])
  of ExtraGenericParameter:
    "extra generic parameter"
  of RoutineIsNotGeneric:
    "routine is not generic"
  of CouldNotInferTypeVar:
    concat("could not infer type for ", pool.syms[m.error.typeVar])
  of TooManyArguments:
    "too many arguments"
  of TooFewArguments:
    "too few arguments"

proc addErrorMsg*(dest: var string; m: Match) =
  assert m.err
  dest.add "[" & $(m.error.pos) & "] " & getErrorMsg(m)

proc addErrorMsg*(dest: var TokenBuf; m: Match) =
  assert m.err
  dest.addParLe ErrT, m.argInfo
  dest.addDotToken()
  let str = "For type " & typeToString(m.fn.typ) & " mismatch at position\n" &
    "[" & $(m.pos+1) & "] " & getErrorMsg(m)
  dest.addStrLit str
  dest.addParRi()

proc expected(f, a: Cursor): string =
  concat("expected: ", typeToString(f), " but got: ", typeToString(a))

proc typeImpl(s: SymId): Cursor =
  let res = tryLoadSym(s)
  assert res.status == LacksNothing
  result = res.decl
  assert result.stmtKind == TypeS
  inc result # skip ParLe
  for i in 1..4:
    skip(result) # name, export marker, pragmas, generic parameter

proc objtypeImpl*(s: SymId): Cursor =
  result = typeImpl(s)
  let k = typeKind result
  if k in {RefT, PtrT}:
    inc result

proc getTypeSection*(s: SymId): TypeDecl =
  let res = tryLoadSym(s)
  assert res.status == LacksNothing
  result = asTypeDecl(res.decl)

proc getProcDecl*(s: SymId): Routine =
  let res = tryLoadSym(s)
  assert res.status == LacksNothing
  result = asRoutine(res.decl, SkipInclBody)

proc isObjectType(s: SymId): bool =
  let impl = objtypeImpl(s)
  result = impl.typeKind in {ObjectT, RefObjectT, PtrObjectT}

proc isEnumType*(n: Cursor): bool =
  if n.kind == Symbol:
    let impl = getTypeSection(n.symId)
    result = impl.kind == TypeY and impl.body.typeKind in {EnumT, HoleyEnumT}
  else:
    result = false

proc isConcept(s: SymId): bool =
  #let impl = typeImpl(s)
  # XXX Model Concept in the grammar
  #result = impl.tag == ConceptT
  result = false

iterator inheritanceChain(s: SymId): SymId =
  var objbody = objtypeImpl(s)
  while true:
    let od = asObjectDecl(objbody)
    if od.kind in {ObjectT, RefObjectT, PtrObjectT}:
      var parent = od.parentType
      if parent.typeKind in {RefT, PtrT}:
        inc parent
      if parent.kind == Symbol:
        let ps = parent.symId
        yield ps
        objbody = objtypeImpl(ps)
      else:
        break
    else:
      break

proc matchesConstraint(m: var Match; f: var Cursor; a: Cursor): bool

proc matchesConstraintAux(m: var Match; f: var Cursor; a: Cursor): bool =
  result = false
  case f.typeKind
  of NotT:
    inc f
    if not matchesConstraint(m, f, a):
      result = true
    if f.kind != ParRi: result = false
    skipToEnd f
  of AndT:
    inc f
    result = true
    while f.kind != ParRi:
      if not matchesConstraint(m, f, a):
        result = false
        break
    skipToEnd f
  of OrT:
    inc f
    while f.kind != ParRi:
      if matchesConstraint(m, f, a):
        result = true
        break
    skipToEnd f
  of ConceptT:
    # XXX Use some algorithm here that can cache the result
    # so that it can remember e.g. "int fulfils Fibable". For
    # now this should be good enough for our purposes:
    result = true
    skip f
  of TypeKindT:
    var aTag = a
    if a.kind == Symbol:
      aTag = typeImpl(a.symId)
    if aTag.typeKind == TypeKindT:
      inc aTag
    inc f
    assert f.kind == ParLe
    result = aTag.kind == ParLe and f.tagId == aTag.tagId
    inc f
    assert f.kind == ParRi
    inc f
    assert f.kind == ParRi
    inc f
  of OrdinalT:
    result = isOrdinalType(a)
    skip f
  else:
    result = false

proc matchesConstraint(m: var Match; f: var Cursor; a: Cursor): bool =
  result = false
  if f.kind == DotToken:
    inc f
    return true
  if a.kind == Symbol:
    let res = tryLoadSym(a.symId)
    assert res.status == LacksNothing
    if res.decl.symKind == TypevarY:
      var typevar = asTypevar(res.decl)
      return matchesConstraint(m, f, typevar.typ)
  if f.kind == Symbol:
    let res = tryLoadSym(f.symId)
    assert res.status == LacksNothing
    var typeImpl = asTypeDecl(res.decl)
    if typeImpl.kind == TypeY:
      result = matchesConstraint(m, typeImpl.body, a)
    inc f
  else:
    result = matchesConstraintAux(m, f, a)

proc matchesConstraint(m: var Match; f: SymId; a: Cursor): bool =
  let res = tryLoadSym(f)
  assert res.status == LacksNothing
  var typevar = asTypevar(res.decl)
  assert typevar.kind == TypevarY
  result = matchesConstraint(m, typevar.typ, a)

proc isTypevar(s: SymId): bool =
  let res = tryLoadSym(s)
  assert res.status == LacksNothing
  let typevar = asTypevar(res.decl)
  result = typevar.kind == TypevarY

proc linearMatch(m: var Match; f, a: var Cursor) =
  let fOrig = f
  let aOrig = a
  var nested = 0
  while true:
    if f.kind == Symbol and isTypevar(f.symId):
      # type vars are specal:
      let fs = f.symId
      if m.inferred.contains(fs):
        # rematch?
        var prev = m.inferred[fs]
        linearMatch(m, prev, a)
        inc f
        if m.err: break
        continue
      elif matchesConstraint(m, fs, a):
        m.inferred[fs] = a # NOTICE: Can introduce modifiers for a type var!
        inc f
        skip a
        continue
      else:
        m.error(ConstraintMismatch, f, a)
        break
    elif f.kind == a.kind:
      case f.kind
      of UnknownToken, EofToken,
          DotToken, Ident, Symbol, SymbolDef,
          StringLit, CharLit, IntLit, UIntLit, FloatLit:
        if f.uoperand != a.uoperand:
          m.error(InvalidMatch, fOrig, aOrig)
          break
      of ParLe:
        if f.uoperand != a.uoperand:
          m.error(InvalidMatch, fOrig, aOrig)
          break
        inc nested
      of ParRi:
        if nested == 0: break
        dec nested
    else:
      m.error(InvalidMatch, fOrig, aOrig)
      break
    inc f
    inc a
    # only match a single tree/token:
    if nested == 0: break

proc expectParRi(m: var Match; f: var Cursor) =
  if f.kind == ParRi:
    inc f
  else:
    m.error FormalTypeNotAtEndBug, f, f

proc extractCallConv(c: var Cursor): CallConv =
  result = NimcallC
  if substructureKind(c) == PragmasS:
    inc c
    while c.kind != ParRi:
      let res = callConvKind(c)
      if res != NoCallConv:
        result = res
      skip c
    inc c
  elif c.kind == DotToken:
    inc c
  else:
    raiseAssert "BUG: No pragmas found"

proc procTypeMatch(m: var Match; f, a: var Cursor) =
  if f.typeKind == ProcT:
    inc f
    for i in 1..4: skip f
  if a.typeKind == ProcT:
    inc a
    for i in 1..4: skip a
  var hasParams = 0
  if f.substructureKind == ParamsS:
    inc f
    if f.kind != ParRi: inc hasParams
  if a.substructureKind == ParamsS:
    inc a
    if a.kind != ParRi: inc hasParams, 2
  if hasParams == 3:
    while f.kind != ParRi and a.kind != ParRi:
      var fParam = takeLocal(f, SkipFinalParRi)
      var aParam = takeLocal(a, SkipFinalParRi)
      assert fParam.kind == ParamY
      assert aParam.kind == ParamY
      linearMatch m, fParam.typ, aParam.typ
    if f.kind == ParRi:
      if a.kind == ParRi:
        discard "ok"
      else:
        m.error FormalParamsMismatch, f, a
        skipToEnd a
    else:
      m.error FormalParamsMismatch, f, a
      skipToEnd f
      skipToEnd a
  elif hasParams == 2:
    m.error FormalParamsMismatch, f, a
    skipToEnd a
  elif hasParams == 1:
    m.error FormalParamsMismatch, f, a
    skipToEnd f

  # also correct for the DotToken case:
  inc f
  inc a

  # match return types:
  let fret = typeKind f
  let aret = typeKind a
  if fret == aret and fret == VoidT:
    skip f
    skip a
  else:
    linearMatch m, f, a
  # match calling conventions:
  let fcc = extractCallConv(f)
  let acc = extractCallConv(a)
  if fcc != acc:
    m.error CallConvMismatch, f, a
  skip f # effects
  skip f # body
  expectParRi m, f

const
  TypeModifiers = {MutT, OutT, LentT, SinkT, StaticT}

proc skipModifier*(a: Cursor): Cursor =
  result = a
  if result.kind == ParLe and result.typeKind in TypeModifiers:
    inc result

proc commonType(f, a: Cursor): Cursor =
  # XXX Refine
  result = a

proc typevarRematch(m: var Match; typeVar: SymId; f, a: Cursor) =
  let com = commonType(f, a)
  if com.kind == ParLe and com.tagId == ErrT:
    m.error InvalidRematch, f, a
    m.error.typeVar = typeVar
  elif matchesConstraint(m, typeVar, com):
    m.inferred[typeVar] = skipModifier(com)
  else:
    m.error ConstraintMismatch, typeImpl(typeVar), a

proc useArg(m: var Match; arg: Item) =
  m.args.addSubtree arg.n

proc singleArgImpl(m: var Match; f: var Cursor; arg: Item)

proc matchSymbol(m: var Match; f: Cursor; arg: Item) =
  let a = skipModifier(arg.typ)
  let fs = f.symId
  if isTypevar(fs):
    if m.inferred.contains(fs):
      typevarRematch(m, fs, m.inferred[fs], a)
    elif matchesConstraint(m, fs, a):
      m.inferred[fs] = a
    else:
      m.error ConstraintMismatch, f, a
  elif isObjectType(fs):
    if a.kind != Symbol:
      m.error InvalidMatch, f, a
    elif a.symId == fs:
      discard "direct match, no annotation required"
    else:
      var diff = 1
      for fparent in inheritanceChain(fs):
        if fparent == a.symId:
          m.args.addParLe OconvX, m.argInfo
          m.args.addIntLit diff, m.argInfo
          if m.flipped:
            dec m.inheritanceCosts, diff
          else:
            inc m.inheritanceCosts, diff
          inc m.opened
          diff = 0 # mark as success
          break
        inc diff
      if diff != 0:
        m.error InvalidMatch, f, a
      elif m.skippedMod == OutT:
        m.error UnavailableSubtypeRelation, f, a
  elif isConcept(fs):
    m.error NotImplementedConcept, f, a
  else:
    # fast check that works for aliases too:
    if a.kind == Symbol and a.symId == fs:
      discard "perfect match"
    else:
      var impl = typeImpl(fs)
      if impl.kind == ParLe and impl.tagId == ErrT:
        m.error InvalidMatch, f, a
      else:
        singleArgImpl(m, impl, arg)

proc cmpTypeBits(context: ptr SemContext; f, a: Cursor): int =
  if (f.kind == IntLit or f.kind == InlineInt) and
     (a.kind == IntLit or a.kind == InlineInt):
    result = typebits(context.g.config, f.load) - typebits(context.g.config, a.load)
  else:
    result = -1

proc matchIntegralType(m: var Match; f: var Cursor; arg: Item) =
  var a = skipModifier(arg.typ)
  if f.tag == a.tag:
    inc a
  else:
    m.error InvalidMatch, f, a
    return
  let forig = f
  inc f
  let cmp = cmpTypeBits(m.context, f, a)
  if cmp == 0:
    discard "same types"
  elif cmp > 0:
    # f has more bits than a, great!
    if m.skippedMod in {MutT, OutT}:
      m.error ImplicitConversionNotMutable, forig, forig
    else:
      m.args.addParLe HconvX, m.argInfo
      m.args.addSubtree forig
      inc m.intCosts
      inc m.opened
  else:
    m.error InvalidMatch, f, a
  inc f

proc matchArrayType(m: var Match; f: var Cursor; a: var Cursor) =
  if a.typeKind == ArrayT:
    var a1 = a
    var f1 = f
    inc a1
    inc f1
    skip a1
    skip f1
    let fLen = lengthOrd(m.context[], f1)
    let aLen = lengthOrd(m.context[], a1)
    if fLen.isNaN or aLen.isNaN:
      # match typevars
      linearMatch m, f, a
    elif fLen == aLen:
      inc f
      inc a
      linearMatch m, f, a
      skip f
      expectParRi m, f
    else:
      m.error InvalidMatch, f, a
  else:
    m.error InvalidMatch, f, a

proc isStringType*(a: Cursor): bool {.inline.} =
  result = a.kind == Symbol and a.symId == pool.syms.getOrIncl(StringName)
  #a.typeKind == StringT: StringT now unused!

proc isSomeStringType*(a: Cursor): bool {.inline.} =
  result = a.typeKind == CstringT or isStringType(a)

proc singleArgImpl(m: var Match; f: var Cursor; arg: Item) =
  case f.kind
  of Symbol:
    matchSymbol m, f, arg
    inc f
  of ParLe:
    let fk = f.typeKind
    case fk
    of MutT:
      var a = arg.typ
      if a.typeKind in {MutT, OutT, LentT}:
        inc a
      else:
        m.skippedMod = f.typeKind
      inc f
      singleArgImpl m, f, Item(n: arg.n, typ: a)
      expectParRi m, f
    of IntT, UIntT, FloatT, CharT:
      matchIntegralType m, f, arg
      expectParRi m, f
    of BoolT:
      var a = skipModifier(arg.typ)
      if a.typeKind != fk:
        m.error InvalidMatch, f, a
      inc f
      expectParRi m, f
    of InvokeT:
      # Keep in mind that (invok GenericHead Type1 Type2 ...)
      # is tyGenericInvokation in the old Nim. A generic *instance*
      # is always a nominal type ("Symbol") like
      # `(type GeneratedName (invok MyInst ConcreteTypeA ConcreteTypeB) (object ...))`.
      # This means a Symbol can match an InvokT.
      var a = skipModifier(arg.typ)
      if a.kind == Symbol:
        var t = getTypeSection(a.symId)
        if t.kind == TypeY and t.typevars.typeKind == InvokeT:
          linearMatch m, f, t.typevars
        else:
          m.error InvalidMatch, f, a
          skip f
      else:
        linearMatch m, f, a
    of RangeT:
      # for now acts the same as base type
      var a = skipModifier(arg.typ)
      inc f # skip to base type
      linearMatch m, f, a
      skip f
      skip f
      expectParRi m, f
    of ArrayT:
      var a = skipModifier(arg.typ)
      matchArrayType m, f, a
    of SetT, UncheckedArrayT, OpenArrayT:
      var a = skipModifier(arg.typ)
      linearMatch m, f, a
    of CstringT:
      var a = skipModifier(arg.typ)
      if a.typeKind == NilT:
        discard "ok"
        inc f
        expectParRi m, f
      elif isStringType(a) and arg.n.kind == StringLit:
        m.args.addParLe HconvX, m.argInfo
        m.args.addSubtree f
        inc m.opened
        inc m.intCosts
        inc f
        expectParRi m, f
      else:
        linearMatch m, f, a
    of PointerT:
      var a = skipModifier(arg.typ)
      case a.typeKind
      of NilT:
        discard "ok"
        inc f
        expectParRi m, f
      of PtrT:
        m.args.addParLe HconvX, m.argInfo
        m.args.addSubtree f
        inc m.opened
        inc m.intCosts
        inc f
        expectParRi m, f
      else:
        linearMatch m, f, a
    of PtrT, RefT:
      var a = skipModifier(arg.typ)
      case a.typeKind
      of NilT:
        discard "ok"
        inc f
        skip f
        expectParRi m, f
      else:
        linearMatch m, f, a
    of TypedescT:
      # do not skip modifier
      var a = arg.typ
      linearMatch m, f, a
    of VarargsT:
      discard "do not even advance f here"
      if m.firstVarargPosition < 0:
        m.firstVarargPosition = m.args.len
    of UntypedT, TypedT:
      # `typed` and `untyped` simply match everything:
      inc f
      expectParRi m, f
    of TupleT:
      let fOrig = f
      let aOrig = arg.typ
      var a = aOrig
      if a.typeKind != TupleT:
        m.error InvalidMatch, fOrig, aOrig
        skip f
      else:
        # skip tags:
        inc f
        inc a
        while f.kind != ParRi:
          if a.kind == ParRi:
            # len(f) > len(a)
            m.error InvalidMatch, fOrig, aOrig
          # only the type of the field is important:
          var ffld = asLocal(f).typ
          var afld = asLocal(a).typ
          linearMatch m, ffld, afld
          # skip fields:
          skip f
          skip a
        if a.kind != ParRi:
          # len(a) > len(f)
          m.error InvalidMatch, fOrig, aOrig
    of ProcT:
      var a = skipModifier(arg.typ)
      case a.typeKind
      of NilT:
        discard "ok"
        skip f
      else:
        procTypeMatch m, f, a
    of NoType, ObjectT, RefObjectT, PtrObjectT, EnumT, HoleyEnumT, VoidT, OutT, LentT, SinkT, NilT, OrT, AndT, NotT,
        ConceptT, DistinctT, StaticT, IterT, AutoT, SymKindT, TypeKindT, OrdinalT:
      m.error UnhandledTypeBug, f, f
  else:
    m.error MismatchBug, f, arg.typ

proc singleArg(m: var Match; f: var Cursor; arg: Item) =
  singleArgImpl(m, f, arg)
  if not m.err:
    m.useArg arg # since it was a match, copy it
    while m.opened > 0:
      m.args.addParRi()
      dec m.opened

proc typematch*(m: var Match; formal: Cursor; arg: Item) =
  var f = formal
  singleArg m, f, arg

type
  TypeRelation* = enum
    NoMatch
    ConvertibleMatch
    SubtypeMatch
    GenericMatch
    EqualMatch

proc usesConversion*(m: Match): bool {.inline.} =
  result = abs(m.inheritanceCosts) + m.intCosts > 0

proc classifyMatch*(m: Match): TypeRelation {.inline.} =
  if m.err:
    return NoMatch
  if m.intCosts != 0:
    return ConvertibleMatch
  if m.inheritanceCosts != 0:
    return SubtypeMatch
  if m.inferred.len != 0:
    # maybe a better way to track this
    return GenericMatch
  result = EqualMatch

proc sigmatchLoop(m: var Match; f: var Cursor; args: openArray[Item]) =
  var i = 0
  var isVarargs = false
  while f.kind != ParRi:
    m.skippedMod = NoType

    assert f.symKind == ParamY
    let param = asLocal(f)
    var ftyp = param.typ
    # This is subtle but only this order of `i >= args.len` checks
    # is correct for all cases (varargs/too few args/too many args)
    if ftyp != "varargs":
      if i >= args.len: break
      skip f
    else:
      isVarargs = true
      if i >= args.len: break
    m.argInfo = args[i].n.info

    singleArg m, ftyp, args[i]
    if m.err: break
    inc m.pos
    inc i
  if isVarargs:
    if m.firstVarargPosition < 0:
      m.firstVarargPosition = m.args.len
    skip f


iterator typeVars(fn: SymId): SymId =
  let res = tryLoadSym(fn)
  assert res.status == LacksNothing
  var c = res.decl
  if isRoutine(c.symKind):
    inc c # skip routine tag
    for i in 1..3:
      skip c # name, export marker, pattern
    if c.substructureKind == TypevarsS:
      inc c
      while c.kind != ParRi:
        if c.symKind == TypeVarY:
          var tv = c
          inc tv
          yield tv.symId
        skip c

proc collectDefaultValues(f: var Cursor): seq[Item] =
  result = @[]
  while f.symKind == ParamY:
    let param = asLocal(f)
    if param.val.kind == DotToken: break
    result.add Item(n: param.val, typ: param.typ)
    skip f

proc matchTypevars*(m: var Match; fn: FnCandidate; explicitTypeVars: Cursor) =
  m.tvars = initHashSet[SymId]()
  if fn.kind in RoutineKinds:
    var e = explicitTypeVars
    for v in typeVars(fn.sym):
      m.tvars.incl v
      if e.kind == DotToken: discard
      elif e.kind == ParRi:
        m.error.typeVar = v
        m.error0 MissingExplicitGenericParameter
        break
      else:
        if matchesConstraint(m, v, e):
          m.inferred[v] = e
        else:
          let res = tryLoadSym(v)
          assert res.status == LacksNothing
          var typevar = asTypevar(res.decl)
          assert typevar.kind == TypevarY
          m.error ConstraintMismatch, typevar.typ, e
        skip e
    if e.kind != DotToken and e.kind != ParRi:
      m.error0 ExtraGenericParameter
  elif explicitTypeVars.kind != DotToken:
    # aka there are explicit type vars
    if m.tvars.len == 0:
      m.error0 RoutineIsNotGeneric
      return

proc sigmatch*(m: var Match; fn: FnCandidate; args: openArray[Item];
               explicitTypeVars: Cursor) =
  assert fn.kind != NoSym or fn.sym == SymId(0)
  m.fn = fn
  matchTypevars m, fn, explicitTypeVars

  var f = fn.typ
  assert f == "params"
  inc f # "params"
  sigmatchLoop m, f, args

  if m.pos < args.len:
    # not all arguments where used, error:
    m.error0 TooManyArguments
  elif f.kind != ParRi:
    # use default values for these parameters, but this needs to be done
    # properly with generics etc. so we use a helper `args` seq and pretend
    # the programmer had written out these arguments:
    let moreArgs = collectDefaultValues(f)
    sigmatchLoop m, f, moreArgs
    if f.kind != ParRi:
      m.error0 TooFewArguments

  if f.kind == ParRi:
    inc f
    m.returnType = f # return type follows the parameters in the token stream

  # check all type vars have a value:
  if not m.err and fn.kind in RoutineKinds:
    for v in typeVars(fn.sym):
      let inf = m.inferred.getOrDefault(v)
      if inf == default(Cursor):
        m.error.typeVar = v
        m.error0 CouldNotInferTypeVar
        break
      m.typeArgs.addSubtree inf

type
  DisambiguationResult* = enum
    NobodyWins,
    FirstWins,
    SecondWins

proc cmpMatches*(a, b: Match): DisambiguationResult =
  assert not a.err
  assert not b.err
  if a.inheritanceCosts < b.inheritanceCosts:
    result = FirstWins
  elif a.inheritanceCosts > b.inheritanceCosts:
    result = SecondWins
  elif a.intCosts < b.intCosts:
    result = FirstWins
  elif a.intCosts > b.intCosts:
    result = SecondWins
  else:
    let diff = a.inferred.len - b.inferred.len
    if diff < 0:
      result = FirstWins
    elif diff > 0:
      result = SecondWins
    else:
      result = NobodyWins

# How to implement named parameters: In a preprocessing step
# The signature is matched against the named parameters. The
# call is then reordered to `f`'s needs. This keeps the common case fast
# where no named parameters are used at all.

