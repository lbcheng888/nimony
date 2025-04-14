# Cheng 语言实施计划与进度 (Python 引导阶段)

本文档概述了使用 Python 实现 Cheng 语言初始版本（引导编译器）的计划，并将用于跟踪开发进度。最终目标是实现 Cheng 自举。

**核心目标:**

*   同像性 (S-表达式)
*   极简主义 (小核心 + 宏)
*   确定性内存管理 (仿射类型 + 有限借用)
*   优雅简洁的语法

**项目结构:**

*   `src/cheng_py/`: Python 实现的引导编译器源代码
    *   `parser/`: S-表达式解析器 (`parser.py`)
    *   `analyzer/`: 语义分析器 (`analyzer.py`, `types.py`, `borrow_checker.py`)
    *   `codegen/`: 代码生成器 (目标可能是 Python 字节码或 Cheng 自身) (`codegen.py`)
    *   `runtime/`: 运行时支持库 (`runtime.py`)
    *   `core/`: 核心语言原语和特殊形式实现 (`core.py`, `environment.py`)
*   `tests/cheng_py/`: 针对 Python 实现的单元测试和集成测试
*   `examples/cheng/`: 示例代码
*   `stdlib/cheng/`: 标准库

**实施阶段:**

**阶段 1: 核心解析器与 AST (状态: 已完成)**

*   [X] 任务 1.1: 定义核心 AST 节点类 (`src/cheng_py/core/ast.py`) (Symbol, Integer, Float, Boolean, String, Pair)。
*   [X] 任务 1.2: 实现 S-表达式解析器 (`src/cheng_py/parser/parser.py`)，将文本转换为 AST。**注意:** 当前解析器将 `(f x)` 解析为 `ListLiteralExpr`，求值器负责处理函数应用。
*   [X] 任务 1.3: 实现基本的 AST `__repr__` 或 `__str__` 方法，用于调试。
*   [X] 任务 1.4: 为解析器和 AST 节点编写单元测试 (`tests/cheng_py/test_parser.py`, `tests/cheng_py/test_ast.py`)。
*   [X] 任务 1.5: 构建一个最小的 REPL 框架 (仅 Read-Print)。

**阶段 2: 基础求值器与核心语义 (状态: 已完成)**

*   [X] 任务 2.1: 实现基础 `eval` 函数 (`src/cheng_py/core/evaluator.py`) 处理原子值。
*   [X] 任务 2.2: 实现 `quote` 特殊形式。
*   [X] 任务 2.3: 实现环境（Environment）类 (`src/cheng_py/core/environment.py`) 用于变量绑定和查找。
*   [X] 任务 2.4: 实现 `if` 特殊形式。
*   [X] 任务 2.5: 实现 `=` 用于顶层和局部变量定义/赋值 (AssignmentExpr)。**注意:** 变量默认可变。
*   [X] 任务 2.6: 实现核心列表操作原语 (`cons`, `car`, `cdr`, `pair?`, `null?`)。
*   [X] 任务 2.7: 实现基本算术原语 (`+`, `-`, `*`, `/`)。
*   [X] 任务 2.8: 为求值器和核心原语编写单元测试 (`tests/cheng_py/test_evaluator.py`)。
*   [X] 任务 2.9: 扩展 REPL 以支持 Eval (Read-Eval-Print)。

**阶段 3: 函数、闭包与作用域 (状态: 已完成)**

*   [X] 任务 3.1: 实现 `lambda` 特殊形式以创建函数（闭包）。
*   [X] 任务 3.2: 实现函数应用 (`apply`) 逻辑。
*   [X] 任务 3.3: 确保正确的词法作用域规则。
*   [X] 任务 3.4: 实现 `let` 用于局部绑定 (非递归)。
*   [X] 任务 3.6: 为函数、闭包和作用域编写单元测试。

**阶段 4: 仿射类型系统与借用检查 (核心) (状态: 基本完成)**

*   [X] 任务 4.1: 设计并实现类型表示 (`src/cheng_py/analyzer/types.py`) (基本类型, RefType 完成, **MutRefType 已移除**)。
*   [X] 任务 4.2: 实现类型推断/检查的基础框架 (`src/cheng_py/analyzer/type_checker.py`) (核心形式, AssignmentExpr (`=`), PrefixExpr (`&`, `*`), BinaryExpr 等基本检查完成)。
*   [X] 任务 4.3: 实现仿射类型规则检查（值使用跟踪，包括移动语义）。 (TypeChecker 中已加入基础移动检查，**运行时模拟已支持状态跟踪**)
*   [X] 任务 4.4: 设计并实现基础的借用规则 (`&` **为可变引用**) 和检查 (`src/cheng_py/analyzer/type_checker.py` 中引入 BorrowEnvironment/State，基本别名检查完成，**运行时模拟已支持状态跟踪**)
*   [X] 任务 4.5: 将类型检查和借用检查集成到分析阶段 (TypeChecker 使用 BorrowEnvironment，**并与运行时模拟交互**)。
*   [X] 任务 4.6: 定义处理类型/借用错误的机制 (基本 TypeError)。
*   [X] 任务 4.7: 为类型系统和借用检查器编写单元测试 (`tests/cheng_py/test_type_checker.py`，`tests/cheng_py/all_test_examples.py` 包含基本 `&`, `*` 和借用错误测试，核心类型检查功能已测试，**所有测试通过**)。
*   [ ] 任务 4.8: 实现完整的借用检查规则 (别名、生命周期 - 进行中)。
*   [X] **任务 4.9:** 修复 `pytest` 收集测试时的 `ModuleNotFoundError` (通过添加 `__init__.py` 文件解决)。

