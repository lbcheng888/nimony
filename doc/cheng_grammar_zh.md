# Cheng 语言语法规范 (v0.4 - 基于 Pratt 解析器实现)

本文档描述了 Cheng 语言当前的语法，该语法由 `src/cheng_py/parser/parser.py` 中的 Pratt 解析器实现，并参考了 `tests/cheng_py/all_test_examples.py` 中的示例。

## 1. 词法结构

### 1.1 空白符

空白字符（空格、制表符、换行符）用于分隔标记 (token)，但在其他情况下会被忽略（字符串字面量内部除外）。

### 1.2 标记 (Tokens)

语言识别以下标记（基于 `token_specification`）：

*   **括号:** `(`, `)`
*   **方括号:** `[`, `]`
*   **准引用/解引用:** `` ` `` (BACKTICK), `,@` (COMMA_AT), `,` (COMMA)
*   **字面量:**
    *   **布尔值:** `true`, `false` (BOOLEAN)
    *   **浮点数:** `[+-]?(\d+\.\d*|\.\d+)([eE][+-]?\d+)?` (FLOAT)
    *   **整数:** `[+-]?\d+` (INTEGER)
    *   **字符串:** `"(?:\\.|[^"\\])*"` (STRING)
*   **运算符:**
    *   **比较:** `==` (EQEQ), `<=` (LTEQ), `>=` (GTEQ), `<` (LT), `>` (GT)
    *   **赋值:** `=` (EQUALS)
    *   **算术:** `+` (PLUS), `-` (MINUS), `*` (STAR), `/` (SLASH)
    *   **引用/解引用:** `&` (AMPERSAND), `*` (STAR - 与乘法共享，由上下文区分)
    *   **三目/切片:** `?` (QUESTION), `:` (COLON)
    *   **引用语法糖:** `'` (QUOTE_TICK)
*   **符号 (Symbol):** `[a-zA-Z_?!+\-\*\/<>=&|][a-zA-Z0-9_?!+\-\*\/<>=&|]*` (SYMBOL - 匹配关键字和标识符，解析器区分)
    *   **Nil:** `()` 在解析阶段直接识别为空列表，对应 `Nil` 单例对象。
    *   **空白:** `\s+` (词法分析阶段忽略)
    *   **不匹配:** `.` (MISMATCH - 引发词法错误)

## 2. 句法结构 (基于 Pratt 解析器)

Cheng 语言采用 Pratt 解析器，结合了对 S-表达式风格括号结构的处理。核心是 `parse_expression` 函数，它根据运算符的**绑定力 (Binding Power, BP)** 和**NUD (Null Denotation)** / **LED (Left Denotation)** 函数来解析表达式。

*   **NUD:** 处理前缀表达式（字面量、符号、前缀运算符 `&`, `*`, `` ` ``, `,`, `,@`, `'`）和 S-表达式结构 `(...)`。
*   **LED:** 处理中缀运算符 (`+`, `-`, `*`, `/`, `==`, `<`, `>`, `<=`, `>=`, `=`) 和后缀运算符 (`[...]`)。

### 2.1 顶层程序结构

一个 Cheng 程序由一个或多个顶层表达式或宏定义组成。这些表达式按顺序求值。如果存在多个顶层表达式，它们会被解析器隐式地包装在一个序列中。

```cheng
表达式1
表达式2
宏定义1
表达式3
```

### 2.2 表达式 (Expression)

解析器通过 `parse_expression(rbp)` 函数递归地解析表达式，其中 `rbp` 是右绑定力。

