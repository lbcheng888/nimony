# examples/cheng/parse_example.py

import sys
import os

# 确保能找到 src 目录下的模块
# 获取当前脚本的目录
script_dir = os.path.dirname(os.path.abspath(__file__))
# 获取项目根目录 (假设 examples 在根目录下一层)
project_root = os.path.dirname(script_dir)
# 获取 src 目录
src_dir = os.path.join(project_root, 'src')
# 将 src 目录添加到 Python 路径
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)

try:
    from cheng_py.parser.parser import parse, ParseError
    from cheng_py.core.ast import ChengExpr # Import base type for type hint
except ImportError as e:
    print(f"错误：无法导入解析器模块: {e}", file=sys.stderr)
    print("请确保脚本位于项目结构内，并且依赖项已安装。", file=sys.stderr)
    sys.exit(1)

# Cheng 代码示例
cheng_code = """
;; 定义一个简单的平方函数
(define square
  (lambda (x)
    (* x x)))

;; 调用函数 (虽然我们还不能求值，但可以解析)
(square 5)
"""

print("--- Cheng Code ---")
print(cheng_code)
print("------------------")

try:
    # 解析代码
    asts: list[ChengExpr] = parse(cheng_code)

    print("\n--- Parsed ASTs ---")
    for i, ast in enumerate(asts):
        print(f"AST {i+1}: {repr(ast)}")
    print("-------------------")

except ParseError as e:
    print(f"\n解析错误: {e}")
except Exception as e:
    print(f"\n发生意外错误: {e}")
