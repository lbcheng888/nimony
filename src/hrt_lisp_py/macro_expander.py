import dataclasses
import copy # Needed for deep copying AST nodes during substitution
from typing import Dict, Optional, List, Any, Union

# Adjust import path similarly to parser.py and semantic_analyzer.py
try:
    from .ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode,
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, UnquoteSplicingNode, # Added UnquoteSplicingNode
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression,
        LetBinding,
        print_ast # For debugging
    )
    from .semantic_analyzer import Environment, SemanticError # Need Environment to lookup macros
except ImportError:
    # Fallback for running script directly or different project structure
    from src.hrt_lisp_py.ast_nodes import (
        ASTNode, Program, Symbol, DefineNode, DefmacroNode, LambdaNode, LetNode,
        IfNode, FunctionCallNode, SetBangNode, BeginNode,
        QuoteNode, QuasiquoteNode, UnquoteNode, UnquoteSplicingNode, # Added UnquoteSplicingNode
        SExpression,
        IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral, Expression,
        LetBinding,
        print_ast
    )
    from src.hrt_lisp_py.semantic_analyzer import Environment, SemanticError

# Define known keywords/special forms that have dedicated AST nodes
# This helps the quasiquote handler distinguish them from regular function calls.
KNOWN_KEYWORDS = {
    "if", "let", "lambda", "define", "defmacro", "set!", "begin",
    "quote", "quasiquote", "unquote"
}

# Internal marker for splicing during quasiquote expansion
class _SpliceMarker:
    """Internal marker to indicate result should be spliced."""
    def __init__(self, elements: List[ASTNode]):
        self.elements = elements

class MacroExpansionError(Exception):
    """Custom exception for errors during macro expansion."""
    pass