1.  **NUD 处理:** 首先查找当前标记的 NUD 函数并执行，解析出表达式的“左侧”部分 (`left`)。
    *   **字面量/符号:** 直接返回对应的 AST 节点 (Integer, Float, Boolean, String, Symbol)。
    *   **前缀运算符 (`&`, `*`, `` ` ``, `,`, `,@`, `'`):** 解析其后的操作数（使用该前缀运算符的绑定力作为 `rbp`），并构建相应的 `PrefixExpr`, `QuasiquoteExpr`, `UnquoteExpr`, `UnquoteSplicingExpr`, `QuoteExpr` 节点。
    *   **左括号 `(`:** 调用 `parse_paren_constructs` 处理括号内的结构。
2.  **LED 处理:** 循环检查后续标记，只要其绑定力大于当前的 `rbp`，就查找并执行其 LED 函数，将 `left` 作为其左操作数，更新 `left` 为 LED 函数返回的结果。
    *   **中缀运算符 (`+`, `-`, `*`, `/`, `==`, `<`, `>`, `<=`, `>=`, `=`):** 解析右操作数（使用该中缀运算符的绑定力作为 `rbp`），并构建 `BinaryExpr` 或 `AssignmentExpr` 节点。
    *   **左方括号 `[`:** 调用 `parse_index_or_slice` 处理索引或切片，构建 `IndexExpr` 或 `SliceExpr`。
    *   **左括号 `(` (作为 LED):** 不再使用。函数应用通过 S-表达式结构处理。

### 2.3 括号结构 `(...)` (`parse_paren_constructs`)

当解析器遇到左括号 `(` 时，`parse_paren_constructs` 被调用，根据括号内的内容解析为不同的 AST 节点：

1.  **空列表 `()`:** 如果紧跟右括号 `)`，则解析为 `Nil` 单例对象。求值结果也是 `Nil`。在条件表达式中，`Nil` 被视为 `false`。
2.  **解析第一个元素:** 使用默认优先级调用 `parse_expression` 解析括号内的第一个元素。
3.  **特殊形式检查 (基于第一个元素):**
    *   如果第一个元素是符号 `if`：按顺序解析条件、then 分支、可选的 else 分支（都使用默认优先级），最后消耗 `)`，返回 `IfExpr`。**注意:** 如果 `else` 分支省略且条件为假，表达式求值结果为 `Nil`。
    *   如果第一个元素是符号 `lambda`：解析参数列表 `(p1 p2 ...)` 和函数体表达式序列。函数体表达式被收集并包装在一个 `SequenceExpr`（如果多于一个）或作为单个表达式（如果只有一个）或 `Nil`（如果没有）中。返回 `LambdaExpr`。
4.  **函数/宏调用形式:** 如果第一个元素不是已知的特殊形式符号：
    *   将第一个元素视为**操作符 (operator)**。
    *   循环解析后续的表达式（都使用默认优先级），直到遇到右括号 `)`，将它们收集为**操作数 (operands)** 列表。
    *   消耗右括号 `)`。
    *   返回 `ApplyExpr(operator, operands)` 节点。例如 `(f x y)` 解析为 `ApplyExpr(Symbol('f'), [Symbol('x'), Symbol('y')])`，`(1 + 1)` 解析为 `ApplyExpr(Integer(1), [BinaryExpr(...)])` (这在求值时会失败，因为 1 不是函数)。
    *   **求值语义:** 求值器在遇到 `ApplyExpr` 时，会先求值操作符，然后求值操作数，最后将操作数应用于求值后的操作符（函数或宏）。

**注意:** 解析器本身不再直接生成 `ListLiteralExpr`。列表字面量需要通过 `quote` 或 `quasiquote` 来创建，例如 `' (1 2)` 或 `` `(1 2) ``。

### 2.4 宏定义

顶层解析器会特别识别 `符号 = \` (` 模式：

## 2. 句法结构 (基于 Pratt 解析器)

Cheng 语言采用 Pratt 解析器，结合了对 S-表达式风格括号结构的处理。核心是 `parse_expression` 函数，它根据运算符的**绑定力 (Binding Power, BP)** 和**NUD (Null Denotation)** / **LED (Left Denotation)** 函数来解析表达式。

*   **NUD:** 处理前缀表达式（字面量、符号、前缀运算符 `&`, `*`, `` ` ``, `,`, `,@`, `'`）和 S-表达式结构 `(...)`。
*   **LED:** 处理中缀运算符 (`+`, `-`, `*`, `/`, `==`, `<`, `>`, `<=`, `>=`, `=`) 和后缀运算符 (`[...]`)。

### 2.1 顶层程序结构

一个 Cheng 程序由一个或多个顶层表达式或宏定义组成。这些表达式按顺序求值。如果存在多个顶层表达式，它们会被解析器隐式地包装在一个序列中。

```cheng
表达式1
表达式2
宏定义1
表达式3
```

### 2.2 表达式 (Expression)

解析器通过 `parse_expression(rbp)` 函数递归地解析表达式，其中 `rbp` 是右绑定力。

1.  **NUD 处理:** 首先查找当前标记的 NUD 函数并执行，解析出表达式的“左侧”部分 (`left`)。
    *   **字面量/符号:** 直接返回对应的 AST 节点 (Integer, Float, Boolean, String, Symbol)。
    *   **前缀运算符 (`&`, `*`, `` ` ``, `,`, `,@`, `'`):** 解析其后的操作数（使用该前缀运算符的绑定力作为 `rbp`），并构建相应的 `PrefixExpr`, `QuasiquoteExpr`, `UnquoteExpr`, `UnquoteSplicingExpr`, `QuoteExpr` 节点。
    *   **左括号 `(`:** 调用 `parse_paren_constructs` 处理括号内的结构。
2.  **LED 处理:** 循环检查后续标记，只要其绑定力大于当前的 `rbp`，就查找并执行其 LED 函数，将 `left` 作为其左操作数，更新 `left` 为 LED 函数返回的结果。
    *   **中缀运算符 (`+`, `-`, `*`, `/`, `==`, `<`, `>`, `<=`, `>=`, `=`):** 解析右操作数（使用该中缀运算符的绑定力作为 `rbp`），并构建 `BinaryExpr` 或 `AssignmentExpr` 节点。
    *   **左方括号 `[`:** 调用 `parse_index_or_slice` 处理索引或切片，构建 `IndexExpr` 或 `SliceExpr`。
    *   **左括号 `(` (作为 LED):** 不再使用。函数应用通过 S-表达式结构处理。


### 2.4 宏定义

顶层解析器会特别识别 `符号 = \` (` 模式：

```cheng
宏名称 = ` (参数1 参数2 ...) 宏体表达式
```

这会被解析为 `DefineMacroExpr` 节点。参数列表由零个或多个符号组成。宏体是单个表达式。**注意:** 宏定义语句本身在求值时通常返回 `Nil`。

### 2.5 赋值

使用中缀运算符 `=` 进行赋值：

```cheng
变量名 = 表达式
```

解析为 `AssignmentExpr`。左侧必须是符号。赋值是右结合的。
*   **定义:** 如果变量名是首次出现，`=` 定义一个新的变量绑定。根据当前实现和测试用例，新定义的变量**默认为可变**。
*   **重新赋值:** 如果变量名已存在，`=` 对其进行重新赋值（要求变量是可变的，根据默认规则，已存在的变量都是可变的）。
**注意:** 赋值表达式本身在求值时通常返回 `Nil`。

### 2.5.1 三目运算符 (Ternary Operator)

Cheng 支持 C 风格的三目条件运算符 `?:` 作为 `if` 表达式的简洁形式。

```cheng
条件表达式 ? 真值表达式 : 假值表达式
```

解析器将 `?` 视为一个低优先级的右结合中缀运算符。当解析器遇到 `?` 时，它会解析其右侧的真值表达式，然后期望一个 `:` 标记，接着解析假值表达式。最终构建一个 `TernaryExpr(condition, true_expr, false_expr)` AST 节点。

```cheng
x = (a > b) ? a : b  将 a 和 b 中的较大值赋给 x
```

### 2.6 序列

Cheng 语言主要通过以下方式处理表达式序列：

1.  **顶层隐式序列:** 程序中的多个顶层表达式会被解析器自动包装成一个 `SequenceExpr`。
2.  **Lambda 体隐式序列:** `lambda` 特殊形式的函数体如果包含多个表达式，会被解析器自动包装成一个 `SequenceExpr`。
3.  **显式序列:** 在需要显式表示一个应按顺序求值的表达式序列的地方（例如 `if` 的分支），可以使用 `SequenceExpr`。**注意:** 当前解析器没有直接的语法来创建 `SequenceExpr`，它主要由顶层和 lambda 体的解析隐式生成。如果需要显式序列，通常通过宏（例如定义一个 `begin` 宏）或直接在求值器/运行时构建。

不再需要 `begin` 关键字作为内置特殊形式。

### 2.7 索引与切片

使用方括号 `[]` 进行索引或切片：

```cheng
序列[索引]
序列[开始:结束]
序列[:结束]
序列[开始:]
```

解析为 `IndexExpr` 或 `SliceExpr`。开始和结束索引是可选的表达式。
**(实现状态:** 测试用例 (`all_test_examples.py`) 表明此语法的求值逻辑当前尚未实现。语法本身由解析器支持。)

### 2.8 引用与解引用

*   **引用 (Borrow):** 前缀 `&`，例如 `&x`，解析为 `PrefixExpr('&', ...)`。根据当前规范，`&` 创建一个**可变引用**。语言中没有单独的不可变引用语法。
*   **解引用 (Dereference):** 前缀 `*`，例如 `*ptr`，解析为 `PrefixExpr('*', ...)`。用于访问引用指向的值。

**(实现状态:** 类型检查器 (`type_checker.py`) 支持对 `&` 和 `*` 的基本检查，包括基础的借用规则（例如，不能对已借用的变量再次进行可变借用或赋值）。求值逻辑尚未完全实现。)

### 2.9 引用与准引用

*   **引用 (Quote):**
    *   `'datum`：解析为 `QuoteExpr(datum)`。`datum` 使用 `parse_datum` 解析，它会递归地将内部结构解析为 `Symbol`, `Integer`, `Float`, `Boolean`, `String`, `Pair`, `Nil`, `QuoteExpr`, `QuasiquoteExpr`, `UnquoteExpr`, `UnquoteSplicingExpr`。
*   **准引用 (Quasiquote):**
    *   `` `datum ``：解析为 `QuasiquoteExpr(datum)`。`datum` 同样使用 `parse_datum` 解析。
*   **解引用 (Unquote):**
    *   `,expression`：解析为 `UnquoteExpr(expression)`。`expression` 使用 `parse_expression` 解析。
*   **解引用展开 (Unquote Splicing):**
    *   `,@expression`：解析为 `UnquoteSplicingExpr(expression)`。`expression` 使用 `parse_expression` 解析。

**(实现状态:** 测试用例 (`all_test_examples.py`) 表明基础的准引用、解引用和解引用展开功能已实现并有测试用例。`parse_datum` 现在能正确处理嵌套的引用/准引用标记。)

## 3. 操作符优先级与结合性

解析器根据以下绑定力（Precedence）处理运算符（从低到高）：

| 级别 (BP) | 运算符        | 结合性        | AST 节点 (示例)        | NUD/LED 函数                 |
| :-------- | :------------ | :------------ | :--------------------- | :--------------------------- |
| 1         | `=`           | 右            | `AssignmentExpr`       | `parse_assignment` (LED)     |
| 2         | `? :`         | 右            | `TernaryExpr`          | `parse_ternary` (LED on `?`) |
| 3         | `==`, `<`, `>`, `<=`, `>=` | 不结合        | `BinaryExpr`           | `parse_infix_operator` (LED) (注意: `==` 对列表执行深度比较) |
| 4         | `+`, `-`      | 左            | `BinaryExpr`           | `parse_infix_operator` (LED) |
| 5         | `*`, `/`      | 左            | `BinaryExpr`           | `parse_infix_operator` (LED) |
| 6         | `&`, `*` (前缀) | 右            | `PrefixExpr`           | `parse_prefix_operator` (NUD) |
| 6         | `` ` ``, `,`, `,@`, `'` | 右 (?)        | `QuasiquoteExpr`, etc. | NUD 函数                     |
| 7         | `[` (索引/切片) | 左            | `IndexExpr`, `SliceExpr` | `parse_index_or_slice` (LED) |
| N/A       | `(` (函数调用)  | N/A           | `ApplyExpr`            | `parse_paren_constructs` (NUD) |

**注意:**

*   括号 `()` 用于覆盖优先级，并通过 `parse_paren_constructs` 处理特殊形式和函数/宏调用 (`ApplyExpr`)。
*   比较运算符不结合，意味着 `a < b < c` 是语法错误。
*   前缀运算符 `*` (解引用) 和中缀运算符 `*` (乘法) 通过 NUD 和 LED 函数区分。
*   函数调用通过 S-表达式结构 `(函数 参数...)` 表示，由 `parse_paren_constructs` 解析为 `ApplyExpr`（见 2.3 节）。

此规范旨在反映当前解析器的行为和 `all_test_examples.py` 中的示例。随着语言的发展，这些规则可能会进一步调整。
