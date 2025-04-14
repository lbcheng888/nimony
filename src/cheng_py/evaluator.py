# src/cheng_py/evaluator.py

from typing import Any, List, Dict # Add Dict
from .core.ast import (
    ChengAstNode, Symbol, Integer, Float, Boolean, String, Pair, Nil, ChengExpr,
    DefineMacroExpr # Import DefineMacroExpr
)
from .core.environment import Environment, EnvironmentError
from .core.macros import Macro # Import Macro
from .core.macro_expander import expand_macros # Import expand_macros

class EvaluationError(Exception):
    """求值错误"""
    pass

class Procedure:
    """表示一个由 lambda 创建的函数 (闭包)"""
    def __init__(self, params: List[Symbol], body: List[ChengExpr], closure_env: Environment):
        self.params = params
        self.body = body # 函数体可能包含多个表达式
        self.closure_env = closure_env

    def __repr__(self):
        # 提供一个简单的表示
        param_names = " ".join(p.value for p in self.params)
        return f"<Procedure ({param_names})>"


def evaluate(expr: ChengExpr, env: Environment) -> Any:
    """
    在给定环境中求值一个表达式 (AST 节点)。
    在求值前进行宏扩展。
    """
    # --- Task 6.4: Integrate Macro Expansion ---
    # Expand macros before evaluating the expression
    # Note: This might need refinement regarding when/how expansion happens,
    # especially with nested defines or macros defining other macros.
    # For now, expand the whole expression upfront.
    try:
        expr = expand_macros(expr, env)
    except Exception as e:
        # Catch expansion errors separately if needed
        raise EvaluationError(f"Error during macro expansion: {e}") from e

    # --- Original Evaluation Logic ---
    if isinstance(expr, (Integer, Float, Boolean, String)):
        # 1. 自我求值类型 (除了 Nil 和 Symbol)
        return expr.value # 返回原始 Python 值，而不是 AST 节点
    elif expr is Nil:
        # Nil 也自我求值，但它没有 .value 属性
        return expr # 或者返回 Python 的 None? 保持 Nil 更符合 Lisp 传统
    elif isinstance(expr, Symbol):
        # 2. 符号：在环境中查找
        try:
            return env.lookup(expr)
        except EnvironmentError as e:
            raise EvaluationError(str(e)) from e
    elif isinstance(expr, Pair):
        # 3. 列表/对
        # 空列表 () 在 Lisp 中通常自我求值，或者在某些上下文中为 false
        # 但作为表达式求值，它通常是错误的，除非它是 quote 的结果
        # 我们将在处理函数调用时再明确这一点，现在先处理特殊形式

        # 检查是否是特殊形式或函数调用
        # 确保列表非空才能访问 car
        if expr.car is Nil and expr.cdr is Nil:
             raise EvaluationError("Cannot evaluate empty list '()'")

        first_element = expr.car
        if isinstance(first_element, Symbol):
            op_name = first_element.value
            # 3.1 特殊形式处理
            if op_name == 'quote':
                # (quote expr) -> expr (不求值)
                # 验证结构：(quote <datum>)
                if not isinstance(expr.cdr, Pair) or expr.cdr.cdr is not Nil:
                     raise EvaluationError(f"Malformed quote: expected (quote <datum>), got {expr}")
                # 返回 quote 的参数 AST 节点本身，不进行求值
                return expr.cdr.car
            elif op_name == 'if':
                # (if test conseq alt)
                # TODO: 实现 if 特殊形式
                raise NotImplementedError("Special form 'if' not implemented yet")
            elif op_name == 'define':
                # (define var expr)
                # TODO: 实现 define 特殊形式 (需要处理 DefineExpr AST node if parser creates it)
                # Assuming parser still creates Pair for now: (define name value)
                if not isinstance(expr.cdr, Pair) or not isinstance(expr.cdr.car, Symbol) or \
                   not isinstance(expr.cdr.cdr, Pair) or expr.cdr.cdr.cdr is not Nil:
                    raise EvaluationError(f"Malformed define: expected (define symbol value), got {expr}")
                var_name = expr.cdr.car
                value_expr = expr.cdr.cdr.car
                value = evaluate(value_expr, env)
                env.define(var_name, value)
                return Nil # Define returns Nil

            # --- Task 6.1 & 6.4: Handle DefineMacroExpr ---
            # Note: expand_macros now returns DefineMacroExpr, so we handle it here,
            # not as a Pair starting with 'define-macro'.
            # elif op_name == 'define-macro': # This case is unlikely if expand_macros works correctly
            #     pass # Should be handled by the DefineMacroExpr case below

            elif op_name == 'lambda':
                # (lambda (params...) body...)
                # 验证结构: (lambda <param-list> <body>...)
                if not isinstance(expr.cdr, Pair) or \
                   not isinstance(expr.cdr.car, (Pair, Nil)) or \
                   not isinstance(expr.cdr.cdr, Pair): # Body 至少要有一个表达式
                    raise EvaluationError(f"Malformed lambda: expected (lambda (params...) body...), got {expr}")

                params_list_ast = expr.cdr.car
                body_ast = expr.cdr.cdr # Body 是一个 Pair 链表

                # 提取参数符号列表
                params: List[Symbol] = []
                current_param = params_list_ast
                while isinstance(current_param, Pair):
                    if not isinstance(current_param.car, Symbol):
                        raise EvaluationError(f"Lambda parameters must be symbols, got {current_param.car} in {expr}")
                    params.append(current_param.car)
                    current_param = current_param.cdr
                if current_param is not Nil: # 检查参数列表是否正确结束
                    raise EvaluationError(f"Malformed parameter list in lambda: {params_list_ast}")

                # 提取函数体表达式列表
                body: List[ChengExpr] = []
                current_body = body_ast
                while isinstance(current_body, Pair):
                    body.append(current_body.car)
                    current_body = current_body.cdr
                if current_body is not Nil: # Body 列表也应以 Nil 结尾
                     raise EvaluationError(f"Malformed body in lambda: {body_ast}")
                if not body: # 函数体不能为空
                    raise EvaluationError(f"Lambda body cannot be empty: {expr}")


                # 创建并返回 Procedure 对象 (闭包)
                # 捕获当前的 env 作为闭包环境
                return Procedure(params, body, env)

            # --- Task 6.3: Handle Quasiquote Forms (needed for macro body evaluation) ---
            elif op_name == 'quasiquote':
                if not isinstance(expr.cdr, Pair) or expr.cdr.cdr is not Nil:
                    raise EvaluationError(f"Malformed quasiquote: expected (quasiquote <datum>), got {expr}")
                return quasiquote_eval(expr.cdr.car, env)
            elif op_name == 'unquote':
                 # Unquote should only appear inside quasiquote, handled by quasiquote_eval
                 raise EvaluationError(f"'unquote' appeared outside of quasiquote context: {expr}")
            elif op_name == 'unquote-splicing':
                 # Unquote-splicing should only appear inside quasiquote
                 raise EvaluationError(f"'unquote-splicing' appeared outside of quasiquote context: {expr}")

            # TODO: 添加其他特殊形式如 set!, begin, let 等

            else:
                # 3.2 函数调用 (操作符是符号)
                procedure = evaluate(first_element, env) # 求值操作符符号
                # 求值操作数
                args = []
                current_arg = expr.cdr
                while isinstance(current_arg, Pair):
                    args.append(evaluate(current_arg.car, env))
                    current_arg = current_arg.cdr
                if current_arg is not Nil:
                    raise EvaluationError(f"Malformed argument list in call: {expr}")

                # 应用过程
                return apply_procedure(procedure, args, expr) # 传入 expr 用于错误信息

        else:
             # 3.3 函数调用 (操作符是表达式)
             # 例如 ((lambda (x) (+ x 1)) 5)
             procedure = evaluate(first_element, env) # 求值操作符表达式
             # 求值操作数 (与上面相同)
             args = []
             current_arg = expr.cdr
             while isinstance(current_arg, Pair):
                 args.append(evaluate(current_arg.car, env))
                 current_arg = current_arg.cdr
             if current_arg is not Nil:
                 raise EvaluationError(f"Malformed argument list in call: {expr}")

             # 应用过程
             return apply_procedure(procedure, args, expr)

    # --- Handle DefineMacroExpr AST Node ---
    elif isinstance(expr, DefineMacroExpr):
         # Create Macro object and define it in the environment
         # The body is already expanded by the initial expand_macros call
         macro = Macro(expr.params, expr.body, env) # Capture current env
         env.define_macro(expr.name, macro)
         return Nil # Defining a macro returns Nil

    else:
        # 不应该到达这里，除非 AST 类型未被处理
        raise EvaluationError(f"Cannot evaluate unknown expression type: {type(expr)}")

