# src/cheng_py/runtime/runtime.py

from enum import Enum, auto
from typing import Any, Optional

# 从 AST 导入基础类型，因为运行时值会包装它们
# 使用 try-except 处理可能的循环导入或路径问题
try:
    from ..core.ast import ChengExpr, Symbol, Integer, Float, Boolean, String, Pair, Nil, Closure, Reference
except ImportError:
    # 如果直接运行此文件或路径设置有问题，尝试不同的相对路径
    # This might be needed if running tests directly in this directory later
    from cheng_py.core.ast import ChengExpr, Symbol, Integer, Float, Boolean, String, Pair, Nil, Closure, Reference


class MemoryState(Enum):
    """模拟值的内存/所有权状态"""
    VALID = auto()    # 值有效且拥有所有权
    MOVED = auto()    # 值已被移动，不再有效
    BORROWED = auto() # 值当前被（可变地）借用 (因为 & 是可变借用)

class RuntimeValue:
    """包装运行时值及其内存状态"""
    def __init__(self, value: ChengExpr, state: MemoryState = MemoryState.VALID):
        self.value: ChengExpr = value
        self.state: MemoryState = state
        # borrow_count 在当前设计中有点多余，因为我们只有一个可变借用状态 BORROWED
        # 但保留它可能对未来扩展（如区分不可变借用）有用
        # 0: 未借用, 1: 可变借用 (BORROWED state implies borrow_count=1)
        self.borrow_count: int = 1 if state == MemoryState.BORROWED else 0

    def __repr__(self) -> str:
        # Simplified repr to avoid excessive nesting if value is complex
        value_repr = repr(self.value)
        if len(value_repr) > 50:
             value_repr = type(self.value).__name__ + "(...)"
        return f"RuntimeValue({value_repr}, state={self.state.name})"

    def is_valid(self) -> bool:
        """检查值是否仍然有效（未被移动）"""
        return self.state != MemoryState.MOVED

    def mark_moved(self):
        """将值标记为已移动"""
        if not self.is_valid():
            pass # 允许重复标记为 moved
        if self.state == MemoryState.BORROWED:
            raise RuntimeError(f"Cannot move value while it is borrowed: {self!r}")
        self.state = MemoryState.MOVED
        self.value = Nil # 或者设置为一个特殊的 MovedPlaceholder 对象
        self.borrow_count = 0

    def borrow(self):
        """模拟可变借用 (&)"""
        if not self.is_valid():
            raise RuntimeError(f"Cannot borrow moved value: {self!r}")
        # 当前设计：& 是唯一的可变借用，不允许同时存在其他借用或所有权访问
        if self.state == MemoryState.BORROWED:
             raise RuntimeError(f"Cannot borrow mutably while already borrowed: {self!r}")
        self.state = MemoryState.BORROWED
        self.borrow_count = 1 # 可变借用计数为 1

    def return_borrow(self):
        """模拟归还可变借用"""
        if self.state != MemoryState.BORROWED:
            raise RuntimeError(f"Cannot return borrow for value not mutably borrowed: {self!r}")
        self.borrow_count = 0
        self.state = MemoryState.VALID # 归还后恢复有效状态

# --- 运行时环境 (模拟内存和作用域) ---

