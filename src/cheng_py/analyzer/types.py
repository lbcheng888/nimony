# src/cheng_py/analyzer/types.py

"""
定义 Cheng 语言的类型表示，用于静态分析（类型检查和借用检查）。
"""

from typing import List, Optional, Tuple

class ChengType:
    """类型表示的基类。"""
    def __eq__(self, other):
        # 默认情况下，类型相等意味着它们是完全相同的类实例
        # 子类可以覆盖此行为（例如，对于结构相等性或子类型）
        return isinstance(other, self.__class__)

    def __hash__(self):
        # 允许将类型用作字典键等
        return hash(self.__class__.__name__)

    def __repr__(self):
        return self.__class__.__name__

# --- 基本类型 ---

class IntType(ChengType):
    """整型"""
    pass

class FloatType(ChengType):
    """浮点型"""
    pass

class BoolType(ChengType):
    """布尔型"""
    pass

class StringType(ChengType):
    """字符串类型"""
    pass

class NilType(ChengType):
    """空列表 '() 或 Nil 类型"""
    pass

class SymbolType(ChengType):
    """符号类型 (通常在 quote 或宏参数中使用，运行时不存在)"""
    # 注意：运行时通常没有 Symbol 类型的值，它们会被解析或求值。
    # 但在类型检查阶段，尤其是在处理 quote 或宏时，可能需要表示它。
    pass

class AnyType(ChengType):
    """表示任何类型，用于某些泛型原语或未指定类型的情况"""
    # 谨慎使用，可能削弱类型检查
    pass

# --- 复合类型 ---

class PairType(ChengType):
    """Cons Cell / Pair 类型"""
    def __init__(self, car_type: ChengType, cdr_type: ChengType):
        self.car_type = car_type
        self.cdr_type = cdr_type

    def __eq__(self, other):
        return (isinstance(other, PairType) and
                self.car_type == other.car_type and
                self.cdr_type == other.cdr_type)

    def __hash__(self):
        return hash((self.__class__.__name__, self.car_type, self.cdr_type))

    def __repr__(self):
        return f"Pair({self.car_type!r}, {self.cdr_type!r})"

# ListType 可以是 PairType 的一种特殊递归形式或单独定义
# 例如，List(T) 可以表示为 Pair(T, List(T)) | Nil
# 为了简化，我们可以先只用 PairType，或者定义一个 ListType 别名/辅助函数

class ListType(ChengType):
    """列表类型 List(T)"""
    def __init__(self, element_type: ChengType):
        self.element_type = element_type

    def __eq__(self, other):
        return (isinstance(other, ListType) and
                self.element_type == other.element_type)

    def __hash__(self):
        return hash((self.__class__.__name__, self.element_type))

    def __repr__(self):
        return f"List({self.element_type!r})"

class FuncType(ChengType):
    """函数类型"""
    def __init__(self, param_types: List[ChengType], return_type: ChengType):
        # TODO: Consider affine/borrow information in param/return types later
        self.param_types = tuple(param_types) # Use tuple for hashability
        self.return_type = return_type

    def __eq__(self, other):
        return (isinstance(other, FuncType) and
                self.param_types == other.param_types and
                self.return_type == other.return_type)

    def __hash__(self):
        return hash((self.__class__.__name__, self.param_types, self.return_type))

    def __repr__(self):
        params_repr = ", ".join(repr(p) for p in self.param_types)
        return f"Func(({params_repr}) -> {self.return_type!r})"

# --- 引用类型 ---

class RefType(ChengType):
    """不可变引用类型 &T"""
    def __init__(self, pointee_type: ChengType):
        self.pointee_type = pointee_type
        # TODO: Add lifetime parameter?

    def __eq__(self, other):
        return (isinstance(other, RefType) and
                self.pointee_type == other.pointee_type)

    def __hash__(self):
        return hash((self.__class__.__name__, self.pointee_type))

    def __repr__(self):
        # 根据规范，& 代表可变引用，所以类型名可以反映这一点
        # 或者保持 RefType 但在文档中说明其可变性
        # 暂时保持 Ref，但注释说明
        return f"Ref({self.pointee_type!r})" # Represents the single (mutable) reference type


# --- 单例实例 (方便使用) ---
T_INT = IntType()
T_FLOAT = FloatType()
T_BOOL = BoolType()
T_STRING = StringType()
T_NIL = NilType()
T_SYMBOL = SymbolType()
T_ANY = AnyType()

# --- 辅助函数 ---
def make_list_type(element_type: ChengType) -> ChengType:
    """辅助函数创建递归的列表类型 Pair(T, List(T)) | Nil"""
    # 这是一个简化的表示，实际类型检查可能需要更复杂的处理
    # 或者直接使用 ListType(T)
    # return PairType(element_type, ListType(element_type)) # This is recursive definition
    return ListType(element_type) # Use the simpler ListType for now

def make_func_type(param_types: List[ChengType], return_type: ChengType) -> FuncType:
    return FuncType(param_types, return_type)

def is_copy_type(t: ChengType) -> bool:
    """检查一个类型是否是可复制的 (按值传递/赋值)"""
    # 基本类型通常是可复制的
    # 引用类型 (&T) 默认应该是移动语义，除非显式 Copy
    # 列表、函数、Pair 通常不是可复制的
    # AnyType is treated as potentially copyable for simplicity, might need refinement.
    return isinstance(t, (IntType, FloatType, BoolType, StringType, NilType, SymbolType, AnyType)) # Removed RefType
