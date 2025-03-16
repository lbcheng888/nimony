# Generated by tools/gen_tags.nim from doc/tags.md. DO NOT EDIT!


type
  NifcExpr* = enum
    NoExpr
    SufC = (2, "suf")  ## literal with suffix annotation
    AtC = (3, "at")  ## array indexing operation
    DerefC = (4, "deref")  ## pointer deref operation
    DotC = (5, "dot")  ## object field selection
    PatC = (6, "pat")  ## pointer indexing operation
    ParC = (7, "par")  ## syntactic parenthesis
    AddrC = (8, "addr")  ## address of operation
    NilC = (9, "nil")  ## nil pointer value
    InfC = (10, "inf")  ## positive infinity floating point value
    NeginfC = (11, "neginf")  ## negative infinity floating point value
    NanC = (12, "nan")  ## NaN floating point value
    FalseC = (13, "false")  ## boolean `false` value
    TrueC = (14, "true")  ## boolean `true` value
    AndC = (15, "and")  ## boolean `and` operation
    OrC = (16, "or")  ## boolean `or` operation
    NotC = (18, "not")  ## boolean `not` operation
    NegC = (19, "neg")  ## negation operation
    SizeofC = (20, "sizeof")  ## `sizeof` operation
    AlignofC = (21, "alignof")  ## `alignof` operation
    OffsetofC = (22, "offsetof")  ## `offsetof` operation
    OconstrC = (23, "oconstr")  ## object constructor
    AconstrC = (24, "aconstr")  ## array constructor
    AddC = (30, "add")
    SubC = (31, "sub")
    MulC = (32, "mul")
    DivC = (33, "div")
    ModC = (34, "mod")
    ShrC = (35, "shr")
    ShlC = (36, "shl")
    BitandC = (37, "bitand")
    BitorC = (38, "bitor")
    BitxorC = (39, "bitxor")
    BitnotC = (40, "bitnot")
    EqC = (41, "eq")
    NeqC = (42, "neq")
    LeC = (43, "le")
    LtC = (44, "lt")
    CastC = (45, "cast")  ## `cast` operation
    ConvC = (46, "conv")  ## type conversion
    CallC = (47, "call")  ## call operation
    ErrvC = (256, "errv")  ## error flag for `NIFC`

proc rawTagIsNifcExpr*(raw: uint32): bool {.inline.} =
  let r = raw - 2'u32
  r <= 255'u32 and r.uint8 in {0'u8, 1'u8, 2'u8, 3'u8, 4'u8, 5'u8, 6'u8, 7'u8, 8'u8, 9'u8, 10'u8, 11'u8, 12'u8, 13'u8, 14'u8, 16'u8, 17'u8, 18'u8, 19'u8, 20'u8, 21'u8, 22'u8, 28'u8, 29'u8, 30'u8, 31'u8, 32'u8, 33'u8, 34'u8, 35'u8, 36'u8, 37'u8, 38'u8, 39'u8, 40'u8, 41'u8, 42'u8, 43'u8, 44'u8, 45'u8, 254'u8}

type
  NifcStmt* = enum
    NoStmt
    CallS = (47, "call")  ## call operation
    GvarS = (51, "gvar")  ## global variable declaration
    TvarS = (52, "tvar")  ## thread local variable declaration
    VarS = (53, "var")  ## variable declaration
    ConstS = (55, "const")  ## const variable declaration
    ProcS = (64, "proc")  ## proc declaration
    TypeS = (71, "type")  ## type declaration
    EmitS = (76, "emit")  ## emit statement
    AsgnS = (77, "asgn")  ## assignment statement
    ScopeS = (78, "scope")  ## explicit scope annotation, like `stmts`
    IfS = (79, "if")  ## if statement header
    BreakS = (84, "break")  ## `break` statement
    WhileS = (87, "while")  ## `while` statement
    CaseS = (88, "case")  ## `case` statement
    LabS = (90, "lab")  ## label, target of a `jmp` instruction
    JmpS = (91, "jmp")  ## jump/goto instruction
    RetS = (92, "ret")  ## `return` instruction
    StmtsS = (94, "stmts")  ## list of statements
    ImpS = (134, "imp")  ## import declaration
    InclS = (136, "incl")  ## `#include` statement or `incl` set operation
    DiscardS = (147, "discard")  ## `discard` statement
    TryS = (148, "try")  ## `try` statement
    RaiseS = (149, "raise")  ## `raise` statement
    OnerrS = (150, "onerr")  ## error handling statement

proc rawTagIsNifcStmt*(raw: uint32): bool {.inline.} =
  let r = raw - 47'u32
  r <= 255'u32 and r.uint8 in {0'u8, 4'u8, 5'u8, 6'u8, 8'u8, 17'u8, 24'u8, 29'u8, 30'u8, 31'u8, 32'u8, 37'u8, 40'u8, 41'u8, 43'u8, 44'u8, 45'u8, 47'u8, 87'u8, 89'u8, 100'u8, 101'u8, 102'u8, 103'u8}

