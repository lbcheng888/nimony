# Cheng 语言实施计划与进度 (C 引导阶段 -> 自举)

本文档详细规划了使用 C 语言实现 Cheng 语言 L0 子集（作为引导编译器基础）的步骤，跟踪开发进度，并明确通往最终目标——Cheng 自举——的路径。

**核心目标:**

*   **同像性:** 代码即数据，基于 S-表达式。
*   **极简主义:** 最小化的核心语言，通过宏系统扩展。
*   **确定性内存管理:** 编译时保证内存安全，以所有权与借用为主，辅以 Arena 分配。
*   **C 互操作:** 与 C 语言无缝调用。
*   **并行编译:** 设计支持模块间并行编译的系统。
*   **语法:** 追求优雅与简洁。

**项目结构:**

*   `cheng_c/`: C 语言实现的 L0 子集编译器/运行时组件。
    *   `include/`: 头文件 (`l0_types.h`, `l0_arena.h`, `l0_parser.h`, `l0_eval.h`, `l0_env.h`, `l0_primitives.h`, `l0_codegen.h`...)
    *   `src/`: 源文件 (`l0_types.c`, `l0_arena.c`, `l0_parser.c`, `l0_eval.c`, `l0_env.c`, `l0_primitives.c`, `l0_codegen.c`...)
    *   `tests/`: 单元测试 (`test_arena.c`, `test_parser.c`, `test_eval.c`...)
    *   `Makefile`: 构建脚本。
*   `src/cheng/`: (未来) 使用 Cheng 语言自身编写的编译器源代码。
*   `stdlib/cheng/`: (未来) Cheng 标准库。
*   `examples/cheng/`: Cheng 语言示例代码。
*   `doc/`: 设计文档和规范 (`design.md`, `plan.md`, `cheng_grammar_zh.md`...)

**实施阶段:**

**阶段 0: L0 C 实现 (引导编译器基础) (状态: 进行中)**

*目标: 创建一个能够执行或编译 L0 子集代码的 C 程序。*

*   [X] **任务 0.1: 定义 L0 核心数据类型** (`cheng_c/include/l0_types.h`, `cheng_c/src/l0_types.c`)
    *   实现 `L0_Value` 结构体。
    *   定义 `L0_ValueType` 枚举 (NIL, BOOLEAN, INTEGER, SYMBOL, PAIR, PRIMITIVE, CLOSURE)。
    *   实现类型谓词 (`l0_is_nil`, `l0_is_pair`, etc.)。
*   [X] **任务 0.2: 实现 Arena 内存分配器** (`cheng_c/include/l0_arena.h`, `cheng_c/src/l0_arena.c`)
    *   实现 `l0_arena_create`, `l0_arena_destroy`。
    *   实现 `l0_arena_alloc` (支持对齐)。
    *   实现 `l0_arena_reset`。
    *   实现 `l0_arena_strdup` (用于复制字符串到 Arena)。
    *   将所有 `L0_Value` 构造函数 (`l0_make_...`) 修改为使用 Arena 分配。
*   [X] **任务 0.3: 实现 L0 S-表达式解析器** (`cheng_c/include/l0_parser.h`, `cheng_c/src/l0_parser.c`)
    *   实现 `l0_parse_string` API。
    *   支持解析整数、布尔值 (`#t`, `#f`)、符号。
    *   支持解析空列表 `()` 为 NIL。
    *   支持解析嵌套列表。
    *   支持 `quote` 语法糖 `'expr` 转换为 `(quote expr)`。
    *   实现基本的错误报告 (设置全局错误状态和消息)。
    *   确保所有解析过程中创建的 `L0_Value` 都通过 Arena 分配。
*   [X] **任务 0.4: 为 Arena 和 Parser 编写单元测试** (`cheng_c/tests/test_arena.c`, `cheng_c/tests/test_parser.c`)
    *   测试 Arena 的创建、分配、重置、销毁。
    *   测试解析各种原子和列表结构。
    *   测试 `quote` 语法糖的转换。
    *   测试各种解析错误情况 (未闭合列表、非法字符等)。
    *   更新 `Makefile` 以编译和运行测试。
