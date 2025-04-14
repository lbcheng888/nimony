NIFC 方言
=========

NIFC 是 NIF 的一种方言，设计上非常接近 C 语言。其优点包括：

- 比直接生成 C/C++ 代码更容易。
- 拥有所有 NIF 相关的工具支持。
- NIFC 通过清晰区分以下类型，改进了 C 语言中令人困惑的数组和指针类型：
  `array` 始终是值类型，`ptr` 始终指向单个元素，而 `aptr` 指向元素数组。
- 继承直接在类型系统中建模，而不是像 C 语言那样使用涉及结构体与其第一个元素之间别名的古怪类型别名规则。

名称修饰（Name Mangling）
-----------------------

名称修饰由 NIFC 执行。基于以下假设：

- 一个 NIF 符号的形式为 `identifier.<number>.modulesuffix`（如果是顶层条目）
  或 `identifier.<number>`（对于局部符号）。例如，`replace.2.strutils` 将是
  `strutils` 中的第二个 `replace`。泛型实例会获得 `.g` 后缀。
- 从 C 或 C++ 导入的符号使用 `.c` 作为 `modulesuffix`。注意，通过 `\xx`
  名称可以包含 `::`，这对于 C++ 支持是必需的。这些符号在 Nim 中可以有不同的
  名称。原始名称可以通过 `was` 注解提供。更多细节请参见语法部分。

以 `.c` 结尾的名称通过移除 `.c` 后缀进行修饰。对于其他名称，`.` 被替换为 `_`，
并且 `_` 被编码为 `Q_`。

根据设计，未从 C 导入的名称中包含数字，因此不会与 C 或 C++ 的关键字冲突。

其他字符或字符组合（无论它们在 C/C++ 标识符中是否有效！）通过下表进行编码：

| 字符组合 | 编码为 |
| -------- | -------- |
| `Q`      | `QQ` (由于此规则，`Q` 现在可用作转义字符) |
| `_`      | `Q_` |
| `[]=`    | `putQ` |
| `[]`     | `getQ` |
| `$`      | `dollarQ` |
| `%`      | `percentQ` |
| `&`      | `ampQ` |
| `^`      | `roofQ` |
| `!`      | `emarkQ` |
| `?`      | `qmarkQ` |
| `*`      | `starQ` |
| `+`      | `plusQ` |
| `-`      | `minusQ` |
| `/`      | `slashQ` |
| `\`     | `bslashQ` |
| `==`     | `eqQ` |
| `=`      | `eQ` |
| `<=`     | `leQ` |
| `>=`     | `geQ` |
| `<`      | `ltQ` |
| `>`      | `gtQ` |
| `~`      | `tildeQ` |
| `:`      | `colonQ` |
| `@`      | `atQ` |
| `\|`    | `barQ` |
| 其他     | `XxxQ` 其中 `xx` 是十六进制值 |

语法
----

生成的 NIFC 代码必须遵守此语法。为了提高可读性，`'('` 和 `')'` 不带引号书写，
`[]` 用于分组。

```
Empty ::= <根据 NIF 规范>
Identifier ::= <根据 NIF 规范>
Symbol ::= <根据 NIF 规范>
SymbolDef ::= <根据 NIF 规范>
Number ::= <根据 NIF 规范>
CharLiteral ::= <根据 NIF 规范>
StringLiteral ::= <根据 NIF 规范>
IntBits ::= ('+' | '-') [0-9]+

Lvalue ::= Symbol | (deref Expr (cppref)?) |
             (at Expr Expr) | # 数组索引
             (dot Expr Symbol Number) | # 字段访问
             (pat Expr Expr) | # 指针索引
             (errv) | (ovf)

Call ::= (call Expr+)
CallCanRaise ::= (onerr Stmt Expr+)

ArithExpr ::= (add Type Expr Expr) |
         (sub Type Expr Expr) |
         (mul Type Expr Expr) |
         (div Type Expr Expr) |
         (mod Type Expr Expr)

