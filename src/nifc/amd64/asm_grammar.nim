# GENERATED BY NifGram. DO NOT EDIT!
# ORIGINAL FILE: ./src/nifc/amd64/asm_grammar.nif
proc genInstruction(c: var Context): bool



proc genDirective(c: var Context): bool =
  when declared(handleDirective):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, GlobalT):
    emit(c, ".global ")
    if not lookupSym(c):
      error(c, "SYMBOL expected")
    nl(c)
    kw2 = matchParRi(c)
  if not kw2: return false
  when declared(handleDirective):
    handleDirective(c, before1)
  return true

proc genCode(c: var Context): bool =
  when declared(handleCode):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, TextT):
    emit(c, ".section .text")
    nl(c)
    var sym3 = declareSym(c)
    if not success(sym3):
      error(c, "SYMBOLDEF expected")
    emit(c, ":")
    nl(c)
    var om4 = false
    while not peekParRi(c):
      if not genInstruction(c):
        break
      else:
        om4 = true
    if not om4:
      error(c, "invalid Code")
    kw2 = matchParRi(c)
  if not kw2: return false
  nl(c)
  
  when declared(handleCode):
    handleCode(c, before1)
  return true

proc genExternDecl(c: var Context): bool =
  when declared(handleExternDecl):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, ExternT):
    emitTag(c, "extern")
    var sym3 = declareSym(c)
    if not success(sym3):
      error(c, "SYMBOLDEF expected")
    nl(c)
    kw2 = matchParRi(c)
  if not kw2: return false
  when declared(handleExternDecl):
    handleExternDecl(c, before1)
  return true

proc genDataValue(c: var Context): bool =
  when declared(handleDataValue):
    var before1 = save(c)
  var or2 = false
  block or3:
    if matchStringLit(c):
      or2 = true
      break or3
    if matchIntLit(c):
      or2 = true
      break or3
    if matchUIntLit(c):
      or2 = true
      break or3
    if matchFloatLit(c):
      or2 = true
      break or3
  if not or2: return false
  when declared(handleDataValue):
    handleDataValue(c, before1)
  return true

proc genDataKey(c: var Context): bool =
  when declared(handleDataKey):
    var before1 = save(c)
  var or2 = false
  block or3:
    if genDataValue(c):
      or2 = true
      break or3
    var kw4 = false
    if isTag(c, TimesT):
      emitTag(c, "times")
      if not matchIntLit(c):
        error(c, "INTLIT expected")
      if not genDataValue(c):
        error(c, "DataValue expected")
      kw4 = matchParRi(c)
    if kw4:
      or2 = true
      break or3
  if not or2: return false
  nl(c)
  
  when declared(handleDataKey):
    handleDataKey(c, before1)
  return true

proc genDataDecl(c: var Context): bool =
  when declared(handleDataDecl):
    var before1 = save(c)
  var sym2 = declareSym(c)
  if not success(sym2): return false
  emit(c, ":")
  
  nl(c)
  
  var om3 = false
  while not peekParRi(c):
    var or4 = false
    block or5:
      var kw6 = false
      if isTag(c, StringT):
        emit(c, ".string ")
        if not genDataKey(c):
          error(c, "DataKey expected")
        kw6 = matchParRi(c)
      if kw6:
        or4 = true
        break or5
      var kw7 = false
      if isTag(c, ByteT):
        emit(c, ".byte ")
        if not genDataKey(c):
          error(c, "DataKey expected")
        kw7 = matchParRi(c)
      if kw7:
        or4 = true
        break or5
      var kw8 = false
      if isTag(c, WordT):
        emit(c, ".2byte ")
        if not genDataKey(c):
          error(c, "DataKey expected")
        kw8 = matchParRi(c)
      if kw8:
        or4 = true
        break or5
      var kw9 = false
      if isTag(c, LongT):
        emit(c, ".4byte ")
        if not genDataKey(c):
          error(c, "DataKey expected")
        kw9 = matchParRi(c)
      if kw9:
        or4 = true
        break or5
      var kw10 = false
      if isTag(c, QuadT):
        emit(c, ".8byte ")
        if not genDataKey(c):
          error(c, "DataKey expected")
        kw10 = matchParRi(c)
      if kw10:
        or4 = true
        break or5
    if not or4:
      break
    else:
      om3 = true
  if not om3: return false
  nl(c)
  
  when declared(handleDataDecl):
    handleDataDecl(c, before1)
  return true