class MacroExpander:
    """
    Performs macro expansion on an AST.
    This is typically done after parsing and before full semantic analysis or code generation.
    """
    def __init__(self, initial_env: Environment):
        # We need the environment primarily to look up macro definitions.
        # Expansion might happen in phases, potentially updating the environment.
        self.env = initial_env
        # Counter to help generate unique symbols if needed (gensym) - basic version
        self._gensym_counter = 0

    def expand(self, node: ASTNode) -> ASTNode:
        """Public entry point to start macro expansion on a node."""
        # We traverse the tree and potentially replace nodes.
        # The visit method should return the (potentially new) node.
        return self.visit(node)

    def visit(self, node: ASTNode) -> ASTNode:
        """Generic visit method (Visitor pattern dispatcher). Returns the potentially transformed node."""
        method_name = f'visit_{type(node).__name__}'
        visitor = getattr(self, method_name, self.generic_visit)
        return visitor(node)

    def generic_visit(self, node: ASTNode) -> ASTNode:
        """Default visitor: recursively visit children and rebuild the node if any child changed."""
        changed = False
        new_fields = {}
        for field in dataclasses.fields(node):
            old_value = getattr(node, field.name)
            new_value = old_value

            if isinstance(old_value, ASTNode):
                new_value = self.visit(old_value)
            elif isinstance(old_value, list):
                new_list = []
                for item in old_value:
                    if isinstance(item, ASTNode):
                        new_item = self.visit(item)
                        new_list.append(new_item)
                        if new_item is not item:
                            changed = True
                    else:
                        new_list.append(item) # Keep non-AST items as is
                new_value = new_list
            # Note: LetBinding is handled via its fields if generic_visit is used on LetNode

            new_fields[field.name] = new_value
            if new_value is not old_value:
                changed = True

        if changed:
            # If any child node was replaced, create a new node of the same type
            # This assumes dataclasses can be reconstructed this way.
            return type(node)(**new_fields)
        else:
            # If no children changed, return the original node
            return node

    # --- Visitor Methods for Specific Node Types ---

    def visit_Program(self, node: Program) -> Program:
        """Expand expressions in the program body."""
        new_body = []
        for expr in node.body:
            expanded_expr = self.visit(expr)
            # Macro expansion might return a sequence (e.g., via begin)
            # Handle this by extending the list if BeginNode is returned
            if isinstance(expanded_expr, BeginNode):
                 new_body.extend(expanded_expr.expressions)
            else:
                 new_body.append(expanded_expr)

        # Only create new Program node if body actually changed
        if any(new is not old for new, old in zip(new_body, node.body)) or len(new_body) != len(node.body):
             # Convert list to tuple for Program node
             return Program(body=tuple(new_body))
        else:
             return node


    def visit_FunctionCallNode(self, node: FunctionCallNode) -> ASTNode:
        """The core of macro expansion: check if the call target is a macro."""
        # First, check if the function position itself needs expansion (unlikely but possible)
        # Although, typically macros aren't allowed in function position directly.
        # Let's assume the function part is a Symbol for macro calls.
        if not isinstance(node.function, Symbol):
            # If it's not a symbol, treat it as a regular function call and expand arguments
            expanded_function = self.visit(node.function)
            expanded_args = [self.visit(arg) for arg in node.arguments]
            if expanded_function is not node.function or any(new is not old for new, old in zip(expanded_args, node.arguments)):
                 return FunctionCallNode(function=expanded_function, arguments=expanded_args)
            else:
                 return node # No changes

        # It's a symbol, check if it's a macro
        macro_name = node.function.name
        macro_definition = self.env.lookup_macro(macro_name)

        if macro_definition and isinstance(macro_definition, DefmacroNode):
            # --- It's a macro call! ---
            print(f"Expanding macro: {macro_name}") # Debug print

            # 1. Check argument count, considering rest parameter
            num_fixed_params = len(macro_definition.params)
            num_received = len(node.arguments)
            has_rest_param = macro_definition.rest_param is not None

            if has_rest_param:
                # If rest param exists, must receive at least the fixed number of args
                if num_received < num_fixed_params:
                    raise MacroExpansionError(
                        f"Macro '{macro_name}' expects at least {num_fixed_params} arguments "
                        f"(due to rest parameter '{macro_definition.rest_param.name}'), but received {num_received}"
                    )
            else:
                # If no rest param, must receive exactly the fixed number of args
                if num_received != num_fixed_params:
                    raise MacroExpansionError(
                        f"Macro '{macro_name}' expects exactly {num_fixed_params} arguments, "
                        f"but received {num_received}"
                    )

            # 2. Create substitution map (macro param symbol name -> argument AST node)
            # Arguments are NOT evaluated/expanded before substitution.
            substitutions: Dict[str, ASTNode] = {}
            # Bind fixed parameters
            for i in range(num_fixed_params):
                param = macro_definition.params[i]
                arg = node.arguments[i]
                substitutions[param.name] = arg

            # Bind rest parameter if it exists
            if has_rest_param:
                rest_args = node.arguments[num_fixed_params:]
                # Wrap the rest arguments in an SExpression node (representing a list)
                # Important: Use deepcopy for the arguments being put into the list
                # to avoid aliasing issues if they are used elsewhere or modified.
                # Convert list to tuple for SExpression node
                rest_list_node = SExpression(elements=tuple(copy.deepcopy(arg) for arg in rest_args))
                substitutions[macro_definition.rest_param.name] = rest_list_node

            # 3. Substitute arguments into a *copy* of the macro body
            # Deep copy is crucial to avoid modifying the original macro definition
            # and to handle nested expansions correctly.
            macro_body_copy = copy.deepcopy(macro_definition.body)
            expanded_body = self._substitute(macro_body_copy, substitutions)

            # 4. Recursively expand the result of the substitution
            # A macro might expand into code that contains another macro call.
            result_node = self.expand(expanded_body) # Call expand recursively

            print(f"--- Macro '{macro_name}' expanded to: ---") # Debug print
            print_ast(result_node, indent="  ")
            print(f"---------------------------------------")

            return result_node # Return the fully expanded body

        else:
            # --- It's a regular function call ---
            # Expand arguments first
            expanded_args = [self.visit(arg) for arg in node.arguments]
            # Return a new node only if arguments changed
            if any(new is not old for new, old in zip(expanded_args, node.arguments)):
                 # Convert list to tuple for FunctionCallNode
                 return FunctionCallNode(function=node.function, arguments=tuple(expanded_args)) # Keep original function symbol
            else:
                 return node # No changes

    def _substitute(self, node: ASTNode, substitutions: Dict[str, ASTNode]) -> ASTNode:
        """
        Recursively substitutes symbols in the macro body copy based on the substitution map.
        This is a simple substitution, not handling hygiene yet.
        """
        if isinstance(node, Symbol) and node.name in substitutions:
            # If the symbol is a macro parameter, replace it with a deep copy
            # of the corresponding argument AST node. Copying is important
            # if the same argument is used multiple times in the macro body.
            return copy.deepcopy(substitutions[node.name])
        elif isinstance(node, ASTNode):
            # Recursively substitute in children
            new_fields = {}
            for field in dataclasses.fields(node):
                old_value = getattr(node, field.name)
                new_value = old_value

                if isinstance(old_value, ASTNode):
                    new_value = self._substitute(old_value, substitutions)
                elif isinstance(old_value, list):
                    new_list = []
                    for item in old_value:
                        if isinstance(item, ASTNode):
                            new_list.append(self._substitute(item, substitutions))
                        else:
                            new_list.append(item) # Keep non-AST items
                    new_value = new_list

                new_fields[field.name] = new_value

            # Reconstruct the node with substituted children
            # We assume the node was already copied before calling _substitute,
            # so we modify its fields directly.
            for name, value in new_fields.items():
                 setattr(node, name, value)
            return node
        else:
            # Should not happen with AST nodes, but return as is if it does
            return node

    # --- Visit methods that might need special handling ---

    def visit_DefineNode(self, node: DefineNode) -> DefineNode:
        """Expand the value being defined."""
        expanded_value = self.visit(node.value)
        if expanded_value is not node.value:
            return DefineNode(symbol=node.symbol, value=expanded_value)
        else:
            return node

    def visit_DefmacroNode(self, node: DefmacroNode) -> DefmacroNode:
        """Macros define macros? Possible, but complex. For now, don't expand inside macro body at definition time."""
        # We *could* expand the body here, but that's usually not how Lisp macros work.
        # Expansion happens at the call site.
        # Also, defining the macro should have already happened before expansion starts.
        return node # Return the definition as is

    def visit_LambdaNode(self, node: LambdaNode) -> LambdaNode:
        """Expand the body of the lambda."""
        # Note: Need to consider hygiene if lambda captures macro parameters.
        # Simple substitution doesn't handle this.
        expanded_body = self.visit(node.body)
        if expanded_body is not node.body:
            return LambdaNode(params=node.params, body=expanded_body)
        else:
            return node

    def visit_LetNode(self, node: LetNode) -> LetNode:
        """Expand binding values and the body."""
        new_bindings = []
        bindings_changed = False
        for binding in node.bindings:
            expanded_value = self.visit(binding.value)
            new_bindings.append(LetBinding(symbol=binding.symbol, value=expanded_value))
            if expanded_value is not binding.value:
                bindings_changed = True

        expanded_body = self.visit(node.body)
        body_changed = expanded_body is not node.body

        if bindings_changed or body_changed:
            # Convert list to tuple for LetNode bindings
            return LetNode(bindings=tuple(new_bindings), body=expanded_body)
        else:
            return node

    def visit_IfNode(self, node: IfNode) -> IfNode:
        """Expand condition, then, and else branches."""
        expanded_cond = self.visit(node.condition)
        expanded_then = self.visit(node.then_branch)
        expanded_else = self.visit(node.else_branch)
        if (expanded_cond is not node.condition or
                expanded_then is not node.then_branch or
                expanded_else is not node.else_branch):
            return IfNode(condition=expanded_cond, then_branch=expanded_then, else_branch=expanded_else)
        else:
            return node

    def visit_SetBangNode(self, node: SetBangNode) -> SetBangNode:
        """Expand the value being assigned."""
        expanded_value = self.visit(node.value)
        if expanded_value is not node.value:
            return SetBangNode(symbol=node.symbol, value=expanded_value)
        else:
            return node

    def visit_BeginNode(self, node: BeginNode) -> BeginNode:
         """Expand expressions within the begin block."""
         new_expressions = []
         expressions_changed = False
         for expr in node.expressions:
             expanded_expr = self.visit(expr)
             # Handle nested expansion possibly returning BeginNode
             if isinstance(expanded_expr, BeginNode):
                 new_expressions.extend(expanded_expr.expressions)
                 expressions_changed = True # Assume change if BeginNode was flattened
             else:
                 new_expressions.append(expanded_expr)
                 if expanded_expr is not expr:
                     expressions_changed = True

         if expressions_changed:
             # If only one expression remains after flattening, return it directly
             if len(new_expressions) == 1:
                  return new_expressions[0]
             else:
                  # Convert list to tuple for BeginNode
                  return BeginNode(expressions=tuple(new_expressions))
         else:
             return node


    def visit_QuoteNode(self, node: QuoteNode) -> QuoteNode:
        """Do not expand inside quoted expressions."""
        return node # Return as is

    # Literals and Symbols are terminals for expansion (unless substituted)
    def visit_Symbol(self, node: Symbol) -> Symbol: return node
    def visit_IntegerLiteral(self, node: IntegerLiteral) -> IntegerLiteral: return node
    def visit_FloatLiteral(self, node: FloatLiteral) -> FloatLiteral: return node
    def visit_StringLiteral(self, node: StringLiteral) -> StringLiteral: return node
    def visit_BoolLiteral(self, node: BoolLiteral) -> BoolLiteral: return node
    def visit_SExpression(self, node: SExpression) -> SExpression:
         # Expand elements within a generic SExpression if encountered
         return self.generic_visit(node)

    def visit_QuasiquoteNode(self, node: QuasiquoteNode) -> ASTNode:
        """Handles the expansion of a quasiquoted expression."""
        # Start the quasiquote expansion process at level 1
        # The substitutions dictionary is passed for handling unquoted macro parameters
        return self._handle_quasiquote(node.expression, level=1)

    def _handle_quasiquote(self, node: ASTNode, level: int) -> ASTNode:
        """
        Recursively processes nodes within a quasiquote context.
        level: Tracks the nesting depth of quasiquotes. Unquoting happens at level 1.
        """
        if level == 0: # Should not happen if called correctly from visit_QuasiquoteNode
             return self.visit(node)

        if isinstance(node, UnquoteNode):
            if level == 1:
                # Level 1 unquote: Expand the inner expression *fully*
                # This is where the substitution/evaluation happens.
                # In our macro expander context, "evaluate" means "expand".
                print(f"  (Quasiquote) Unquoting at level 1: {type(node.expression).__name__}") # Debug
                return self.visit(node.expression) # Expand the unquoted part
            else:
                # Nested unquote (level > 1): Decrease level and recurse
                # The result should be (unquote ...)
                print(f"  (Quasiquote) Nested unquote at level {level}, reducing level.") # Debug
                processed_expr = self._handle_quasiquote(node.expression, level - 1)
                # Reconstruct the UnquoteNode if the inner part changed
                if processed_expr is not node.expression:
                     return UnquoteNode(expression=processed_expr)
                else:
                     return node # Return original if inner part didn't change

        elif isinstance(node, UnquoteSplicingNode):
            if level == 1:
                # Level 1 unquote-splicing: Expand inner expression and expect a list
                print(f"  (Quasiquote) Unquote-splicing at level 1: {type(node.expression).__name__}") # Debug
                expanded_inner = self.visit(node.expression)
                # The result of expansion MUST be an SExpression (list) to be spliced
                if isinstance(expanded_inner, SExpression):
                    # Return a marker containing the elements to be spliced
                    print(f"  (Quasiquote) Splicing {len(expanded_inner.elements)} elements.") # Debug
                    return _SpliceMarker(elements=expanded_inner.elements) # Return marker
                else:
                    # If the expanded result is not a list, it's an error for splicing
                    raise MacroExpansionError(
                        f"Unquote-splicing (,@) requires the expression "
                        f"'{type(node.expression).__name__}' to evaluate to a list (SExpression), "
                        f"but got {type(expanded_inner).__name__}"
                    )
            else:
                # Nested unquote-splicing (level > 1): Decrease level and recurse
                print(f"  (Quasiquote) Nested unquote-splicing at level {level}, reducing level.") # Debug
                processed_expr = self._handle_quasiquote(node.expression, level - 1)
                # Reconstruct the UnquoteSplicingNode if the inner part changed
                # Ensure UnquoteSplicingNode is imported at the top level
                if processed_expr is not node.expression:
                     return UnquoteSplicingNode(expression=processed_expr)
                else:
                     return node # Return original

        elif isinstance(node, QuasiquoteNode):
            # Nested quasiquote: Increase level and recurse
            # The result should be (quasiquote ...)
            print(f"  (Quasiquote) Nested quasiquote at level {level}, increasing level.") # Debug
            processed_expr = self._handle_quasiquote(node.expression, level + 1)
            # Reconstruct the QuasiquoteNode if the inner part changed
            if processed_expr is not node.expression:
                 return QuasiquoteNode(expression=processed_expr)
            else:
                 return node # Return original

        # --- Handle lists/SExpressions/Calls ---
        # Inside quasiquote, how we handle list-like structures depends on
        # whether they represent a known special form or just data.
        elif isinstance(node, FunctionCallNode):
            func_symbol = node.function
            # Check if it's a call to a known keyword/special form
            is_keyword_call = isinstance(func_symbol, Symbol) and func_symbol.name in KNOWN_KEYWORDS

            if is_keyword_call:
                # --- It's a keyword form (like `if`, `let`, `lambda`) ---
                keyword_name = func_symbol.name
                print(f"  (Quasiquote) Reconstructing keyword form '{keyword_name}' at level {level}.") # Debug

                processed_args = []
                list_changed = False
                for arg in node.arguments:
                    processed_arg = self._handle_quasiquote(arg, level)
                    if isinstance(processed_arg, _SpliceMarker):
                        processed_args.extend(processed_arg.elements)
                        list_changed = True
                    else:
                        processed_args.append(processed_arg)
                        if processed_arg is not arg:
                            list_changed = True

                # Reconstruct the specific node based on the keyword
                if keyword_name == 'lambda':
                    if len(processed_args) < 1: raise MacroExpansionError(f"Generated lambda form is missing parameter list: {processed_args}")
                    params_node = processed_args[0]
                    body_nodes = processed_args[1:]
                    final_params: List[Symbol] = []
                    if isinstance(params_node, SExpression) and all(isinstance(p, Symbol) for p in params_node.elements): final_params = params_node.elements
                    else: raise MacroExpansionError(f"Generated lambda parameter list is not a list of symbols: {type(params_node).__name__}")
                    final_body: Expression
                    if not body_nodes: raise MacroExpansionError("Generated lambda form has no body.")
                    elif len(body_nodes) == 1: final_body = body_nodes[0]
                    else:
                        # Convert list to tuple for BeginNode
                        final_body = BeginNode(expressions=tuple(body_nodes))
                    print(f"  (Quasiquote) Constructing LambdaNode with params: {[p.name for p in final_params]}, body: {type(final_body).__name__}")
                    # Convert list to tuple for LambdaNode params
                    return LambdaNode(params=tuple(final_params), body=final_body)
                elif keyword_name == 'if':
                     if len(processed_args) != 3: raise MacroExpansionError(f"Generated 'if' form requires 3 arguments, got {len(processed_args)}")
                     return IfNode(condition=processed_args[0], then_branch=processed_args[1], else_branch=processed_args[2])
                # Add more keywords as needed...
                else:
                     # Fallback: If keyword reconstruction not implemented, treat as data list (SExpression)
                     # Or better: Fall through to generic ASTNode handler below
                     print(f"  (Quasiquote) Keyword '{keyword_name}' reconstruction falling back to generic handler.")
                     pass # Let generic handler process it

            else:
                # --- It's a regular function call treated as data template ---
                print(f"  (Quasiquote) Treating function call '{func_symbol.name if isinstance(func_symbol, Symbol) else type(func_symbol).__name__}' as data list at level {level}.") # Debug
                items_to_process = [node.function] + node.arguments
                new_elements = []
                list_changed = False
                for elem in items_to_process:
                    processed_elem = self._handle_quasiquote(elem, level)
                    if isinstance(processed_elem, _SpliceMarker):
                        new_elements.extend(processed_elem.elements)
                        list_changed = True
                    else:
                        new_elements.append(processed_elem)
                        if processed_elem is not elem:
                            list_changed = True
                # Return SExpression for data lists, converting list to tuple
                return SExpression(elements=tuple(new_elements)) if list_changed else node

        elif isinstance(node, SExpression):
             if node.elements and isinstance(node.elements[0], Symbol) and node.elements[0].name in KNOWN_KEYWORDS:
                  keyword_name = node.elements[0].name
                  print(f"  (Quasiquote) Treating SExpression starting with keyword '{keyword_name}' like FunctionCallNode.")
                  processed_args = []
                  list_changed = False
                  for arg in node.elements[1:]:
                      processed_arg = self._handle_quasiquote(arg, level)
                      if isinstance(processed_arg, _SpliceMarker):
                          processed_args.extend(processed_arg.elements)
                          list_changed = True
                      else:
                          processed_args.append(processed_arg)
                          if processed_arg is not arg:
                              list_changed = True

                  # Reconstruct based on keyword (similar logic as above)
                  if keyword_name == 'lambda':
                      if len(processed_args) < 1: raise MacroExpansionError(f"Generated lambda form is missing parameter list: {processed_args}")
                      params_node = processed_args[0]; body_nodes = processed_args[1:]
                      final_params: List[Symbol] = []
                      if isinstance(params_node, SExpression) and all(isinstance(p, Symbol) for p in params_node.elements): final_params = list(params_node.elements) # Keep as list temporarily
                      else: raise MacroExpansionError(f"Generated lambda parameter list is not a list of symbols: {type(params_node).__name__}")
                      final_body: Expression
                      if not body_nodes: raise MacroExpansionError("Generated lambda form has no body.")
                      elif len(body_nodes) == 1: final_body = body_nodes[0]
                      else:
                          # Convert list to tuple for BeginNode
                          final_body = BeginNode(expressions=tuple(body_nodes))
                      # Convert list to tuple for LambdaNode params
                      return LambdaNode(params=tuple(final_params), body=final_body)
                  elif keyword_name == 'if':
                      if len(processed_args) != 3: raise MacroExpansionError(f"Generated 'if' form requires 3 arguments, got {len(processed_args)}")
                      return IfNode(condition=processed_args[0], then_branch=processed_args[1], else_branch=processed_args[2])
                  # Add more keywords...
                  else:
                      print(f"  (Quasiquote) Warning: Keyword '{keyword_name}' reconstruction in SExpression not fully implemented.")
                      if list_changed:
                           all_elements = [node.elements[0]] + processed_args
                           # Convert list to tuple for SExpression
                           return SExpression(elements=tuple(all_elements))
                      else:
                           return node
             else:
                  # --- Regular SExpression (data list) ---
                  print(f"  (Quasiquote) Treating SExpression as data list at level {level}.") # Debug
                  new_elements = []
                  # *** CORRECTED INDENTATION STARTS HERE ***
                  changed = False # Tracks if any element itself changed *or* if splicing occurred
                  for elem in node.elements:
                      processed_elem = self._handle_quasiquote(elem, level)
                      if isinstance(processed_elem, _SpliceMarker):
                          new_elements.extend(processed_elem.elements)
                          changed = True
                      else:
                          new_elements.append(processed_elem)
                          if processed_elem is not elem:
                              changed = True
                  # Convert list to tuple for SExpression
                  return SExpression(elements=tuple(new_elements)) if changed else node
                  # *** CORRECTED INDENTATION ENDS HERE ***

        # --- Handle Atoms ---
        elif isinstance(node, (Symbol, IntegerLiteral, FloatLiteral, StringLiteral, BoolLiteral)):
            print(f"  (Quasiquote) Treating atom {type(node).__name__} literally at level {level}.") # Debug
            return node # Return atom as is

        # --- Handle other complex nodes (IfNode, LetNode, DefineNode, etc.) ---
        # This also handles keyword FunctionCallNodes that fell through.
        elif isinstance(node, ASTNode):
             node_type_name = type(node).__name__
             if isinstance(node, FunctionCallNode) and isinstance(node.function, Symbol):
                 if node.function.name in KNOWN_KEYWORDS:
                      node_type_name = f"Keyword({node.function.name})"

             print(f"  (Quasiquote) Processing generic/keyword node {node_type_name} at level {level}.") # Debug
             changed = False
             new_fields = {}
             for field in dataclasses.fields(node):
                 old_value = getattr(node, field.name)
                 new_value = old_value
                 if isinstance(old_value, ASTNode):
                     new_value = self._handle_quasiquote(old_value, level)
                 elif isinstance(old_value, list):
                     new_list = []
                     list_changed = False
                     for item in old_value:
                         if isinstance(item, ASTNode):
                             processed_item = self._handle_quasiquote(item, level)
                             if isinstance(processed_item, _SpliceMarker):
                                 new_list.extend(processed_item.elements)
                                 list_changed = True
                             else:
                                 new_list.append(processed_item)
                                 if processed_item is not item:
                                     list_changed = True
                         else:
                             new_list.append(item)
                     if list_changed:
                         new_value = new_list
                         changed = True
                     else:
                          new_value = old_value

                 new_fields[field.name] = new_value
                 if new_value is not old_value:
                     changed = True

             if changed:
                 # Before reconstructing, check if any list field needs conversion to tuple
                 final_fields = {}
                 node_type = type(node)
                 for field_info in dataclasses.fields(node_type):
                     field_name = field_info.name
                     field_type = field_info.type
                     current_value = new_fields[field_name]
                     # Check if the field type is Tuple and the current value is a List
                     # Need to handle potential Union types or generic Tuple like Tuple[ASTNode, ...]
                     # A simple check for `Tuple` in the type hint string might be sufficient for now
                     # Or check origin if using typing.get_origin (more robust)
                     is_tuple_type = False
                     try:
                         # Attempt to get origin for complex types like Tuple[Symbol, ...]
                         from typing import get_origin
                         origin = get_origin(field_type)
                         if origin is tuple or origin is Tuple:
                             is_tuple_type = True
                     except ImportError: # Fallback for older Python?
                         if 'Tuple' in str(field_type): # Less reliable check
                             is_tuple_type = True

                     if is_tuple_type and isinstance(current_value, list):
                         print(f"  (Quasiquote) Converting list field '{field_name}' to tuple for {node_type_name}.") # Debug
                         final_fields[field_name] = tuple(current_value)
                     else:
                         final_fields[field_name] = current_value

                 # Reconstruct the node of the *same type* using potentially converted fields
                 return node_type(**final_fields)
             else:
                 return node
        else:
             return node

    # Add visit_UnquoteNode - it should generally not be visited directly by the main
    # visitor during expansion, as it's handled inside _handle_quasiquote.
    # If visited outside quasiquote, it's an error or should be treated literally.
    def visit_UnquoteNode(self, node: UnquoteNode) -> ASTNode:
        # Unquote outside of quasiquote is often an error, but for expansion,
        # maybe just expand its content? Or treat as literal? Let's expand content.
        # This might need refinement based on desired language semantics.
        print(f"Warning: Unquote visited outside quasiquote context. Expanding content.") # Debug
        return UnquoteNode(expression=self.visit(node.expression))

    # Add visit_UnquoteSplicingNode for completeness
    def visit_UnquoteSplicingNode(self, node: UnquoteSplicingNode) -> ASTNode:
        # Similar to UnquoteNode, splicing outside quasiquote is likely an error.
        print(f"Warning: Unquote-splicing visited outside quasiquote context. Expanding content.") # Debug
        # We still return UnquoteSplicingNode, as splicing needs list context.
        return UnquoteSplicingNode(expression=self.visit(node.expression))