Expr ::= Number | CharLiteral | StringLiteral |
         Lvalue |
         (par Expr) | # 将表达式包裹在括号中
         (addr Lvalue (cppref)?) | # "取地址" 操作
         (nil) | (false) | (true) |
         (inf) | (neginf) | (nan) |
         (and Expr Expr) | # "&&"
         (or Expr Expr) | # "||"
         (not Expr) | # "!"
         (neg Type Expr) |
         (sizeof Type) |
         (alignof Type) |
         (offsetof Type SYMBOL) |
         (oconstr Type (kv Symbol Expr)*) |  # (对象构造器){...}
         (aconstr Type Expr*) |              # 数组构造器
         ArithExpr |
         (shr Type Expr Expr) |
         (shl Type Expr Expr) |
         (bitand Type Expr Expr) |
         (bitor Type Expr Expr) |
         (bitnot Type Expr) |
         (bitxor Type Expr Expr) |
         (eq Expr Expr) |
         (neq Expr Expr) |
         (le Expr Expr) |
         (lt Expr Expr) |
         (cast Type Expr) |
         (conv Type Expr) |
         Call |
         CallCanRaise

BranchValue ::= Number | CharLiteral | Symbol | (true) | (false)
BranchRange ::= BranchValue | (range BranchValue BranchValue)
BranchRanges ::= (ranges BranchRange+)

VarDeclCommon ::= SymbolDef VarPragmas Type [Empty | Expr]
VarDecl ::= (var VarDeclCommon) | # 局部变量
            (gvar VarDeclCommon) | # 全局变量
            (tvar VarDeclCommon) # 线程局部变量

ConstDecl ::= (const SymbolDef VarPragmas Type Expr)
EmitStmt ::= (emit Expr+)
TryStmt ::= (try StmtList StmtList StmtList)
RaiseStmt ::= (raise [Empty | Expr])
AsgnStmt ::= (asgn Lvalue Expr)
KeepOverflowStmt ::= (keepovf ArithExpr Lvalue)
IfStmt ::= (if (elif Expr StmtList)+ (else StmtList)? )
WhileStmt ::= (while Expr StmtList)
CaseStmt ::= (case Expr (of BranchRanges StmtList)* (else StmtList)?)
LabelStmt ::= (lab SymbolDef)
JumpStmt ::= (jmp Symbol)
ScopeStmt ::= (scope StmtList)
DiscardStmt ::= (discard Expr)

Stmt ::= Call |
         CallCanRaise |
         VarDecl |
         ConstDecl |
         EmitStmt |
         TryStmt |
         RaiseStmt |
         AsgnStmt |
         KeepOverflowStmt |
         IfStmt |
         WhileStmt |
         (break) |
         CaseStmt |
         LabelStmt |
         JumpStmt |
         ScopeStmt |
         (ret [Empty | Expr]) | # return 语句
         DiscardStmt


StmtList ::= (stmts SCOPE Stmt*)

Param ::= (param SymbolDef ParamPragmas Type)
Params ::= Empty | (params Param*)

ProcDecl ::= (proc SymbolDef Params Type ProcPragmas [Empty | StmtList])

FieldDecl ::= (fld SymbolDef FieldPragmas Type)

UnionDecl ::= (union Empty FieldDecl*)
ObjDecl ::= (object [Empty | Type] FieldDecl*)
EnumFieldDecl ::= (efld SymbolDef Number)
EnumDecl ::= (enum Type EnumFieldDecl+)

ProcType ::= (proctype Empty Params Type ProcTypePragmas)

IntQualifier ::= (atomic) | (ro)
PtrQualifier ::= (atomic) | (ro) | (restrict)

Type ::= Symbol |
         (i IntBits IntQualifier*) |
         (u IntBits IntQualifier*) |
         (f IntBits IntQualifier*) |
         (c IntBits IntQualifier*) | # 字符类型
         (bool IntQualifier*) |
         (void) |
         (ptr Type PtrQualifier* (cppref)?) | # 指向单个对象的指针
         (flexarray Type) |
         (aptr Type PtrQualifier*) | # 指向对象数组的指针
         ProcType
