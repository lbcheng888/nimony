# Generated by tools/gen_tags.nim from doc/tags.md. DO NOT EDIT!


type
  NifIndexKind* = enum
    NoIndexTag
    KvIdx = (28, "kv")  ## key-value pair
    VvIdx = (29, "vv")  ## value-value pair (used for explicitly named arguments in function calls)
    GvarIdx = (51, "gvar")  ## global variable declaration
    TvarIdx = (52, "tvar")  ## thread local variable declaration
    VarIdx = (53, "var")  ## variable declaration
    ConstIdx = (55, "const")  ## const variable declaration
    GletIdx = (57, "glet")  ## global let variable declaration
    TletIdx = (58, "tlet")  ## thread local let variable declaration
    LetIdx = (59, "let")  ## let variable declaration
    CursorIdx = (60, "cursor")  ## cursor variable declaration
    ProcIdx = (64, "proc")  ## proc declaration
    FuncIdx = (65, "func")  ## function declaration
    IteratorIdx = (66, "iterator")  ## iterator declaration
    ConverterIdx = (67, "converter")  ## converter declaration
    MethodIdx = (68, "method")  ## method declaration
    MacroIdx = (69, "macro")  ## macro declaration
    TemplateIdx = (70, "template")  ## template declaration
    TypeIdx = (71, "type")  ## type declaration
    InlineIdx = (123, "inline")  ## `inline` proc annotation
    ExportIdx = (143, "export")  ## `export` statement
    FromexportIdx = (144, "fromexport")  ## specific exported symbols from a module
    ExportexceptIdx = (145, "exportexcept")  ## `exportexcept` statement
    BuildIdx = (204, "build")  ## `build` pragma
    DestroyIdx = (250, "destroy")
    DupIdx = (251, "dup")
    CopyIdx = (252, "copy")
    WasmovedIdx = (253, "wasmoved")
    SinkhIdx = (254, "sinkh")
    TraceIdx = (255, "trace")
    IndexIdx = (263, "index")  ## index section
    PublicIdx = (264, "public")  ## public section
    PrivateIdx = (265, "private")  ## private section

proc rawTagIsNifIndexKind*(raw: uint32): bool {.inline.} =
  let r = raw - 28'u32
  r <= 255'u32 and r.uint8 in {0'u8, 1'u8, 23'u8, 24'u8, 25'u8, 27'u8, 29'u8, 30'u8, 31'u8, 32'u8, 36'u8, 37'u8, 38'u8, 39'u8, 40'u8, 41'u8, 42'u8, 43'u8, 95'u8, 115'u8, 116'u8, 117'u8, 176'u8, 222'u8, 223'u8, 224'u8, 225'u8, 226'u8, 227'u8, 235'u8, 236'u8, 237'u8}