# --- 初始全局环境 ---
def create_global_env() -> Environment:
    """创建并填充全局环境 (目前为空)"""
    env = Environment()
    # TODO: 添加内建函数 (+, -, *, /, cons, car, cdr, list, eq?, etc.)
    # env.define(Symbol("+"), some_add_function)
    return env

# --- 应用过程 ---
def apply_procedure(proc: Any, args: List[Any], call_expr: ChengExpr) -> Any:
    """将过程应用于参数列表"""
    if isinstance(proc, Procedure):
        # 用户定义的函数 (Procedure)
        # 检查参数数量
        if len(proc.params) != len(args):
            raise EvaluationError(f"Arity mismatch: procedure {proc} expected {len(proc.params)} arguments, got {len(args)} in {call_expr}")

        # 创建新的调用环境，链接到闭包环境
        call_env = Environment(outer=proc.closure_env)

        # 绑定参数到新环境
        for param, arg in zip(proc.params, args):
            call_env.define(param, arg)

        # 在新环境中依次求值函数体
        last_result = Nil # 默认返回值，如果 body 为空 (虽然 lambda 检查了)
        for body_expr in proc.body:
            last_result = evaluate(body_expr, call_env)

        return last_result

    # TODO: 处理内建函数 (BuiltinProcedure)
    # elif isinstance(proc, BuiltinProcedure):
    #     return proc.apply(args)

    else:
        raise EvaluationError(f"Not a procedure: cannot apply {proc} in {call_expr}")



