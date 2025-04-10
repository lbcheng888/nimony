# C++ 到 NIF 映射指南

本文档概述了将 C++ 源代码结构映射到 Nimony NIF（Nimony Intermediate Format）表示的关键概念和具体映射关系。目标是使用 NIF 来描述 C++ 代码的静态结构和元数据，以便进行进一步的分析或代码生成。

## 一般原则

1.  **NIF 节点与标签 (Tags):** 每个 C++ 语言结构（如类型、声明、语句、表达式）都将映射到一个带有特定标签的 NIF 节点。标签清晰地标识了 C++ 结构的类型。
2.  **属性 (Attributes):** NIF 节点的属性用于存储结构的详细信息，例如名称、修饰符、限定符、操作符、访问说明符等。
3.  **结构 (Structure):** NIF 节点的嵌套关系反映了 C++ 代码的语法结构。例如，一个函数定义的 NIF 节点将包含参数和函数体的子节点。
4.  **源位置 (Source Location):** 每个映射的 NIF 节点都尝试使用 `nifbuilder.addLineInfo` 添加 `line` 和 `col` 属性，指明其在原始 C++ 源代码中的位置。

## 具体映射

以下是根据 `src/nifcpp/transformer.nim` 当前实现的 NIF 映射：

### 1. 类型 (Types)

| C++ 结构                 | AST 节点                 | NIF 映射                     | 关键属性                                     | 备注 / 状态                                                                 |
| :----------------------- | :----------------------- | :--------------------------- | :------------------------------------------- | :-------------------------------------------------------------------------- |
| 基本类型 (`int`, `float`...) | `BaseTypeExpr`           | (标识符)                     | `name: string` (e.g., "int", "MyClass")      | ✅ (映射到 Nim 类型名, 已扩展)                                              |
| 指针 (`*`)               | `PointerTypeExpr`        | `PtrT`                       | `baseType: TypeExpr`                         | ✅                                                                          |
| 引用 (`&`)               | `ReferenceTypeExpr`      | `"var"`                      | `baseType: TypeExpr`                         | ✅ (映射到 `"var"`)                                                         |
| `const` 限定符           | `ConstQualifiedTypeExpr` | 基础类型 + `(pragma const)`  | `baseType: TypeExpr`                         | ✅                                                                          |
| `volatile` 限定符        | `VolatileQualifiedTypeExpr`| 基础类型 + `(pragma volatile)` | `baseType: TypeExpr`                         | ✅                                                                          |
| 数组类型               | `ArrayTypeExpr`          | `ArrayT`                     | `elementType: TypeExpr`, `size: Expression?` | ✅                                                                          |
| 函数类型               | `FunctionTypeExpr`       | `ParamsT`                    | `returnType: TypeExpr`, `parameters: seq[Parameter]` | ✅ (限定符通过 `PragmasU` 处理)                                             |

### 2. 声明 (Declarations)