ArrayDecl ::= (array Type Expr)
TypeDecl ::= (type SymbolDef TypePragmas [ProcType | ObjDecl | UnionDecl | EnumDecl | ArrayDecl])

CallingConvention ::= (cdecl) | (stdcall) | (safecall) | (syscall)  |
                      (fastcall) | (thiscall) | (noconv) | (member)

Attribute ::= (attr StringLiteral)
ProcPragma ::= (inline) | (noinline) | CallingConvention | (varargs) | (was Identifier) | (selectany) | Attribute |
            | (raises) | (errs)

ProcTypePragma ::= CallingConvention | (varargs) | Attribute

ProcTypePragmas ::= Empty | (pragmas ProcTypePragma+)
ProcPragmas ::= Empty | (pragmas ProcPragma+)

CommonPragma ::= (align Number) | (was Identifier) | Attribute
VarPragma ::= CommonPragma | (static)
VarPragmas ::= Empty | (pragmas VarPragma+)

ParamPragma ::= (was Identifier) | Attribute
ParamPragmas ::= Empty | (pragmas ParamPragma+)

FieldPragma ::= CommonPragma | (bits Number)
FieldPragmas ::= (pragmas FieldPragma+)

TypePragma ::= CommonPragma | (vector Number)
TypePragmas ::= Empty | (pragmas TypePragma+)


ExternDecl ::= (imp ProcDecl | VarDecl | ConstDecl)
IgnoreDecl ::= (nodecl ProcDecl | VarDecl | ConstDecl)
Include ::= (incl StringLiteral)

TopLevelConstruct ::= ExternDecl | IgnoreDecl | ProcDecl | VarDecl | ConstDecl |
                      TypeDecl | Include | EmitStmt | Call | CallCanRaise |
                      TryStmt | RaiseStmt | AsgnStmt | KeepOverflowStmt |
                      IfStmt | WhileStmt | CaseStmt | LabelStmt | JumpStmt |
                      ScopeStmt | DiscardStmt

Module ::= (stmts TopLevelConstruct*)