*   [X] **任务 0.5: 实现 L0 求值器** (`l0_eval.c`, `l0_env.c`, `l0_primitives.c`)
    *   [X] **子任务 0.5.1: 环境实现** (`l0_env.h`, `l0_env.c`)
        *   定义环境结构 (例如，指向 frame 的指针，frame 包含绑定表和指向外层环境的指针)。
        *   实现 `l0_env_create` (创建全局环境)。
        *   实现 `l0_env_define` (在环境中定义变量)。
        *   实现 `l0_env_lookup` (查找变量值)。
        *   实现 `l0_env_extend` (创建扩展环境，用于 `let` 和函数调用)。
    *   [X] **子任务 0.5.2: 求值器核心** (`l0_eval.h`, `l0_eval.c`)
        *   实现 `l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena)` 函数。
        *   处理原子求值 (数字、布尔值自求值；符号在环境中查找)。
        *   实现 `quote` 特殊形式 (返回未求值的操作数)。
        *   实现 `if` 特殊形式 (求值条件，根据结果求值 true 或 false 分支)。
        *   实现 `lambda` 特殊形式 (创建 `L0_TYPE_CLOSURE` 值，包含参数列表、函数体和当前环境指针)。
        *   实现 `define` 特殊形式 (求值表达式，将结果绑定到环境中)。
        *   实现 `let` 特殊形式 (创建新环境，绑定变量，求值函数体)。
        *   实现函数应用 (求值操作符和操作数；如果是 Primitive 则直接调用 C 函数；如果是 Closure 则创建新环境，绑定参数，求值函数体)。
    *   [X] **子任务 0.5.3: 内建原语** (`l0_primitives.h`, `l0_primitives.c`)
        *   定义 `L0_PrimitiveFunc` 函数指针类型。
        *   实现 `cons`, `car`, `cdr`, `pair?`, `null?`。
        *   实现基本算术运算 (`+`, `-`, `*`, `/`)。
        *   实现基本比较运算 (`=`, `<`, `>`)。 (注意 `=` 可能需要比较不同类型)
        *   实现类型谓词 (`integer?`, `boolean?`, `symbol?` 等)。
        *   将原语注册到全局环境中。
*   [X] **任务 0.6: 为 L0 求值器编写单元测试** (`cheng_c/tests/test_eval.c`)
    *   测试原子求值和符号查找。
    *   测试 `quote`, `if`, `lambda`, `define`, `let`。
    *   测试函数应用（包括闭包和作用域）。
    *   测试所有内建原语。
    *   测试递归（例如阶乘）。
*   [X] **任务 0.7: 实现 REPL** (`cheng_c/src/main.c`)
    *   创建交互式读取-求值-打印循环。
    *   集成 Arena、Parser、Eval、Env、Primitives。
    *   更新 `Makefile` 添加 `run` 目标。
*   [X] **任务 0.8: 实现 L0 到 C 的代码生成器** (`l0_codegen.c`)
    *   *我们选择直接用 C 实现 L0->C 编译器作为 Stage 0。*
    *   [X] 定义 C 代码生成策略。
    *   [X] 实现将 L0 AST 转换为等效 C 代码的函数。
    *   [X] 提供了 C 运行时支持 (`l0_types.c`, `l0_arena.c` 等)。

**阶段 1: L0 自举编译器 (使用 L0 编写) (状态: 已完成)**

*目标: 使用 L0 语言自身编写一个能将 L0 代码编译成 C 代码的编译器。*

*   [X] **任务 1.1: 设计 L0 编译器结构** (`src/l0_compiler/compiler.l0`)
    *   [X] 定义编译器的主流程：读取 -> 解析 -> 代码生成。
    *   [X] 使用 L0 列表表示 AST。
*   [X] **任务 1.2: 在 L0 中实现解析器接口**
    *   [X] L0 代码调用 C 实现的 `parse-string` 原语。
*   [X] **任务 1.3: 在 L0 中实现 C 代码生成器**
    *   [X] 编写 L0 函数来遍历 L0 AST。
    *   [X] 生成等效的 C 代码字符串。
    *   [X] 处理符号映射和基本的作用域。
*   [X] **任务 1.4: 实现编译器入口点**
    *   [X] 编写 L0 代码来读取源文件，调用解析器和代码生成器，并将生成的 C 代码写入输出文件。

