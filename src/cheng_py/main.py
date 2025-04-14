# src/cheng_py/main.py

import sys
# 更改: 导入 evaluate 和环境创建函数
from .parser.parser import parse
from .core.macro_expander import expand_macros # Task 6.4: Import expand_macros
from .core.evaluator import evaluate
from .core.environment import create_global_env, EnvironmentError
from .ast import Nil # Task 6.4: Import Nil for result check
# 注意: Python 内建 SyntaxError, TypeError, ValueError, RuntimeError


def repl():
    """基本的 Read-Eval-Print 循环"""
    print("Cheng REPL (Read-Eval-Print)")
    print("输入 S-表达式 或 'quit' 退出。")
    # 更改: 创建全局环境
    global_env = create_global_env()

    while True:
        try:
            # 1. 读取输入 (Read)
            line = input("cheng> ")
            if line.lower() == 'quit':
                break
            if not line.strip(): # 跳过空行
                continue

            # 2. 解析输入 (Parse)
            ast = parse(line)

            # 2.5 宏展开 (Expand Macros) - Task 6.4
            expanded_ast = expand_macros(ast, global_env)

            # 3. 求值表达式 (Eval)
            # 更改: 调用 evaluate on expanded_ast
            result = evaluate(expanded_ast, global_env)

            # 4. 打印结果 (Print)
            # 更改: 打印求值结果
            # 对于 define 等返回 Nil 的情况，可以选择不打印或打印特定标记
            if result is not None and result is not Nil: # 避免打印 define 的 Nil 返回值
                 print(repr(result))

        except SyntaxError as e:
            print(f"  语法错误: {e}")
        # 更改: 捕获环境错误
        except EnvironmentError as e:
            print(f"  环境错误: {e}")
        # 更改: 捕获求值期间的其他运行时错误
        except (TypeError, ValueError, RuntimeError) as e:
             print(f"  运行时错误: {e}")
        except EOFError:
            print("\n再见！") # 处理 Ctrl+D
            break # 退出 REPL
        except KeyboardInterrupt:
            print("\n中断。输入 'quit' 退出。") # 处理 Ctrl+C
        except Exception as e: # 捕获未预料的错误
            print(f"发生意外错误: {type(e).__name__}: {e}")
            # 可以考虑在这里打印堆栈跟踪以进行调试
            # import traceback
            # traceback.print_exc()

if __name__ == "__main__":
    # 假设脚本是从项目根目录使用 python -m src.cheng_py.main 运行
    # 或者 PYTHONPATH 包含项目根目录
    repl()
