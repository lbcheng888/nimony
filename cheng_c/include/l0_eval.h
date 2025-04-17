#ifndef L0_EVAL_H
#define L0_EVAL_H

#include "l0_types.h"
#include "l0_env.h"
#include "l0_arena.h"

/**
 * @brief Evaluates an L0 expression within a given environment.
 *
 * This is the core function of the L0 interpreter. It handles:
 * - Self-evaluating atoms (numbers, booleans, nil).
 * - Symbol lookup in the environment.
 * - Special forms (quote, if, lambda, define, let).
 * - Function application (primitive and closure calls).
 *
 * All newly created values during evaluation (e.g., closure objects, results of primitives)
 * should be allocated in the provided arena.
 *
 * @param expr The L0 expression (as an L0_Value*) to evaluate.
 * @param env The environment in which to evaluate the expression.
 * @param arena The memory arena to use for allocations during evaluation.
 * @return A pointer to the L0_Value representing the result of the evaluation,
 *         or NULL if a runtime error occurred (check l0_parser_error_status/message).
 */
L0_Value* l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena);

/**
 * @brief Helper function to evaluate a list of expressions.
 * Used for evaluating arguments in function calls or bodies in let/lambda.
 *
 * @param list The list of expressions (car of each pair is an expression).
 * @param env The environment to evaluate expressions in.
 * @param arena The arena for allocations.
 * @return A new list containing the evaluated results, or NULL on error.
 */
L0_Value* l0_eval_list(L0_Value* list, L0_Env* env, L0_Arena* arena);

/**
 * @brief Recursively expands macros within an L0 expression AST.
 *
 * This function traverses the AST. When it encounters a list whose car
 * is a symbol bound to a macro in the *macro-table* of the provided environment,
 * it applies the macro's transformer function to the unevaluated arguments
 * and recursively expands the resulting code. Other nodes are traversed
 * recursively.
 *
 * @param expr The L0 expression (AST node) to expand.
 * @param env The environment containing the *macro-table*.
 * @param arena The memory arena for allocating new nodes during expansion.
 * @return A pointer to the L0_Value representing the macro-expanded AST,
 *         or NULL if an error occurred during expansion.
 */
L0_Value* l0_macroexpand(L0_Value* expr, L0_Env* env, L0_Arena* arena);


/**
 * @brief Applies a function (primitive or closure) to a list of evaluated arguments.
 *
 * This function is typically called by l0_eval when it encounters a function application.
 *
 * @param func The function value to apply (must be L0_TYPE_PRIMITIVE or L0_TYPE_CLOSURE).
 * @param args A list of *evaluated* argument values.
 * @param env The current environment (needed for closure application).
 * @param arena The arena for allocations (e.g., creating new environments for closures).
 * @return The result of the function application, or NULL on error.
 */
L0_Value* l0_apply(L0_Value* func, L0_Value* args, L0_Env* env, L0_Arena* arena);


#endif // L0_EVAL_H