# --- Task 6.3: Quasiquote Evaluation Helper ---
def quasiquote_eval(template: ChengExpr, env: Environment) -> ChengExpr:
    """Evaluates a quasiquoted template."""
    if not isinstance(template, Pair):
        # Atoms evaluate to themselves (symbols are quoted)
        return template # Return the AST node itself

    if template is Nil:
        return Nil

    first = template.car
    rest = template.cdr

    # Check for unquote and unquote-splicing
    if isinstance(first, Pair):
        op = first.car
        if isinstance(op, Symbol):
            if op.value == 'unquote':
                 # Evaluate the argument of unquote
                 if not isinstance(first.cdr, Pair) or first.cdr.cdr is not Nil:
                      raise EvaluationError(f"Malformed unquote: {first}")
                 unquoted_val = evaluate(first.cdr.car, env)
                 # Recursively quasiquote the rest and cons the evaluated value
                 processed_rest = quasiquote_eval(rest, env)
                 return Pair(unquoted_val, processed_rest) # Cons the *evaluated* value

            elif op.value == 'unquote-splicing':
                 # Evaluate the argument of unquote-splicing, must be a list
                 if not isinstance(first.cdr, Pair) or first.cdr.cdr is not Nil:
                      raise EvaluationError(f"Malformed unquote-splicing: {first}")
                 spliced_list_val = evaluate(first.cdr.car, env)
                 # Ensure it evaluated to a list structure (Pair or Nil)
                 if not isinstance(spliced_list_val, (Pair, Nil)):
                      raise EvaluationError(f"Argument to unquote-splicing must evaluate to a list, got {type(spliced_list_val)}")

                 # Recursively quasiquote the rest
                 processed_rest = quasiquote_eval(rest, env)

                 # Append the evaluated list to the processed rest
                 # Need a helper to append two list structures
                 return append_list_structures(spliced_list_val, processed_rest)

    # If not unquote/unquote-splicing, process recursively
    processed_car = quasiquote_eval(first, env)
    processed_cdr = quasiquote_eval(rest, env)

    # Optimization: if nothing changed, return original pair
    if processed_car is first and processed_cdr is rest:
        return template
    else:
        return Pair(processed_car, processed_cdr)

def append_list_structures(list1: ChengExpr, list2: ChengExpr) -> ChengExpr:
    """Appends two AST list structures (Pair/Nil). Modifies list1 in place."""
    if list1 is Nil:
        return list2
    if not isinstance(list1, Pair): # Should be checked before call
        raise TypeError("append_list_structures: list1 must be Pair or Nil")

    # Find the last pair of list1
    last_pair = list1
    while isinstance(last_pair.cdr, Pair):
        last_pair = last_pair.cdr

    # Append list2
    if last_pair.cdr is Nil:
        last_pair.cdr = list2
        return list1
    else:
        # Should not happen if list1 is a proper list
        raise TypeError("append_list_structures: list1 is not a proper list")


# --- REPL 修改 (稍后进行) ---
# 需要修改 main.py 中的 REPL 来调用 evaluate 并打印结果，而不是打印 AST