proc genData(c: var Context): bool =
  when declared(handleData):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, DataT):
    emit(c, ".section .bss")
    nl(c)
    var zm3 = true
    while not peekParRi(c):
      if not genDataDecl(c):
        zm3 = false
        break
    if not zm3:
      error(c, "invalid Data")
    kw2 = matchParRi(c)
  if not kw2: return false
  nl(c)
  
  when declared(handleData):
    handleData(c, before1)
  return true

proc genRodata(c: var Context): bool =
  when declared(handleRodata):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, RodataT):
    emit(c, ".section .rodata")
    nl(c)
    var zm3 = true
    while not peekParRi(c):
      if not genDataDecl(c):
        zm3 = false
        break
    if not zm3:
      error(c, "invalid Rodata")
    kw2 = matchParRi(c)
  if not kw2: return false
  nl(c)
  
  when declared(handleRodata):
    handleRodata(c, before1)
  return true

proc genSection(c: var Context): bool =
  when declared(handleSection):
    var before1 = save(c)
  var or2 = false
  block or3:
    if genDirective(c):
      or2 = true
      break or3
    if genExternDecl(c):
      or2 = true
      break or3
    if genCode(c):
      or2 = true
      break or3
    if genData(c):
      or2 = true
      break or3
    if genRodata(c):
      or2 = true
      break or3
  if not or2: return false
  when declared(handleSection):
    handleSection(c, before1)
  return true

proc genModule(c: var Context): bool =
  when declared(handleModule):
    var before1 = save(c)
  var kw2 = false
  if isTag(c, StmtsT):
    emit(c, ".intel_syntax noprefix")
    nl(c)
    nl(c)
    var zm3 = true
    while not peekParRi(c):
      if not genSection(c):
        zm3 = false
        break
    if not zm3:
      error(c, "invalid Module")
    kw2 = matchParRi(c)
  if not kw2: return false
  when declared(handleModule):
    handleModule(c, before1)
  return true

proc genReg(c: var Context): bool =
  when declared(handleReg):
    var before1 = save(c)
  var or2 = false
  block or3:
    if matchAndEmitTag(c, RaxT, "rax"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RbxT, "rbx"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RcxT, "rcx"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RdxT, "rdx"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RsiT, "rsi"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RdiT, "rdi"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RbpT, "rbp"):
      or2 = true
      break or3
    if matchAndEmitTag(c, RspT, "rsp"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R8T, "r8"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R9T, "r9"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R10T, "r10"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R11T, "r11"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R12T, "r12"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R13T, "r13"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R14T, "r14"):
      or2 = true
      break or3
    if matchAndEmitTag(c, R15T, "r15"):
      or2 = true
      break or3
    var kw4 = false
    if isTag(c, Rsp2T):
      emit(c, "rsp")
      kw4 = matchParRi(c)
    if kw4:
      or2 = true
      break or3
  if not or2: return false
  when declared(handleReg):
    handleReg(c, before1)
  return true

proc genFpReg(c: var Context): bool =
  when declared(handleFpReg):
    var before1 = save(c)
  var or2 = false
  block or3:
    if matchAndEmitTag(c, Xmm0T, "xmm0"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm1T, "xmm1"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm2T, "xmm2"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm3T, "xmm3"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm4T, "xmm4"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm6T, "xmm6"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm7T, "xmm7"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm8T, "xmm8"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm9T, "xmm9"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm10T, "xmm10"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm11T, "xmm11"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm12T, "xmm12"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm13T, "xmm13"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm14T, "xmm14"):
      or2 = true
      break or3
    if matchAndEmitTag(c, Xmm15T, "xmm15"):
      or2 = true
      break or3
  if not or2: return false
  when declared(handleFpReg):
    handleFpReg(c, before1)
  return true