type
  NifcType* = enum
    NoType
    ParamsT = (95, "params")  ## list of proc parameters, also used as a "proc type"
    UnionT = (96, "union")  ## union declaration
    ObjectT = (97, "object")  ## object type declaration
    EnumT = (98, "enum")  ## enum type declaration
    ProctypeT = (99, "proctype")  ## proc type declaration (soon obsolete, use params instead)
    IT = (104, "i")  ## `int` builtin type
    UT = (105, "u")  ## `uint` builtin type
    FT = (106, "f")  ## `float` builtin type
    CT = (107, "c")  ## `char` builtin type
    BoolT = (108, "bool")  ## `bool` builtin type
    VoidT = (109, "void")  ## `void` return type
    PtrT = (110, "ptr")  ## `ptr` type contructor
    ArrayT = (111, "array")  ## `array` type constructor
    FlexarrayT = (112, "flexarray")  ## `flexarray` type constructor
    AptrT = (113, "aptr")  ## "pointer to array of" type constructor

proc rawTagIsNifcType*(raw: uint32): bool {.inline.} =
  let r = raw - 95'u32
  r <= 255'u32 and r.uint8 in {0'u8, 1'u8, 2'u8, 3'u8, 4'u8, 9'u8, 10'u8, 11'u8, 12'u8, 13'u8, 14'u8, 15'u8, 16'u8, 17'u8, 18'u8}

type
  NifcOther* = enum
    NoSub
    KvU = (28, "kv")  ## key-value pair
    RangeU = (49, "range")  ## `(range a b)` construct
    RangesU = (50, "ranges")
    ParamU = (54, "param")  ## parameter declaration
    TypevarU = (61, "typevar")  ## type variable declaration
    EfldU = (62, "efld")  ## enum field declaration
    FldU = (63, "fld")  ## field declaration
    ElifU = (81, "elif")  ## pair of (condition, action)
    ElseU = (82, "else")  ## `else` action
    OfU = (89, "of")  ## `of` branch within a `case` statement
    PragmasU = (129, "pragmas")  ## begin of pragma section

proc rawTagIsNifcOther*(raw: uint32): bool {.inline.} =
  let r = raw - 28'u32
  r <= 255'u32 and r.uint8 in {0'u8, 21'u8, 22'u8, 26'u8, 33'u8, 34'u8, 35'u8, 53'u8, 54'u8, 61'u8, 101'u8}

type
  NifcPragma* = enum
    NoPragma
    InlineP = (123, "inline")  ## `inline` proc annotation
    NoinlineP = (124, "noinline")  ## `noinline` proc annotation
    AttrP = (125, "attr")  ## general attribute annoation
    VarargsP = (126, "varargs")  ## `varargs` proc annotation
    WasP = (127, "was")
    SelectanyP = (128, "selectany")
    AlignP = (131, "align")
    BitsP = (132, "bits")
    VectorP = (133, "vector")
    NodeclP = (135, "nodecl")  ## `nodecl` annotation
    RaisesP = (151, "raises")  ## proc annotation
    ErrsP = (152, "errs")  ## proc annotation
    StaticP = (153, "static")  ## `static` type or annotation

proc rawTagIsNifcPragma*(raw: uint32): bool {.inline.} =
  let r = raw - 123'u32
  r <= 255'u32 and r.uint8 in {0'u8, 1'u8, 2'u8, 3'u8, 4'u8, 5'u8, 8'u8, 9'u8, 10'u8, 12'u8, 28'u8, 29'u8, 30'u8}

type
  NifcTypeQualifier* = enum
    NoQualifier
    AtomicQ = (100, "atomic")  ## `atomic` type qualifier for NIFC
    RoQ = (101, "ro")  ## `readonly` (= `const`) type qualifier for NIFC
    RestrictQ = (102, "restrict")  ## type qualifier for NIFC
    CpprefQ = (103, "cppref")  ## type qualifier for NIFC that provides a C++ reference

proc rawTagIsNifcTypeQualifier*(raw: uint32): bool {.inline.} =
  raw >= 100'u32 and raw <= 103'u32

type
  NifcSym* = enum
    NoSym
    GvarY = (51, "gvar")  ## global variable declaration
    TvarY = (52, "tvar")  ## thread local variable declaration
    VarY = (53, "var")  ## variable declaration
    ParamY = (54, "param")  ## parameter declaration
    ConstY = (55, "const")  ## const variable declaration
    EfldY = (62, "efld")  ## enum field declaration
    FldY = (63, "fld")  ## field declaration
    ProcY = (64, "proc")  ## proc declaration
    LabY = (90, "lab")  ## label, target of a `jmp` instruction

proc rawTagIsNifcSym*(raw: uint32): bool {.inline.} =
  let r = raw - 51'u32
  r <= 255'u32 and r.uint8 in {0'u8, 1'u8, 2'u8, 3'u8, 4'u8, 11'u8, 12'u8, 13'u8, 39'u8}