class RuntimeEnvironment:
    """
    模拟运行时环境，管理变量的 RuntimeValue 和作用域。
    与求值器的 Environment 不同，这个环境关注值的生命周期和状态。
    """
    def __init__(self, outer: Optional['RuntimeEnvironment'] = None):
        self.outer: Optional['RuntimeEnvironment'] = outer
        # 存储变量名到 RuntimeValue 的映射
        self.values: dict[str, RuntimeValue] = {}
        # 跟踪在此作用域定义的变量名，用于退出时 drop
        self.defined_in_scope: set[str] = set()

    def extend(self) -> 'RuntimeEnvironment':
        """创建一个新的嵌套作用域环境"""
        return RuntimeEnvironment(outer=self)

    def define(self, name: str, value: ChengExpr, is_copyable: bool = False):
        """定义一个新变量，创建 RuntimeValue 并赋予所有权"""
        if name in self.values:
            # 允许在内部作用域遮蔽外部作用域的变量，但不允许在同一作用域重定义
            raise NameError(f"Variable '{name}' already defined in this scope.")
        # TODO: 区分 Copyable 和 Non-Copyable 类型的值创建
        # 对于 Copyable 类型，赋值和传递参数应该是复制，而不是移动
        # 目前简化处理，所有定义都创建新的 RuntimeValue
        self.values[name] = RuntimeValue(value)
        self.defined_in_scope.add(name)
        print(f"RUNTIME: Defined '{name}' = {self.values[name]!r}") # DEBUG

    def get_runtime_value(self, name: str) -> RuntimeValue:
        """获取变量对应的 RuntimeValue 对象 (用于检查状态、借用等)"""
        env: Optional[RuntimeEnvironment] = self
        while env is not None:
            if name in env.values:
                rt_val = env.values[name]
                # 不需要在这里检查 is_valid，调用者（如 get_value, set_value, move_value）会检查
                return rt_val
            env = env.outer
        raise NameError(f"Variable '{name}' not found.")

    def get_value(self, name: str) -> ChengExpr:
        """获取变量的实际值 (执行读取操作，检查状态)"""
        rt_val = self.get_runtime_value(name)
        if not rt_val.is_valid():
             raise ValueError(f"Attempted to access moved variable '{name}'")
        # 检查是否可以读取 (当前设计中，即使可变借用也允许读取所有者，类型检查器负责阻止)
        print(f"RUNTIME: Accessed '{name}' -> {rt_val!r}") # DEBUG
        return rt_val.value

    def set_value(self, name: str, new_value: ChengExpr):
        """设置现有变量的值 (执行写入操作，检查状态和借用)"""
        rt_val = self.get_runtime_value(name)
        if not rt_val.is_valid():
             raise ValueError(f"Attempted to assign to moved variable '{name}'")
        # 检查是否可以写入 (不能在任何借用期间写入)
        if rt_val.state == MemoryState.BORROWED:
            raise RuntimeError(f"Cannot assign to variable '{name}' while it is borrowed.")
        # 更新值
        rt_val.value = new_value
        # 状态应该保持 VALID
        rt_val.state = MemoryState.VALID
        print(f"RUNTIME: Set '{name}' = {rt_val!r}") # DEBUG

    def assign_value(self, name: str, source_rt_value: RuntimeValue, is_copyable: bool = False):
         """处理赋值操作 (target = source)，考虑移动或复制"""
         print(f"RUNTIME: Assigning to '{name}' from source {source_rt_value!r} (copyable={is_copyable})") # DEBUG
         if not source_rt_value.is_valid():
              raise ValueError(f"Cannot assign from moved value: {source_rt_value!r}")
         if source_rt_value.state == MemoryState.BORROWED:
              raise RuntimeError(f"Cannot assign from borrowed value: {source_rt_value!r}")

         if is_copyable:
              # 复制：创建一个新的 RuntimeValue
              # 假设 value 本身是可深层复制的，或者对于基本类型是按值复制
              # 对于复杂类型，可能需要一个 deepcopy 机制
              copied_value = source_rt_value.value # Simple copy for now
              new_rt_val = RuntimeValue(copied_value)
              print(f"RUNTIME: Copying value to '{name}'") # DEBUG
         else:
              # 移动：获取源值，标记源为 MOVED，目标获得包含源值的新 RuntimeValue
              value_to_move = source_rt_value.value # 获取值 *之前* 标记移动
              source_rt_value.mark_moved() # 现在标记源为 moved
              new_rt_val = RuntimeValue(value_to_move) # 创建新的 RuntimeValue
              print(f"RUNTIME: Moving value to '{name}', source marked as MOVED") # DEBUG


         # 更新或定义目标变量
         if name in self.values:
             # 如果目标已存在，检查是否可写
             target_rt_val = self.values[name]
             if not target_rt_val.is_valid():
                  # 允许覆盖已移动的目标？
                  pass
             if target_rt_val.state == MemoryState.BORROWED:
                  raise RuntimeError(f"Cannot assign to variable '{name}' while it is borrowed.")
             # Drop the old value before assigning the new one
             if target_rt_val.is_valid():
                  print(f"RUNTIME: Dropping old value of '{name}' before assignment: {target_rt_val!r}")
                  target_rt_val.mark_moved() # Simulate drop

             self.values[name] = new_rt_val
             # 如果是覆盖，它不算是在此作用域新定义的
             if name in self.defined_in_scope: self.defined_in_scope.remove(name)

         else:
             # 如果目标不存在，向上查找定义
             env: Optional[RuntimeEnvironment] = self.outer
             defined_outer = False
             while env is not None:
                  if name in env.values:
                       # 目标在外部作用域定义
                       target_rt_val = env.values[name]
                       if not target_rt_val.is_valid(): pass # Allow overwrite moved
                       if target_rt_val.state == MemoryState.BORROWED:
                            raise RuntimeError(f"Cannot assign to variable '{name}' while it is borrowed.")
                       if target_rt_val.is_valid():
                            print(f"RUNTIME: Dropping old value of '{name}' (outer scope) before assignment: {target_rt_val!r}")
                            target_rt_val.mark_moved() # Simulate drop
                       env.values[name] = new_rt_val
                       defined_outer = True
                       break
                  env = env.outer

             if not defined_outer:
                  # 如果在所有外部作用域都找不到，则在当前作用域定义
                  self.values[name] = new_rt_val
                  self.defined_in_scope.add(name)

         print(f"RUNTIME: Assigned '{name}' = {self.values[name]!r}") # DEBUG


    def borrow_value(self, name: str):
         """模拟可变借用 (&)"""
         rt_val = self.get_runtime_value(name)
         if not rt_val.is_valid():
              raise ValueError(f"Attempted to borrow moved variable '{name}'")
         rt_val.borrow()
         print(f"RUNTIME: Borrowed '{name}', state is now {rt_val.state.name}") # DEBUG

    def return_borrowed_value(self, name: str):
         """模拟归还可变借用"""
         # 需要找到持有该 RuntimeValue 的环境
         env: Optional[RuntimeEnvironment] = self
         found = False
         while env is not None:
             if name in env.values:
                 rt_val = env.values[name]
                 rt_val.return_borrow()
                 print(f"RUNTIME: Returned borrow for '{name}', state is now {rt_val.state.name}") # DEBUG
                 found = True
                 break
             env = env.outer
         if not found:
              # 这理论上不应发生，如果借用检查器工作正常
              raise NameError(f"Cannot return borrow for unknown variable '{name}'")

    def move_value(self, name: str):
        """Marks a variable's value as moved (simulates move semantics)."""
        # This is primarily for the type checker's simulation.
        # It finds the value and marks its state.
        rt_val = self.get_runtime_value(name) # Raises NameError if not found
        # Check if already moved or borrowed before marking
        if not rt_val.is_valid():
             # Allow re-marking as moved? Or raise error? Let's allow for now.
             print(f"RUNTIME WARNING: Attempting to move already moved variable '{name}'")
             # raise ValueError(f"Cannot move already moved variable '{name}'")
        if rt_val.state == MemoryState.BORROWED:
             raise RuntimeError(f"Cannot move variable '{name}' while it is borrowed.")

        print(f"RUNTIME: Moving '{name}', marking as MOVED.") # DEBUG
        rt_val.mark_moved()


    def exit_scope(self):
        """模拟退出当前作用域，drop 在此作用域定义的变量"""
        print(f"RUNTIME: Exiting scope {id(self)}...") # DEBUG
        dropped_something = False
        for name in list(self.defined_in_scope): # Iterate over a copy
             if name in self.values:
                 rt_val = self.values[name]
                 if rt_val.is_valid(): # 只 drop 有效的值
                     print(f"RUNTIME: Dropping '{name}' = {rt_val!r} (defined in scope {id(self)})") # DEBUG
                     # 在实际实现中，这里会释放内存
                     rt_val.mark_moved() # 标记为 moved 作为 drop 的模拟
                     dropped_something = True
                 # 从环境中移除，即使它是 moved 或 borrowed (如果借用未归还，这是个错误，但 drop 仍需发生)
                 # 或者只移除 dropped 的？如果 borrowed 变量离开作用域怎么办？
                 # 借用检查器应该阻止这种情况。假设 drop 只发生在有效值上。
                 # del self.values[name] # 从字典中移除？或者只标记？标记更安全
             # 从 defined_in_scope 集合中移除
             self.defined_in_scope.remove(name)
        if not dropped_something:
             print(f"RUNTIME: No values to drop in scope {id(self)}.") # DEBUG