**阶段 2: 多阶段自举编译 (状态: 已完成)**

*目标: 通过多轮编译，生成一个能够稳定编译自身的 Cheng 编译器。*

*   [X] **任务 2.1: Stage 1 编译**
    *   [X] 使用 Stage 0 C 编译器 (`cheng_compiler_c.exe`) 将 `src/l0_compiler/compiler.l0` 编译成 C 代码 (`compiler_stage1.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage1.c` 和 L0 运行时，生成 Stage 1 编译器 (`compiler_stage1.exe`)。
*   [X] **任务 2.2: Stage 2 编译**
    *   [X] 使用 Stage 1 编译器 (`compiler_stage1.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 2 C 代码 (`compiler_stage2.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage2.c` 和 L0 运行时，生成 Stage 2 编译器 (`compiler_stage2.exe`)。
*   [X] **任务 2.3: Stage 3 编译**
    *   [X] 使用 Stage 2 编译器 (`compiler_stage2.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 3 C 代码 (`compiler_stage3.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage3.c` 和 L0 运行时，生成 Stage 3 编译器 (`compiler_stage3.exe`)。
*   [X] **任务 2.4: Stage 4 编译**
    *   [X] 使用 Stage 3 编译器 (`compiler_stage3.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 4 C 代码 (`compiler_stage4.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage4.c` 和 L0 运行时，生成 Stage 4 编译器 (`compiler_stage4.exe`)。
*   [X] **任务 2.5: Stage 5 编译**
    *   [X] 使用 Stage 4 编译器 (`compiler_stage4.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 5 C 代码 (`compiler_stage5.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage5.c` 和 L0 运行时，生成 Stage 5 编译器 (`compiler_stage5.exe`)。
*   [X] **任务 2.6: Stage 6 编译 (最终编译器)**
    *   [X] 使用 Stage 5 编译器 (`compiler_stage5.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 6 C 代码 (`compiler_stage6.c`)。
    *   [X] 使用 GCC 编译 `compiler_stage6.c` 和 L0 运行时，生成 Stage 6 编译器 (`cheng_c/bin/compiler_stage6.exe`)。
*   [X] **任务 2.7: Stage 7 编译 (验证)**
    *   [X] 使用 Stage 6 编译器 (`compiler_stage6.exe`) 编译 `src/l0_compiler/compiler.l0` 生成 Stage 7 C 代码 (`compiler_stage7.c`)。
*   [X] **任务 2.8: 验证自举**
    *   [X] **比较生成的 C 代码:** `fc cheng_c\compiler_stage6.c cheng_c\compiler_stage7.c` -> **结果: 相同!** (通过检查空的 `compare_stage6_stage7.log` 确认)
    *   **结论: 自举成功！编译器能够正确编译自身。最终编译器为 `cheng_c/bin/compiler_stage6.exe`。**

**阶段 3: 迭代开发与特性扩展 (状态: 未开始)**

*目标: 使用自举完成的编译器扩展语言特性。*

*   [ ] **任务 3.1: 添加 L1 特性**
    *   在 `src/l0_compiler/compiler.l0` (或未来重命名为 `compiler.cheng`) 中开始添加 L1 特性。
    *   优先考虑：更健壮的错误处理、基本的宏 (`defmacro`)、改进字符串处理等。
*   [ ] **任务 3.2: 迭代编译**
    *   使用最终的自举编译器 (`cheng_c/bin/compiler_stage6.exe`) 编译包含新特性的编译器源代码，生成下一代编译器。

**阶段 4: 完整特性实现 (状态: 未开始)**

*目标: 在自举的编译器中实现 Cheng 的核心高级特性。*

*   [ ] **任务 4.1: 实现所有权与借用系统 (状态: 进行中 - 编译器端)**
    *   [X] 设计类型表示（**简化为单一可变引用 `&T`**，移除 `&mut T` 和 `Box<T>`，暂不考虑生命周期 `'a`）。
    *   [X] **(C 运行时)** 修改 `L0_Value` (`l0_types.h`, `l0_types.c`) 以支持简化的 `L0_TYPE_REF`。
    *   [X] **(C 运行时)** 在 C 运行时添加 `deref` 原语 (`l0_primitives.h`, `l0_primitives.c`) 支持解引用。
    *   [ ] **子任务 4.1.1: 在 `compiler.l0` 中实现类型检查算法**
        *   [ ] **(编译器)** 定义类型表示：如何在编译器内部表示基本类型 (`'int`, `'bool`, `'string`, `'symbol`, `'float`, `'any`, `'void`?) 和引用类型 `'(ref T)` (例如，使用列表或符号)。
        *   [ ] **(编译器)** 实现类型环境：创建和管理用于在编译时跟踪变量符号到其类型的映射 (例如，关联列表 `((var1 . type1) (var2 . type2) ...)` )。
        *   [ ] **(编译器)** 实现 `type-check` 主函数：接收表达式和类型环境，根据表达式形式分派到具体的检查函数。
        *   [ ] **(编译器)** 实现核心形式的类型检查：
            *   字面量 (Literals): 返回其固有类型。
            *   符号 (Symbols): 在类型环境中查找符号，返回其类型，若未找到则报错。
            *   `quote`: 返回 `'any` 或特定引用类型。
            *   `if`: 检查条件类型为 `'bool`；检查 `then` 和 `else` 分支类型兼容（目前要求相同），返回该类型。
            *   `define`: 检查值的类型，将符号和类型添加到类型环境，返回 `'void` 或值的类型。
            *   `let`: 创建扩展类型环境，检查绑定值的类型并添加到扩展环境，在扩展环境中检查 `body`，返回 `body` 的类型。
            *   `lambda`: 检查函数体类型（在扩展了参数类型的环境中），构造并返回函数类型表示 (例如 `'(func (param-types...) return-type)`)。
            *   函数调用: 检查操作符是否为函数类型；检查参数类型是否匹配；返回函数定义的返回类型。
            *   `(& var)` (引用创建): 检查 `var` 是否在类型环境中存在，返回 `'(ref <type_of_var>)`。**（借用检查点 1）**
            *   `(deref expr)` (解引用): 检查 `expr` 的类型是否为 `'(ref T)`，返回类型 `T`。**（借用检查点 2）**
    *   [ ] **子任务 4.1.2: 在 `compiler.l0` 中实现简化的借用检查规则**
        *   [ ] **(编译器)** 设计借用状态跟踪：扩展类型环境或使用单独结构来跟踪每个变量的当前借用状态（例如：`'unborrowed`, `'borrowed-mut`) 和借用者标识（可选，用于更详细的错误）。
        *   [ ] **(编译器)** 集成借用检查到 `type-check`：
            *   **借用检查点 1 (`& var`)**: 检查 `var` 当前是否已被借用。如果是，则报错（违反单一可变引用规则）。如果否，则将 `var` 的状态标记为 `'borrowed-mut`。
            *   **借用检查点 2 (`deref expr`)**: （可选）可以认为 `deref` 消费了引用，可能需要更新借用状态，但这取决于具体规则。
            *   **访问检查**: 在符号查找时，检查变量是否处于 `'borrowed-mut` 状态。如果是，则报错（违反无别名访问规则）。
            *   **作用域管理**: 在 `let` 退出或函数返回时，需要清除在该作用域内产生的借用状态。
    *   [ ] **子任务 4.1.3: 创建类型/借用检查测试用例**
        *   [ ] 在 `examples/cheng/` 或 `tests/cheng/` 下创建 `.l0` 文件，测试各种类型检查场景（成功和失败）。
        *   [ ] 创建 `.l0` 文件，测试简化的借用规则（成功和失败），例如：重复可变借用、借用期间访问原变量等。
*   [ ] **任务 4.2: 实现完整的宏系统**
    *   实现 `defmacro`。
    *   实现准引用 (`\``)、反引用 (`,`)、反引用拼接 (`,@`)。
    *   实现宏卫生机制。
*   [ ] **任务 4.3: 实现模块系统**
    *   设计模块定义和导入语法。
    *   实现接口文件生成。
    *   实现基于依赖图的并行编译调度（可能需要外部构建工具辅助）。
*   [ ] **任务 4.4: 实现 C FFI (状态: 暂停)**
    *   [X] 定义 `c-declare` 和 `c-export` 语法。
    *   [X] 在 `compiler.l0` 中添加对 FFI 表单的识别和初步处理逻辑 (占位符)。
    *   [X] 实现类型映射逻辑 (Cheng 类型 <-> C 类型)。
    *   [X] 实现 C 函数原型生成 (用于 `c-declare`)。
    *   [ ] 实现 C 回调包装器生成 (用于 `c-export`)。
    *   [ ] 在代码生成阶段处理外部 C 函数调用。
    *   处理 FFI 边界的内存安全问题。
*   [ ] **任务 4.5: 设计和实现标准库**
    *   实现核心数据结构（向量、哈希表等）。
    *   实现 I/O 操作。
    *   实现并发原语（如果设计需要）。

**阶段 5: 优化与工具链 (状态: 未开始)**

*目标: 提升编译器和语言的实用性。*

*   [ ] **任务 5.1: 性能优化**
    *   分析编译器瓶颈。
    *   优化生成的 C 代码。
    *   实现编译器优化遍（例如常量折叠、内联）。
*   [ ] **任务 5.2: 开发工具链**
    *   构建系统/包管理器。
    *   调试器支持。
    *   语言服务器 (LSP)。
*   [ ] **任务 5.3: 文档与社区**
    *   编写用户文档和教程。
    *   建立社区资源。

**进度更新:**

*   *(在此处记录主要里程碑的完成情况)*
*   2025-04-15: 切换到 C 语言实现 L0 子集作为引导路径，重写并细化计划文档。
*   2025-04-15: 完成 C 语言 L0 核心类型定义 (任务 0.1)。
*   2025-04-15: 完成 C 语言 L0 Arena 分配器及测试 (任务 0.2, 部分 0.4)。
*   2025-04-15: 完成 C 语言 L0 解析器（集成 Arena）及测试 (任务 0.3, 部分 0.4)。
*   2025-04-15: 完成 C 语言 L0 求值器（环境、核心逻辑、原语）(任务 0.5)。
*   2025-04-15: 完成 C 语言 L0 求值器单元测试 (任务 0.6)。
*   2025-04-15: 完成 C 语言 L0 REPL 实现 (任务 0.7)。
*   2025-04-15: 完成 C 语言 L0 代码生成器 (任务 0.8)。
*   2025-04-15: 完成 L0 语言编写的 L0 编译器 (`compiler.l0`) (阶段 1)。
*   2025-04-16: 完成 Stage 1 编译 (任务 2.1)。
*   2025-04-16: 完成 Stage 2 编译 (任务 2.2)。
*   2025-04-16: 完成 Stage 3 编译 (任务 2.3)。
*   2025-04-16: 完成 Stage 4 编译 (任务 2.4)。
*   2025-04-16: 完成 Stage 5 编译 (任务 2.5)。
*   2025-04-16: 完成 Stage 6 编译，生成最终编译器 `cheng_c/bin/compiler_stage6.exe` (任务 2.6)。
*   2025-04-16: 完成 Stage 7 编译 (任务 2.7)。
*   2025-04-16: 完成自举验证，比较 Stage 6 和 Stage 7 C 代码，确认相同 (任务 2.8)。
*   2025-04-16: 使用 Stage 6 编译器成功编译并运行 `examples/cheng/test_float.l0`。
*   2025-04-16: 在 `src/l0_compiler/compiler.l0` 中添加 `defmacro` 解析和基础展开逻辑 (任务 3.1 部分完成)。
*   2025-04-16: 在 C 运行时添加 `eval-in-compiler-env` 原语 (任务 3.1 部分完成)。
*   2025-04-16: 使用 Stage 6 编译器编译更新后的 `compiler.l0` 生成 `compiler_stage7_macro.c` (任务 3.2 部分完成)。
*   2025-04-16: 使用 GCC 编译 `compiler_stage7_macro.c` 和运行时，生成 Stage 7 编译器 `cheng_c/bin/compiler_stage7_macro.exe` (任务 3.2 部分完成)。
*   2025-04-16: 确认 C 运行时 (`l0_primitives.c` 等) 已成功编译，包含宏支持所需的原语 (例如 `eval-in-compiler-env`)。
*   **当前状态 (2025-04-16):** Stage 7 编译器 (`cheng_c/bin/compiler_stage7_macro.exe`) 已构建，包含对 `defmacro` 和宏展开的基础支持（依赖 `eval-in-compiler-env`）。
*   2025-04-17: 修复了 C 运行时宏查找问题。修改了 `is-macro?` 和 `get-macro-transformer` 原语 (`l0_primitives.c`) 以接受宏表参数，并更新了 `l0_eval` (`l0_eval.c`) 以从环境中查找宏表并传递给原语。
*   2025-04-17: 简化引用模型，移除 `&mut T` 和 `Box<T>`，仅保留 `&T` (可变引用)。更新了 C 运行时类型 (`l0_types.h`, `l0_types.c`)。
*   2025-04-17: 在 C 运行时添加了 `deref` 原语 (`l0_primitives.h`, `l0_primitives.c`) 以支持解引用操作。
*   2025-04-17: 在 C 运行时 `l0_primitives.c` 的 `l0_register_primitives` 中添加了 `*macro-table*` 的初始化，并成功重新编译 C 运行时库。
*   2025-04-17: 修复了 C 运行时 `prim_print` 中的栈缓冲区溢出问题，改用 arena 分配。使用修复后的运行时重新编译 Stage 8 编译器，并成功运行了 `examples/cheng/test_macro_qq.l0`，确认宏扩展机制本身工作正常。
*   2025-04-17: 在 `src/l0_compiler/compiler.l0` 中简化了 `create-initial-tenv` 中的部分原语类型签名，以绕过 Stage 6 代码生成器的限制。
*   2025-04-17: 使用 Stage 6 编译器 (`cheng_c/bin/compiler_stage6.exe`) 成功编译了修改后的 `src/l0_compiler/compiler.l0`，生成了 C 代码 `cheng_c/compiler_stage7_typecheck.c`。
*   2025-04-17: 清理了 `src/l0_compiler/compiler.l0` 中的重复代码定义。
*   2025-04-17: 创建了类型/借用检查测试文件 `examples/cheng/test_type_check.l0` (任务 4.1.3 部分完成)。
*   2025-04-17: 修复了 C 运行时 `l0_eval.c` 中的括号匹配和语法问题。
*   2025-04-17: 成功编译 Stage 1 (`make bootstrap_stage1`)。
*   2025-04-17: 成功运行类型检查测试 (Stage 1) (`examples/cheng/test_type_check.l0`)，未发现运行时错误。
*   2025-04-17: 成功编译 Stage 2 (`make stage2`)。
*   2025-04-17: 成功编译 Stage 3 (`make stage3`)。
*   2025-04-17: 修正 `Makefile` 中的 `compare` 规则以比较 Stage 2 和 Stage 3。
*   2025-04-17: 验证 Stage 2 和 Stage 3 生成的 C 代码 (`compiler_stage2.c` vs `compiler_stage3.c`) 相同 (`fc /b` 结果: `Files are identical`)。
*   2025-04-17: 成功编译 Stage 6 (`make stage6`)。
*   2025-04-17: 成功编译 Stage 7 (宏支持) (`make stage7_macro`)。
*   2025-04-17: 成功构建测试可执行文件 (`make tests`)。
*   **当前状态 (2025-04-17):** `cheng_c` 目录下的编译器构建过程已成功完成 (`make all`)。核心自举过程稳定 (Stage 2 和 Stage 3 C 代码相同)。Stage 6 和 Stage 7 (宏支持) 编译器已构建。测试已构建。
*   **当前焦点 / 下一步 (2025-04-17):**
    1.  **(可选)** 运行 C 单元测试 (`test_arena.exe`, `test_parser.exe`, `test_eval.exe`) 确认基础功能正常。
    2.  **继续类型检查开发 (任务 4.1):**
        *   构建 Stage 7 编译器 (带类型检查): 使用 GCC 编译 `cheng_c/compiler_stage7_typecheck.c` 和 C 运行时，生成新的 Stage 7 编译器可执行文件 (`cheng_c/bin/compiler_stage7_typecheck.exe`)。
        *   运行类型检查测试 (Stage 7): 使用新构建的 Stage 7 编译器编译 `examples/cheng/test_type_check.l0`，观察类型检查器的输出和行为，验证其正确性并根据需要进行调试或完善 (继续任务 4.1.1, 4.1.2)。