proc genPrimary(c: var Context): bool =
  when declared(handlePrimary):
    var before1 = save(c)
  var or2 = false
  block or3:
    if genReg(c):
      or2 = true
      break or3
    if genFpReg(c):
      or2 = true
      break or3
    if lookupSym(c):
      or2 = true
      break or3
    var kw4 = false
    if isTag(c, RelT):
      emit(c, "[rip+")
      if not lookupSym(c):
        error(c, "SYMBOL expected")
      emit(c, "]")
      kw4 = matchParRi(c)
    if kw4:
      or2 = true
      break or3
    var kw5 = false
    if isTag(c, FsT):
      emit(c, "fs:[")
      if not lookupSym(c):
        error(c, "SYMBOL expected")
      emit(c, "@TPOFF]")
      kw5 = matchParRi(c)
    if kw5:
      or2 = true
      break or3
    if matchIntLit(c):
      or2 = true
      break or3
    if matchUIntLit(c):
      or2 = true
      break or3
  if not or2: return false
  when declared(handlePrimary):
    handlePrimary(c, before1)
  return true

proc genExpr(c: var Context): bool =
  when declared(handleExpr):
    var before1 = save(c)
  var or2 = false
  block or3:
    if genPrimary(c):
      or2 = true
      break or3
    var kw4 = false
    if isTag(c, Mem1T):
      emit(c, "[")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "]")
      kw4 = matchParRi(c)
    if kw4:
      or2 = true
      break or3
    var kw5 = false
    if isTag(c, Mem2T):
      emit(c, "[")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "+")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "]")
      kw5 = matchParRi(c)
    if kw5:
      or2 = true
      break or3
    var kw6 = false
    if isTag(c, Mem3T):
      emit(c, "[")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "+")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "*")
      if not matchIntLit(c):
        error(c, "INTLIT expected")
      emit(c, "]")
      kw6 = matchParRi(c)
    if kw6:
      or2 = true
      break or3
    var kw7 = false
    if isTag(c, Mem4T):
      emit(c, "[")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "+")
      if not genPrimary(c):
        error(c, "Primary expected")
      emit(c, "*")
      if not matchIntLit(c):
        error(c, "INTLIT expected")
      emit(c, "+")
      if not matchIntLit(c):
        error(c, "INTLIT expected")
      emit(c, "]")
      kw7 = matchParRi(c)
    if kw7:
      or2 = true
      break or3
    var kw8 = false
    if isTag(c, ByteatT):
      emit(c, "BYTE PTR ")
      if not genPrimary(c):
        error(c, "Primary expected")
      kw8 = matchParRi(c)
    if kw8:
      or2 = true
      break or3
    var kw9 = false
    if isTag(c, WordatT):
      emit(c, "WORD PTR ")
      if not genPrimary(c):
        error(c, "Primary expected")
      kw9 = matchParRi(c)
    if kw9:
      or2 = true
      break or3
  if not or2: return false
  when declared(handleExpr):
    handleExpr(c, before1)
  return true

proc genLabel(c: var Context): bool =
  when declared(handleLabel):
    var before1 = save(c)
  if not lookupSym(c): return false
  when declared(handleLabel):
    handleLabel(c, before1)
  return true

