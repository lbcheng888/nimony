# src/cheng_py/core/environment.py

from typing import Any, Optional, Dict, Callable
# 更改: 导入 AST 节点和原语函数
from .ast import Symbol, ChengExpr
from .macros import Macro # Task 6.1: Import Macro
from . import primitives

class EnvironmentError(Exception):
    """环境相关错误"""
    pass

class Environment:
    def __init__(self, outer: Optional['Environment'] = None):
        self.bindings: Dict[str, Any] = {}
        self.macros: Dict[str, Macro] = {} # Task 6.1: Add macro storage
        self.outer = outer # 指向外部作用域的环境

    def define(self, name: str, value: Any): # Changed name type to str
        """在当前环境中定义一个新变量"""
        # TODO: 处理重定义？ (目前允许覆盖)
        self.bindings[name] = value

    def lookup(self, name: str) -> Any: # Changed name type to str
        """查找变量的值，会沿着作用域链向上查找"""
        if name in self.bindings:
            return self.bindings[name]
        elif self.outer is not None:
            return self.outer.lookup(name) # Pass str directly
        else:
            raise EnvironmentError(f"Undefined symbol: {name}")

    def set(self, name: str, value: Any): # Changed name type to str
        """设置已存在变量的值，会沿着作用域链向上查找"""
        if name in self.bindings:
            self.bindings[name] = value
        elif self.outer is not None:
            self.outer.set(name, value) # Pass str directly
        else:
            raise EnvironmentError(f"Cannot set undefined symbol: {name}")

    # --- Task 6.1: Macro definition and lookup ---
    def define_macro(self, name: str, macro: Macro): # Changed name type to str
        """Define a macro in the current environment."""
        # TODO: Handle macro redefinition?
        self.macros[name] = macro

    def lookup_macro(self, name: str) -> Optional[Macro]: # Changed name type to str
        """Look up a macro, searching outer environments."""
        if name in self.macros:
            return self.macros[name]
        elif self.outer is not None:
            return self.outer.lookup_macro(name) # Pass str directly
        else:
            return None # Macro not found

# 步骤 2 & 3: 创建并填充全局环境
def create_global_env() -> 'Environment':
    """创建并返回一个包含内建原语的全局环境"""
    env = Environment()

    # 注册列表操作原语
    env.define("cons", primitives.prim_cons)
    env.define("car", primitives.prim_car)
    env.define("cdr", primitives.prim_cdr)

    # 注册类型谓词原语
    env.define("pair?", primitives.prim_is_pair)
    env.define("null?", primitives.prim_is_null)
    env.define("list", primitives.prim_list)    # Added list primitive
    env.define("append", primitives.prim_append) # Added append primitive

    # 任务 2.7: 注册算术原语
    env.define("+", primitives.prim_add)
    env.define("-", primitives.prim_sub)
    env.define("*", primitives.prim_mul)
    env.define("/", primitives.prim_div)

    # 注册比较原语 (包括新添加的)
    env.define("=", primitives.prim_eq)
    env.define(">", primitives.prim_gt)
    env.define("<", primitives.prim_lt)
    env.define(">=", primitives.prim_ge)
    env.define("<=", primitives.prim_le)

    # 注册相等性原语
    env.define("eq?", primitives.prim_eq_q)
    env.define("equal?", primitives.prim_equal_q)

    return env
