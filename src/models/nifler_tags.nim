# Generated by tools/gen_tags.nim from doc/tags.md. DO NOT EDIT!


type
  NiflerKind* = enum
    None
    ErrL = (1, "err")  ## indicates an error
    SufL = (2, "suf")  ## literal with suffix annotation
    AtL = (3, "at")  ## array indexing operation
    DerefL = (4, "deref")  ## pointer deref operation
    DotL = (5, "dot")  ## object field selection
    ParL = (7, "par")  ## syntactic parenthesis
    AddrL = (8, "addr")  ## address of operation
    NilL = (9, "nil")  ## nil pointer value
    ObjL = (23, "obj")  ## object constructor
    BracketL = (26, "bracket")  ## untyped array constructor
    CurlyL = (27, "curly")  ## untyped set constructor
    CurlyatL = (28, "curlyat")  ## curly expression `a{i}`
    KvL = (29, "kv")  ## key-value pair
    VvL = (30, "vv")  ## value-value pair (used for explicitly named arguments in function calls)
    CastL = (46, "cast")  ## `cast` operation
    CallL = (48, "call")  ## call operation
    CmdL = (49, "cmd")  ## command operation
    RangesL = (51, "ranges")
    VarL = (54, "var")  ## variable declaration
    ParamL = (55, "param")  ## parameter declaration
    ConstL = (56, "const")  ## const variable declaration
    LetL = (58, "let")  ## let variable declaration
    TypevarL = (60, "typevar")  ## type variable declaration
    EfldL = (61, "efld")  ## enum field declaration
    FldL = (62, "fld")  ## field declaration
    ProcL = (63, "proc")  ## proc declaration
    FuncL = (64, "func")  ## function declaration
    IteratorL = (65, "iterator")  ## iterator declaration
    ConverterL = (66, "converter")  ## converter declaration
    MethodL = (67, "method")  ## method declaration
    MacroL = (68, "macro")  ## macro declaration
    TemplateL = (69, "template")  ## template declaration
    TypeL = (70, "type")  ## type declaration
    BlockL = (71, "block")  ## block declaration
    AsgnL = (76, "asgn")  ## assignment statement
    IfL = (78, "if")  ## if statement header
    WhenL = (79, "when")  ## when statement header
    ElifL = (80, "elif")  ## pair of (condition, action)
    ElseL = (81, "else")  ## `else` action
    TypevarsL = (82, "typevars")  ## type variable/generic parameters
    BreakL = (83, "break")  ## `break` statement
    ContinueL = (84, "continue")  ## `continue` statement
    ForL = (85, "for")  ## for statement
    WhileL = (86, "while")  ## `while` statement
    CaseL = (87, "case")  ## `case` statement
    OfL = (88, "of")  ## `of` branch within a `case` statement
    RetL = (91, "ret")  ## `return` instruction
    YldL = (92, "yld")  ## yield statement
    StmtsL = (93, "stmts")  ## list of statements
    ParamsL = (94, "params")  ## list of proc parameters, also used as a "proc type"
    ObjectL = (96, "object")  ## object type declaration
    EnumL = (97, "enum")  ## enum type declaration
    ProctypeL = (98, "proctype")  ## proc type declaration (soon obsolete, use params instead)
    PtrL = (109, "ptr")  ## `ptr` type contructor
    PragmasL = (128, "pragmas")  ## begin of pragma section
    PragmaxL = (129, "pragmax")  ## pragma expressions
    IncludeL = (137, "include")  ## `include` statement
    ImportL = (138, "import")  ## `import` statement
    ImportasL = (139, "importas")  ## `import as` statement
    FromL = (140, "from")  ## `from` statement
    ImportexceptL = (141, "importexcept")  ## `importexcept` statement
    ExportL = (142, "export")  ## `export` statement
    ExportexceptL = (143, "exportexcept")  ## `exportexcept` statement
    CommentL = (144, "comment")  ## `comment` statement
    DiscardL = (145, "discard")  ## `discard` statement
    TryL = (146, "try")  ## `try` statement
    RaiseL = (147, "raise")  ## `raise` statement
    StaticL = (151, "static")  ## `static` type or annotation
    UnpackflatL = (156, "unpackflat")  ## unpack into flat variable list
    UnpacktupL = (157, "unpacktup")  ## unpack tuple
    UnpackdeclL = (158, "unpackdecl")  ## unpack var/let/const declaration
    ExceptL = (159, "except")  ## except subsection
    FinL = (160, "fin")  ## finally subsection
    RefobjL = (161, "refobj")  ## `ref object` type
    PtrobjL = (162, "ptrobj")  ## `ptr object` type
    TupleL = (163, "tuple")  ## `tuple` type
    RefL = (165, "ref")  ## `ref` type
    MutL = (166, "mut")  ## `mut` type
    OutL = (167, "out")  ## `out` type
    ConceptL = (171, "concept")  ## `concept` type
    DistinctL = (172, "distinct")  ## `distinct` type
    ItertypeL = (173, "itertype")  ## `itertype` type
    QuotedL = (207, "quoted")  ## name in backticks
    TupL = (212, "tup")  ## tuple constructor
    TabconstrL = (214, "tabconstr")  ## table constructor
    CallstrlitL = (219, "callstrlit")
    InfixL = (220, "infix")
    PrefixL = (221, "prefix")
    TypeofL = (228, "typeof")
    ExprL = (234, "expr")
    DoL = (235, "do")  ## `do` expression
    StaticstmtL = (255, "staticstmt")  ## `static` statement
    BindL = (256, "bind")  ## `bind` statement
    MixinL = (257, "mixin")  ## `mixin` statement
    UsingL = (258, "using")  ## `using` statement
    AsmL = (259, "asm")  ## `asm` statement
    DeferL = (260, "defer")  ## `defer` statement