proc genInstruction(c: var Context): bool =
  when declared(handleInstruction):
    var before1 = save(c)
  var or2 = false
  block or3:
    var kw4 = false
    if isTag(c, MovT):
      emitTag(c, "mov")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw4 = matchParRi(c)
    if kw4:
      or2 = true
      break or3
    var kw5 = false
    if isTag(c, MovapdT):
      emitTag(c, "movapd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw5 = matchParRi(c)
    if kw5:
      or2 = true
      break or3
    var kw6 = false
    if isTag(c, MovsdT):
      emitTag(c, "movsd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw6 = matchParRi(c)
    if kw6:
      or2 = true
      break or3
    var kw7 = false
    if isTag(c, LeaT):
      emitTag(c, "lea")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw7 = matchParRi(c)
    if kw7:
      or2 = true
      break or3
    var kw8 = false
    if isTag(c, AddT):
      emitTag(c, "add")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw8 = matchParRi(c)
    if kw8:
      or2 = true
      break or3
    var kw9 = false
    if isTag(c, SubT):
      emitTag(c, "sub")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw9 = matchParRi(c)
    if kw9:
      or2 = true
      break or3
    var kw10 = false
    if isTag(c, MulT):
      emitTag(c, "mul")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw10 = matchParRi(c)
    if kw10:
      or2 = true
      break or3
    var kw11 = false
    if isTag(c, ImulT):
      emitTag(c, "imul")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw11 = matchParRi(c)
    if kw11:
      or2 = true
      break or3
    var kw12 = false
    if isTag(c, DivT):
      emitTag(c, "div")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw12 = matchParRi(c)
    if kw12:
      or2 = true
      break or3
    var kw13 = false
    if isTag(c, IdivT):
      emitTag(c, "idiv")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw13 = matchParRi(c)
    if kw13:
      or2 = true
      break or3
    var kw14 = false
    if isTag(c, XorT):
      emitTag(c, "xor")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw14 = matchParRi(c)
    if kw14:
      or2 = true
      break or3
    var kw15 = false
    if isTag(c, OrT):
      emitTag(c, "or")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw15 = matchParRi(c)
    if kw15:
      or2 = true
      break or3
    var kw16 = false
    if isTag(c, AndT):
      emitTag(c, "and")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw16 = matchParRi(c)
    if kw16:
      or2 = true
      break or3
    var kw17 = false
    if isTag(c, ShlT):
      emitTag(c, "shl")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw17 = matchParRi(c)
    if kw17:
      or2 = true
      break or3
    var kw18 = false
    if isTag(c, ShrT):
      emitTag(c, "shr")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw18 = matchParRi(c)
    if kw18:
      or2 = true
      break or3
    var kw19 = false
    if isTag(c, SalT):
      emitTag(c, "sal")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw19 = matchParRi(c)
    if kw19:
      or2 = true
      break or3
    var kw20 = false
    if isTag(c, SarT):
      emitTag(c, "sar")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw20 = matchParRi(c)
    if kw20:
      or2 = true
      break or3
    var kw21 = false
    if isTag(c, AddsdT):
      emitTag(c, "addsd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw21 = matchParRi(c)
    if kw21:
      or2 = true
      break or3
    var kw22 = false
    if isTag(c, SubsdT):
      emitTag(c, "subsd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw22 = matchParRi(c)
    if kw22:
      or2 = true
      break or3
    var kw23 = false
    if isTag(c, MulsdT):
      emitTag(c, "mulsd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw23 = matchParRi(c)
    if kw23:
      or2 = true
      break or3
    var kw24 = false
    if isTag(c, DivsdT):
      emitTag(c, "divsd")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw24 = matchParRi(c)
    if kw24:
      or2 = true
      break or3
    var kw25 = false
    if isTag(c, PushT):
      emitTag(c, "push")
      if not genExpr(c):
        error(c, "Expr expected")
      kw25 = matchParRi(c)
    if kw25:
      or2 = true
      break or3
    var kw26 = false
    if isTag(c, PopT):
      emitTag(c, "pop")
      if not genExpr(c):
        error(c, "Expr expected")
      kw26 = matchParRi(c)
    if kw26:
      or2 = true
      break or3
    var kw27 = false
    if isTag(c, IncT):
      emitTag(c, "inc")
      if not genExpr(c):
        error(c, "Expr expected")
      kw27 = matchParRi(c)
    if kw27:
      or2 = true
      break or3
    var kw28 = false
    if isTag(c, DecT):
      emitTag(c, "dec")
      if not genExpr(c):
        error(c, "Expr expected")
      kw28 = matchParRi(c)
    if kw28:
      or2 = true
      break or3
    var kw29 = false
    if isTag(c, NegT):
      emitTag(c, "neg")
      if not genExpr(c):
        error(c, "Expr expected")
      kw29 = matchParRi(c)
    if kw29:
      or2 = true
      break or3
    var kw30 = false
    if isTag(c, NotT):
      emitTag(c, "not")
      if not genExpr(c):
        error(c, "Expr expected")
      kw30 = matchParRi(c)
    if kw30:
      or2 = true
      break or3
    var kw31 = false
    if isTag(c, CmpT):
      emitTag(c, "cmp")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw31 = matchParRi(c)
    if kw31:
      or2 = true
      break or3
    var kw32 = false
    if isTag(c, TestT):
      emitTag(c, "test")
      if not genExpr(c):
        error(c, "Expr expected")
      emit(c, ", ")
      if not genExpr(c):
        error(c, "Expr expected")
      kw32 = matchParRi(c)
    if kw32:
      or2 = true
      break or3
    var kw33 = false
    if isTag(c, CallT):
      emitTag(c, "call")
      if not genExpr(c):
        error(c, "Expr expected")
      kw33 = matchParRi(c)
    if kw33:
      or2 = true
      break or3
    var kw34 = false
    if isTag(c, LabT):
      emit(c, "")
      var sym35 = declareSym(c)
      if not success(sym35):
        error(c, "SYMBOLDEF expected")
      emit(c, ":")
      kw34 = matchParRi(c)
    if kw34:
      or2 = true
      break or3
    var kw36 = false
    if isTag(c, LooplabT):
      emit(c, "")
      var sym37 = declareSym(c)
      if not success(sym37):
        error(c, "SYMBOLDEF expected")
      emit(c, ":")
      kw36 = matchParRi(c)
    if kw36:
      or2 = true
      break or3
    var kw38 = false
    if isTag(c, SeteT):
      emitTag(c, "sete")
      if not genExpr(c):
        error(c, "Expr expected")
      kw38 = matchParRi(c)
    if kw38:
      or2 = true
      break or3
    var kw39 = false
    if isTag(c, SetaT):
      emitTag(c, "seta")
      if not genExpr(c):
        error(c, "Expr expected")
      kw39 = matchParRi(c)
    if kw39:
      or2 = true
      break or3
    var kw40 = false
    if isTag(c, SetaeT):
      emitTag(c, "setae")
      if not genExpr(c):
        error(c, "Expr expected")
      kw40 = matchParRi(c)
    if kw40:
      or2 = true
      break or3
    var kw41 = false
    if isTag(c, SetbT):
      emitTag(c, "setb")
      if not genExpr(c):
        error(c, "Expr expected")
      kw41 = matchParRi(c)
    if kw41:
      or2 = true
      break or3
    var kw42 = false
    if isTag(c, SetbeT):
      emitTag(c, "setbe")
      if not genExpr(c):
        error(c, "Expr expected")
      kw42 = matchParRi(c)
    if kw42:
      or2 = true
      break or3
    var kw43 = false
    if isTag(c, SetgT):
      emitTag(c, "setg")
      if not genExpr(c):
        error(c, "Expr expected")
      kw43 = matchParRi(c)
    if kw43:
      or2 = true
      break or3
    var kw44 = false
    if isTag(c, SetgeT):
      emitTag(c, "setge")
      if not genExpr(c):
        error(c, "Expr expected")
      kw44 = matchParRi(c)
    if kw44:
      or2 = true
      break or3
    var kw45 = false
    if isTag(c, SetlT):
      emitTag(c, "setl")
      if not genExpr(c):
        error(c, "Expr expected")
      kw45 = matchParRi(c)
    if kw45:
      or2 = true
      break or3
    var kw46 = false
    if isTag(c, SetleT):
      emitTag(c, "setle")
      if not genExpr(c):
        error(c, "Expr expected")
      kw46 = matchParRi(c)
    if kw46:
      or2 = true
      break or3
    var kw47 = false
    if isTag(c, SetzT):
      emitTag(c, "setz")
      if not genExpr(c):
        error(c, "Expr expected")
      kw47 = matchParRi(c)
    if kw47:
      or2 = true
      break or3
    var kw48 = false
    if isTag(c, SetcT):
      emitTag(c, "setc")
      if not genExpr(c):
        error(c, "Expr expected")
      kw48 = matchParRi(c)
    if kw48:
      or2 = true
      break or3
    var kw49 = false
    if isTag(c, SetoT):
      emitTag(c, "seto")
      if not genExpr(c):
        error(c, "Expr expected")
      kw49 = matchParRi(c)
    if kw49:
      or2 = true
      break or3
    var kw50 = false
    if isTag(c, SetsT):
      emitTag(c, "sets")
      if not genExpr(c):
        error(c, "Expr expected")
      kw50 = matchParRi(c)
    if kw50:
      or2 = true
      break or3
    var kw51 = false
    if isTag(c, SetpT):
      emitTag(c, "setp")
      if not genExpr(c):
        error(c, "Expr expected")
      kw51 = matchParRi(c)
    if kw51:
      or2 = true
      break or3
    var kw52 = false
    if isTag(c, SetneT):
      emitTag(c, "setne")
      if not genExpr(c):
        error(c, "Expr expected")
      kw52 = matchParRi(c)
    if kw52:
      or2 = true
      break or3
    var kw53 = false
    if isTag(c, SetnaT):
      emitTag(c, "setna")
      if not genExpr(c):
        error(c, "Expr expected")
      kw53 = matchParRi(c)
    if kw53:
      or2 = true
      break or3
    var kw54 = false
    if isTag(c, SetnaeT):
      emitTag(c, "setnae")
      if not genExpr(c):
        error(c, "Expr expected")
      kw54 = matchParRi(c)
    if kw54:
      or2 = true
      break or3
    var kw55 = false
    if isTag(c, SetnbT):
      emitTag(c, "setnb")
      if not genExpr(c):
        error(c, "Expr expected")
      kw55 = matchParRi(c)
    if kw55:
      or2 = true
      break or3
    var kw56 = false
    if isTag(c, SetnbeT):
      emitTag(c, "setnbe")
      if not genExpr(c):
        error(c, "Expr expected")
      kw56 = matchParRi(c)
    if kw56:
      or2 = true
      break or3
    var kw57 = false
    if isTag(c, SetngT):
      emitTag(c, "setng")
      if not genExpr(c):
        error(c, "Expr expected")
      kw57 = matchParRi(c)
    if kw57:
      or2 = true
      break or3
    var kw58 = false
    if isTag(c, SetngeT):
      emitTag(c, "setnge")
      if not genExpr(c):
        error(c, "Expr expected")
      kw58 = matchParRi(c)
    if kw58:
      or2 = true
      break or3
    var kw59 = false
    if isTag(c, SetnlT):
      emitTag(c, "setnl")
      if not genExpr(c):
        error(c, "Expr expected")
      kw59 = matchParRi(c)
    if kw59:
      or2 = true
      break or3
    var kw60 = false
    if isTag(c, SetnleT):
      emitTag(c, "setnle")
      if not genExpr(c):
        error(c, "Expr expected")
      kw60 = matchParRi(c)
    if kw60:
      or2 = true
      break or3
    var kw61 = false
    if isTag(c, SetnzT):
      emitTag(c, "setnz")
      if not genExpr(c):
        error(c, "Expr expected")
      kw61 = matchParRi(c)
    if kw61:
      or2 = true
      break or3
    var kw62 = false
    if isTag(c, JmpT):
      emitTag(c, "jmp")
      if not genLabel(c):
        error(c, "Label expected")
      kw62 = matchParRi(c)
    if kw62:
      or2 = true
      break or3
    var kw63 = false
    if isTag(c, JloopT):
      emit(c, "jmp")
      if not genLabel(c):
        error(c, "Label expected")
      kw63 = matchParRi(c)
    if kw63:
      or2 = true
      break or3
    var kw64 = false
    if isTag(c, JeT):
      emitTag(c, "je")
      if not genLabel(c):
        error(c, "Label expected")
      kw64 = matchParRi(c)
    if kw64:
      or2 = true
      break or3
    var kw65 = false
    if isTag(c, JneT):
      emitTag(c, "jne")
      if not genLabel(c):
        error(c, "Label expected")
      kw65 = matchParRi(c)
    if kw65:
      or2 = true
      break or3
    var kw66 = false
    if isTag(c, JzT):
      emitTag(c, "jz")
      if not genLabel(c):
        error(c, "Label expected")
      kw66 = matchParRi(c)
    if kw66:
      or2 = true
      break or3
    var kw67 = false
    if isTag(c, JnzT):
      emitTag(c, "jnz")
      if not genLabel(c):
        error(c, "Label expected")
      kw67 = matchParRi(c)
    if kw67:
      or2 = true
      break or3
    var kw68 = false
    if isTag(c, JgT):
      emitTag(c, "jg")
      if not genLabel(c):
        error(c, "Label expected")
      kw68 = matchParRi(c)
    if kw68:
      or2 = true
      break or3
    var kw69 = false
    if isTag(c, JngT):
      emitTag(c, "jng")
      if not genLabel(c):
        error(c, "Label expected")
      kw69 = matchParRi(c)
    if kw69:
      or2 = true
      break or3
    var kw70 = false
    if isTag(c, JgeT):
      emitTag(c, "jge")
      if not genLabel(c):
        error(c, "Label expected")
      kw70 = matchParRi(c)
    if kw70:
      or2 = true
      break or3
    var kw71 = false
    if isTag(c, JngeT):
      emitTag(c, "jnge")
      if not genLabel(c):
        error(c, "Label expected")
      kw71 = matchParRi(c)
    if kw71:
      or2 = true
      break or3
    var kw72 = false
    if isTag(c, JaT):
      emitTag(c, "ja")
      if not genLabel(c):
        error(c, "Label expected")
      kw72 = matchParRi(c)
    if kw72:
      or2 = true
      break or3
    var kw73 = false
    if isTag(c, JnaT):
      emitTag(c, "jna")
      if not genLabel(c):
        error(c, "Label expected")
      kw73 = matchParRi(c)
    if kw73:
      or2 = true
      break or3
    var kw74 = false
    if isTag(c, JaeT):
      emitTag(c, "jae")
      if not genLabel(c):
        error(c, "Label expected")
      kw74 = matchParRi(c)
    if kw74:
      or2 = true
      break or3
    var kw75 = false
    if isTag(c, JnaeT):
      emitTag(c, "jnae")
      if not genLabel(c):
        error(c, "Label expected")
      kw75 = matchParRi(c)
    if kw75:
      or2 = true
      break or3
    if matchAndEmitTag(c, NopT, "nop"):
      or2 = true
      break or3
    var kw76 = false
    if isTag(c, SkipT):
      emit(c, "")
      if not matchAny(c):
        error(c, "ANY expected")
      kw76 = matchParRi(c)
    if kw76:
      or2 = true
      break or3
    if matchAndEmitTag(c, RetT, "ret"):
      or2 = true
      break or3
    if matchAndEmitTag(c, SyscallT, "syscall"):
      or2 = true
      break or3
    var kw77 = false
    if isTag(c, CommentT):
      emit(c, "# ")
      var or78 = false
      block or79:
        if matchIdent(c):
          or78 = true
          break or79
        if lookupSym(c):
          or78 = true
          break or79
        if matchStringLit(c):
          or78 = true
          break or79
      if not or78:
        error(c, "invalid Instruction")
      kw77 = matchParRi(c)
    if kw77:
      or2 = true
      break or3
  if not or2: return false
  nl(c)
  
  when declared(handleInstruction):
    handleInstruction(c, before1)
  return true