```

注意:

- `IntBits` 是 8, 16, 32, 64 等，或 `-1` 代表机器字长。
- 调用约定可以不止 `cdecl` 和 `stdcall`。
- `case` 映射到 `switch`，但 `break` 的生成是自动处理的。
- `ro` 代表 `readonly`，是 C 语言中 `const` 类型限定符的概念。
  不要与 NIFC 的 `const` 混淆，后者用于引入命名常量。
- C 允许在过程体内部使用 `typedef`。NIFC 不允许，类型声明必须
  始终在顶层进行。
- `emit` 中的字符串字面量产生 C 代码本身，而不是 C 字符串字面量。
- 对于 `array`，元素类型在元素数量之前。
  原因：与指针类型保持一致性。
- `proctype` 有一个 Empty 节点，而 `proc` 有一个名称，因此参数
  总是第二个子节点，然后是返回类型和调用约定。这使得
  节点结构更规整，可以简化类型检查器。
- `varargs` 被建模为一个 pragma，而不是参数声明的花哨特殊语法。
- 类型 `flexarray` 只能用于对象声明中的最后一个字段。
- pragma `selectany` 可用于合并具有相同名称的过程体。
  它用于泛型过程，以便最终可执行文件中只保留一个泛型实例。
- `attr "abc"` 用 `__attribute__(abc)` 注解符号。
- `cast` 可能通过 `union` 映射到类型裁剪操作，因为 C 的别名规则是有问题的。
- `conv` 是数值类型之间的值保持类型转换，`cast` 是位保持类型转换。
- `array` 映射到内部包含数组的结构体，使数组获得值语义。
  因此数组只能在 `type` 环境中使用，并且是名义类型。
  NIFC 代码生成器必须确保例如 `(type :MyArray.T . (array T 4))` 只被
  发射一次。
- `type` 只能用于为名义类型（即仅与其自身兼容的类型）引入名称，
  或出于代码压缩目的为过程类型引入名称。任意的类型别名 **不能** 使用！
  理由：实现简单性。
- `nodecl` 是一种像 `imp` 一样的导入机制，但声明来自头文件，
  并且不应在生成的 C/C++ 代码中声明。
- `var` 始终是局部变量，`gvar` 是全局变量，`tvar` 是线程局部变量。
- `SCOPE` 指示该构造引入了一个新的局部变量作用域。
- `cppref` 是一个 pragma，指示指针应转换为 C++ 引用 (`T&`)。
  `(deref)` 和 `(addr)` 操作此时仍然是强制性的，并且也必须用 `(cppref)` 注解。
  `(cppref)` 可以与 `(ro)` 结合使用以产生 `const T&` 引用。

作用域
------

NIFC 的作用域规则实际上与 C 相同：C 的 `{ ... }` 花括号引入一个新的局部作用域，
NIFC 的 `StmtList` 非终结符也是如此。语法中用 `SCOPE` 关键字标示，
该关键字本身没有意义，解析器应忽略它。

可以使用 `(scope ...)` 构造引入额外的作用域，它会产生 `{}`。注意，
`scope` 不能通过 `break` 退出，`scope` 在这方面不像 Nim 的 `block` 语句！

基础代码生成器可以使用作用域来生成显著更好的代码。

继承
-----

NIFC 在其对象系统中直接支持继承。但是，不隐含运行时检查，
如果需要，必须手动实现 RTTI。

- `object` 声明允许继承。其第一个子节点是父类（或为空）。
- `dot` 操作接受第三个参数，即"继承"编号。通常是 `0`，但如果字段在父对象中，则为 `1`，
  在父对象的父对象中则为 `2`，依此类推。

声明顺序
---------

NIFC 允许任意顺序的声明，无需前向声明。

异常
----

NIFC 支持两种异常处理原语。

- `try` 和 `raise` 构造：这些必须在 C++ 模式下使用，并转换为其 C++ 等价物。NIFC 调用者有责任确保在禁用 C++ 支持时不发射它们。`try` 构造遵循格式 `(try <actions> <onerr> <finally>)`。`raise` 构造可以生成 C++ `throw` 语句。

- `errv` 和 `onerr` 构造：这些必须在不生成 C++ 代码时使用。可能引发异常的调用必须包装在 `(onerr)` 中。格式为 `(onerr <action> <f> <args>)`，其中 action 通常是 `jmp` 指令。在 C++ 异常处理模式下，action 应始终为点 `.`。可以使用 `(asgn)` 设置类型为 `bool` 的特殊变量 `(errv)`，并像其他位置一样查询，例如 `(asgn (errv) (true)) # 设置错误位`。

函数可以并且必须使用 `(raises)` pragma 进行注解，以指示它们可以引发 C++ 异常。同样，如果它们使用 `(errv)` 机制，则需要使用 `errs` pragma。一个函数可以同时使用这两个注解。那将是一个同时使用 `(raise)` 和 `(errv)` 的 C++ 函数。

溢出检查
---------

NIFC 支持算术运算的溢出检查。`(ovf)` 标签用于访问溢出标志。
需要进行溢出检查的算术运算必须使用 `(keepovf)` 构造完成：

```
(var :x.0 . (i +32) .)
(var :a.0 . (i +32) +90)
(var :b.0 . (i +32) +223231343)
(keepovf (add (i +32) a.0 b.0) x.0)
(if (elif (ovf) (stmts (asgn (ovf) (false)) (call printf.c "overflow")))
    (else (call printf.c "no overflow")))
```

`keepovf` 可以理解为一种元组赋值形式：`(overflowFlag, x.0) = a.0 + b.0`。
由于 `keepovf` 是一个语句而不是表达式，代码生成器通常需要为嵌套表达式引入临时变量。
这对于 GCC 的 `__builtin_saddll_overflow` 构造也是必需的，并且是这种奇怪设计的真正原因。

`(ovf)` 标志是一个左值，可以设置为 `(false)` 以重置该标志：

```
(asgn (ovf) (false))
```

这不是可选的！这对于可靠的本地代码生成是必需的。 