# --- Helper Function ---
def expand_macros(program_ast: Program, env: Environment) -> Program:
    """Expands all macro calls within the Program AST."""
    expander = MacroExpander(env)
    # Expansion might change the top-level Program node itself
    expanded_ast = expander.expand(program_ast)
    if not isinstance(expanded_ast, Program):
         # If expansion resulted in something other than a Program (e.g., a single BeginNode that got reduced)
         # wrap it back into a Program, or handle error appropriately.
         # For simplicity, let's assume expand always returns a Program or raises error.
         # A more robust approach might handle single non-program nodes.
         raise MacroExpansionError("Macro expansion did not result in a Program node.")
    print("Macro expansion phase complete.")
    return expanded_ast

# --- Example Usage (can be added later) ---
# if __name__ == '__main__':
#     from .parser import parse
#     from .semantic_analyzer import SemanticAnalyzer # Need analyzer to build initial env
#
#     test_code = """
#     (define x 10)
#     (defmacro unless (condition body)
#       (list 'if condition #f body)) ; Simplified body
#
#     (unless (> x 5) (print "X is not large"))
#     (unless (< x 5) (print "X is not small"))
#     """
#     try:
#         ast = parse(test_code)
#         print("--- Initial AST ---")
#         print_ast(ast)
#         print("--------------------")
#
#         # Need to run semantic analysis first to populate the environment with macro definitions
#         analyzer = SemanticAnalyzer()
#         analyzer.analyze(ast) # This defines 'unless' in the environment
#         print("Initial semantic analysis (for macro defs) successful.")
#
#         # Now perform macro expansion using the populated environment
#         expanded_ast = expand_macros(ast, analyzer.current_env)
#         print("\n--- Expanded AST ---")
#         print_ast(expanded_ast)
#         print("--------------------")
#
#         # Optionally, run semantic analysis again on the expanded code
#         print("\n--- Semantic Analysis on Expanded AST ---")
#         # Need a new analyzer or reset the environment if analysis modifies it
#         final_analyzer = SemanticAnalyzer() # Use a fresh analyzer
#         final_analyzer.analyze(expanded_ast)
#         print("-----------------------------------------")
#
#     except Exception as e:
#         print(f"\nAn error occurred: {e}")
#         import traceback
#         traceback.print_exc()