| C++ 结构                 | AST 节点                 | NIF 映射                     | 关键属性                                                                                                | 备注 / 状态                                                                                             |
| :----------------------- | :----------------------- | :--------------------------- | :------------------------------------------------------------------------------------------------------ | :------------------------------------------------------------------------------------------------------ |
| 变量声明               | `VariableDeclaration`    | `VarS`                       | `name: Identifier`, `varType: TypeExpr`, `initializer: Expression?`, `access: AccessSpecifier?`         | ✅ (访问说明符通过 pragma 处理)                                                                         |
| 函数参数               | `Parameter`              | `ParamU`                     | `name: Identifier`, `paramType: TypeExpr`, `defaultValue: Expression?`                                  | ✅ (在 `FunctionDefinition` 和 `FunctionTypeExpr` 中处理, `defaultValue` 已处理)                      |
| 函数定义               | `FunctionDefinition`     | `ProcS`                      | `name: Identifier`, `returnType: TypeExpr`, `parameters: seq[Parameter]`, `body: BlockStatement`, `access: AccessSpecifier?` | ✅ (访问说明符通过 pragma 处理, 添加了空 `PragmasU`)                                                  |
| 类定义 (`class`)         | `ClassDefinition`        | `TypeS` + `ObjectT`          | `name: Identifier`, `kind: TokenType`, `bases: seq[InheritanceInfo]`, `members: seq[Statement]`, `isFinal: bool` | ✅ (kind/final 通过 kv/pragma 处理, 访问说明符通过 pragma 处理)                                       |
| 结构体定义 (`struct`)    | `ClassDefinition`        | `TypeS` + `ObjectT`          | `name: Identifier`, `kind: TokenType`, `bases: seq[InheritanceInfo]`, `members: seq[Statement]`, `isFinal: bool` | ✅ (同上, 使用 `ClassDefinition` 并检查 `kind`)                                                       |
| 继承信息             | `InheritanceInfo`        | `(kv access base)` in `bases`| `baseName: Identifier`, `access: AccessSpecifier`                                                       | ✅ (结构待最终确认)                                                                                     |
| 命名空间               | `NamespaceDef`           | `ModuleY`                    | `name: Identifier?`, `declarations: seq[Statement]`                                                     | ✅ (匿名命名空间 `name` 为空)                                                                           |
| 枚举                   | `EnumDefinition`         | `TypeS` + `EnumT`            | `name: Identifier?`, `isClass: bool`, `baseType: TypeExpr?`, `enumerators: seq[Enumerator]`            | ✅ (映射到 `TypeS` + `EnumT`, `isClass` 和 `baseType` 通过 pragma 处理)                                |
| Typedef/Using          | `TypeAliasDeclaration`   | `TypeS`                      | `name: Identifier`, `aliasedType: TypeExpr`                                                             | ✅ (映射到 `TypeS`)                                                                                     |

### 3. 语句 (Statements)

| C++ 结构           | AST 节点                 | NIF 映射             | 关键属性                                                                 | 备注 / 状态                                                              |
| :----------------- | :----------------------- | :------------------- | :----------------------------------------------------------------------- | :----------------------------------------------------------------------- |
| 表达式语句       | `ExpressionStatement`    | (表达式) / `AsgnS`   | `expression: Expression`                                                 | ✅ (映射到 NIF 表达式或 `AsgnS` for assignment)                          |
| 块语句 (`{}`)     | `BlockStatement`         | `StmtsS`             | `statements: seq[Statement]`                                             | ✅                                                                         |
| 返回语句 (`return`) | `ReturnStatement`        | `RetS`               | `returnValue: Expression?`                                               | ✅                                                                         |
| If 语句          | `IfStatement`            | `IfS` + `branch`     | `condition: Expression`, `thenBranch: Statement`, `elseBranch: Statement?` | ✅ (使用 `branch` 结构)                                                  |
| While 语句       | `WhileStatement`         | `WhileS`             | `condition: Expression`, `body: Statement`                               | ✅                                                                         |
| For 语句         | `ForStatement`           | `BlockS` + `WhileS`  | `initializer: Statement?`, `condition: Expression?`, `update: Expression?`, `body: Statement` | ✅ (转换为 `BlockS` + `WhileS` 结构)                                     |
| Range-based For  | `RangeForStatement`      | `ForS`               | `declaration: VariableDeclaration`, `rangeExpr: Expression`, `body: Statement` | ✅ (映射到 `ForS`)                                                       |
| Break            | `BreakStatement`         | `BreakS`             |                                                                          | ✅                                                                         |
| Continue         | `ContinueStatement`      | `ContinueS`          |                                                                          | ✅                                                                         |
| Switch           | `SwitchStatement`        | `CaseS`              | `controlExpr: Expression`, `body: BlockStatement`                        | ✅ (映射到 `CaseS`, `body` 中的 `CaseStatement` 映射到 `OfU`)              |
| Case             | `CaseStatement`          | (在 `Switch` 中处理) | `value: Expression?` (nil for default)                                   | ✅ (作为 `Switch` 映射的一部分，映射到 `OfU`)                            |
| Try/Catch        | `TryStatement`           | `TryS`               | `tryBlock: BlockStmt`, `catchClauses: seq[CatchClause]`                  | ✅                                                                         |
| Catch            | `CatchClause`            | `ExceptU`            | `exceptionDecl: VariableDeclaration?`, `isCatchAll: bool`, `body: BlockStatement` | ✅ (映射到 `ExceptU`, 结构为 `(ExceptU type name body)`)                 |