**阶段 5: 宏系统 (状态: 已完成)**

*   [X] 任务 5.1: 实现 `=` 特殊形式 (通过 `DefineMacroExpr` AST 节点)。
*   [X] 任务 5.2: 实现宏展开阶段（在求值之前，`src/cheng_py/core/macro_expander.py`）。
*   [ ] 任务 5.3: 决定并实现宏卫生策略（当前为非卫生）。
*   [X] 任务 5.4: 为宏定义和展开编写单元测试 (`tests/cheng_py/test_evaluator.py` 中已包含)。

**阶段 6: Python 运行时与内存模拟 (状态: 进行中)**

*   [X] 任务 6.1: 实现 Python 运行时 (`src/cheng_py/runtime/runtime.py`)，模拟 Cheng 的核心数据结构和内存管理行为（仿射性/借用）。(核心模拟完成，包括状态、作用域、移动/复制、借用/归还)
*   [X] 任务 6.2: 确保解释器/代码生成器能与模拟运行时正确交互。(类型检查器已成功与运行时模拟集成，用于状态跟踪和规则检查)

**阶段 7: 代码生成 (目标: Cheng 自身 或 解释执行) (状态: 未开始)**

*   [ ] 任务 7.1: 决定引导编译器的输出：是直接解释执行 AST，还是生成 Cheng 源代码/字节码？(当前为解释执行)
*   [ ] 任务 7.2: (如果生成代码) 设计代码生成器架构 (`src/cheng_py/codegen/codegen.py`)。
*   [ ] 任务 7.3: (如果生成代码) 实现将分析后的 AST 转换为目标表示。
*   [ ] 任务 7.4: (如果生成代码) 处理仿射类型/借用规则在生成代码中的体现。

**阶段 8: 引导编译器完善 (状态: 未开始)**

*   [ ] 任务 8.1: 逐步用 Python 实现足够的 Cheng 功能，使其能够编译自身的核心模块。
*   [ ] 任务 8.2: 编写 Cheng 语言的示例程序 (`examples/cheng/`) 并用 Python 编译器运行/编译。

**阶段 9: 自举尝试 (状态: 未开始)**

*   [ ] 任务 9.1: 使用 Python 引导编译器编译 Cheng 编译器自身的源代码（用 Cheng 编写）。
*   [ ] 任务 9.2: 调试并迭代，直到自举成功。
*   [ ] 任务 9.3: 验证自举生成的编译器能够再次编译自身并产生相同结果。

**阶段 10: 后续开发 (使用自举后的 Cheng 编译器) (状态: 未开始)**

*   [ ] 任务 10.1: 完善标准库 (`stdlib/cheng/`)。
*   [ ] 任务 10.2: 性能优化（编译器和生成代码）。
*   [ ] 任务 10.3: 构建工具链（包管理器等）。
*   [ ] 任务 10.4: 改进错误报告和文档。


**进度更新:**

*   *(在此处记录主要里程碑的完成情况)*
*   2025-04-13: 项目目录结构创建完毕。
*   2025-04-13: 切换到 Python 作为引导实现语言，更新计划文档。
*   2025-04-13: 完成阶段 1 (解析器与 AST)。
*   2025-04-13: 完成阶段 2 (基础求值器与核心语义)。
*   2025-04-13: 完成阶段 3 (函数、闭包与作用域，除 letrec)。
*   2025-04-13: 完成阶段 5 (基本宏系统)。
*   2025-04-14: 开始阶段 4 (类型系统与借用检查)，实现 Ref/MutRef 类型，let mut，以及基础的借用检查结构和规则。
*   2025-04-14: 根据语法规范和测试用例，更新解析器 (`parser.py`)：移除 `(quote ...)` 支持，将 `(f x)` 解析为 `ListLiteralExpr`。
*   2025-04-14: 更新类型系统和检查器：移除 `&mut`/`MutRefType`，将 `&` 定义为唯一的可变引用，更新变量定义 (`=`) 默认为可变，调整借用检查逻辑。
*   2025-04-14: 更新语法规范 (`cheng_grammar_zh.md`) 和测试用例 (`all_test_examples.py`) 以反映引用和可变性更改。
*   2025-04-14: 运行测试发现 `ModuleNotFoundError`，需要修复 (任务 4.9)。
*   2025-04-14: 实现引用 (`&`)、解引用 (`*`) 和通过解引用赋值 (`*target = value`) 的解析和求值逻辑，并更新测试。
*   2025-04-14: 修复 `pytest` 的 `ModuleNotFoundError` (任务 4.9 完成)。
*   2025-04-14: 实现核心运行时模拟 (`src/cheng_py/runtime/runtime.py`)，包括状态、作用域、移动/复制、借用/归还 (任务 6.1 完成)。
*   2025-04-14: 为运行时模拟添加单元测试 (`tests/cheng_py/test_runtime.py`)。
*   2025-04-14: 将运行时模拟成功集成到类型检查器 (`src/cheng_py/analyzer/type_checker.py`)，用于状态跟踪和规则检查 (任务 6.2 完成)。
*   2025-04-14: 更新 `pytest.ini` 以使用 `--tb=short`，解决错误输出过长问题。
*   2025-04-14: 修复 `test_type_checker.py` 中所有失败的测试用例，通过调整测试预期 (`all_test_examples.py`) 和类型检查器逻辑 (`type_checker.py`) (任务 4.7 完成)。