### 4. 表达式 (Expressions)

| C++ 结构                 | AST 节点                 | NIF 映射                     | 关键属性                                                              | 备注 / 状态                                                              |
| :----------------------- | :----------------------- | :--------------------------- | :-------------------------------------------------------------------- | :----------------------------------------------------------------------- |
| 标识符                   | `Identifier`             | (标识符)                     | `name: string`                                                        | ✅                                                                       |
| 整型字面量               | `IntegerLiteral`         | (整型字面量)                 | `value: int64`                                                        | ✅                                                                       |
| 浮点字面量               | `FloatLiteral`           | (浮点字面量)                 | `value: float64`                                                      | ✅                                                                       |
| 字符串字面量             | `StringLiteral`          | (字符串字面量)               | `value: string`                                                       | ✅                                                                       |
| 字符字面量               | `CharLiteral`            | (字符字面量)                 | `value: char`                                                         | ✅                                                                       |
| 初始化列表 (`{}`)       | `InitializerListExpr`    | `BracketX`                   | `values: seq[Expression]`                                             | ✅                                                                       |
| 二元表达式 (`+`, `==`...) | `BinaryExpression`       | (对应 NIF 表达式标签)         | `left: Expression`, `op: TokenType`, `right: Expression`              | ✅ (包括复合赋值 `op=`)                                                  |
| 一元表达式 (`-`, `!`, `++`) | `UnaryExpression`        | (对应 NIF 表达式标签)         | `op: TokenType`, `operand: Expression`, `isPrefix: bool`              | ✅ (包括 `addr`, `deref`, `bitnot`)                                      |
| 函数调用                 | `CallExpression`         | `CallX`                      | `function: Expression`, `arguments: seq[Expression]`                  | ✅                                                                       |
| 成员访问 (`.`, `->`)     | `MemberAccessExpression` | `DotX` / `(DotX (DerefX ..))`| `targetObject: Expression`, `member: Identifier`, `isArrow: bool`     | ✅ (`->` 映射为 `DotX` + `DerefX`)                                       |
| 数组下标 (`[]`)          | `ArraySubscriptExpression`| `BracketX`                   | `array: Expression`, `index: Expression`                              | ✅                                                                       |
| Throw 表达式           | `ThrowExpression`        | `RaiseS`                     | `expression: Expression?`                                             | ✅ (`expression` 为空表示 `throw;`)                                      |
| Lambda 表达式          | `LambdaExpression`       | `ProcS`                      | `captures: seq[Identifier]`, `captureDefault: Option[TokenType]`, `parameters: seq[Parameter]`, `returnType: TypeExpr?`, `body: BlockStatement` | ✅ (映射到 `ProcS`, captures 通过 pragma 处理)                           |

### 5. 模板 (Templates)

| C++ 结构                 | AST 节点                 | NIF 映射                     | 关键属性                                                                                                | 备注 / 状态                                                              |
| :----------------------- | :----------------------- | :--------------------------- | :------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------------- |
| 模板声明               | `TemplateDecl`           | `TemplateS`                  | `parameters: seq[TemplateParameter]`, `declaration: Statement`                                          | ✅                                                                       |
| 模板参数               | `TemplateParameter`      | `TypevarU`                   | `kind: TemplateParameterKind`, `name: Identifier?`, `paramType: TypeExpr?`, `defaultValue: Expression?`, `templateParams: seq[TemplateParameter]?` | ✅ (基本映射, `tpTemplate` kind 和 `templateParams` 待完善)              |

## 未实现/待办事项

*   **模板细节:**
    *   完善 `TemplateParameter` 对 `tpTemplate` 类型（模板模板参数）和 `templateParams` 字段的处理。
    *   处理模板特化/实例化等更复杂的模板功能（可能需要更详细的 AST/Parser 支持）。
*   **错误处理:** 进一步完善错误报告机制。
*   **符号表:** 集成符号表以处理名称解析和类型推断（长期目标）。
*   **NIF 规范确认:** 某些 NIF 结构（如类继承 `bases` 的具体形式）可能需要根据最终的 NIF 规范进行微调。
*   **未来可能的 AST/Parser 扩展:** 考虑是否需要支持更多 C++ 特性（如 C++20 concepts, modules 等）。
