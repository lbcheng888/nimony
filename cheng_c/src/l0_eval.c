#include "l0_eval.h"
#include "l0_types.h"
#include "l0_env.h"
#include "l0_arena.h"
#include "l0_parser.h" // For error reporting
#include "l0_primitives.h" // For L0_PrimitiveFunc type and prim_append
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h> // For exit()

// --- Refactoring Special Form Handling ---
typedef enum {
    SF_NONE,
    SF_QUOTE,
    SF_QUASIQUOTE,
    SF_IF,
    SF_LAMBDA,
    SF_DEFINE,
    SF_SET,
    SF_LET,
    SF_DEFMACRO,
    SF_AND,
    SF_OR,      // Added or
    SF_BEGIN,
    SF_COND,    // Added cond
    SF_UNQUOTE, // Includes unquote-splicing for error check
} L0_SpecialFormType;

// Helper to map symbol string to special form enum (Simplified and reordered)
static L0_SpecialFormType get_special_form_type(const char* sym) {
    if (!sym) return SF_NONE;
    fprintf(stderr, "[DEBUG get_special_form_type v2] Checking symbol: '%s'\n", sym);

    if (strcmp(sym, "if") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'if'\n"); return SF_IF; }
    if (strcmp(sym, "lambda") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'lambda'\n"); return SF_LAMBDA; }
    if (strcmp(sym, "define") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'define'\n"); return SF_DEFINE; }
    // Check quasiquote BEFORE quote
    if (strcmp(sym, "quasiquote") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'quasiquote'\n"); return SF_QUASIQUOTE; } // SF_QUASIQUOTE = 2
    if (strcmp(sym, "quote") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'quote'\n"); return SF_QUOTE; }          // SF_QUOTE = 1
    if (strcmp(sym, "set!") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'set!'\n"); return SF_SET; }
    if (strcmp(sym, "let") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'let'\n"); return SF_LET; }
    if (strcmp(sym, "begin") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'begin'\n"); return SF_BEGIN; }
    if (strcmp(sym, "cond") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'cond'\n"); return SF_COND; } // Added cond check
    if (strcmp(sym, "and") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'and'\n"); return SF_AND; }
    if (strcmp(sym, "or") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'or'\n"); return SF_OR; } // Added or check
    if (strcmp(sym, "defmacro") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'defmacro'\n"); return SF_DEFMACRO; }
    if (strcmp(sym, "unquote") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'unquote'\n"); return SF_UNQUOTE; }
    if (strcmp(sym, "unquote-splicing") == 0) { fprintf(stderr, "[DEBUG get_special_form_type v2] Matched 'unquote-splicing'\n"); return SF_UNQUOTE; } // Treat same as unquote for now

    fprintf(stderr, "[DEBUG get_special_form_type v2] No match found for '%s', returning SF_NONE\n", sym);
    return SF_NONE;
}
// --- End Refactoring ---


// Static variable to track recursion depth for l0_eval
static int eval_depth = 0;
#define MAX_EVAL_DEPTH 1000 // Define a maximum depth

// Forward declarations
L0_Value* l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena);
static L0_Value* l0_expand_quasiquote(L0_Value* template, L0_Env* env, L0_Arena* arena, int depth);

// Helper to evaluate a sequence of expressions (like in a function body or let body)
// Returns the result of the *last* expression evaluated.
static L0_Value* l0_eval_sequence(L0_Value* seq, L0_Env* env, L0_Arena* arena) {
    L0_Value* result = l0_make_nil(arena); // Default result if sequence is empty
    if (!result) return NULL; // Allocation failed

    L0_Value* current_expr_node = seq;
    while (l0_is_pair(current_expr_node)) {
        L0_Value* expr_to_eval = current_expr_node->data.pair.car;
        result = l0_eval(expr_to_eval, env, arena);
        if (!result || l0_parser_error_status != L0_PARSE_OK) {
            // Error occurred during evaluation of one expression, propagate NULL
            return NULL;
        }
         current_expr_node = current_expr_node->data.pair.cdr;
     }
      if (!l0_is_nil(current_expr_node)) {
          l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
          l0_parser_error_message = l0_arena_strdup(arena, "Body sequence is not a proper list.");
          fprintf(stderr, "[DEBUG C eval_sequence EXIT] Returning NULL (not proper list)\n");
          return NULL;
      }
     fprintf(stderr, "[DEBUG C eval_sequence EXIT] Returning last result: %p, Status: %d\n", (void*)result, l0_parser_error_status);
     return result; // Return the result of the last evaluation
 }

// --- Function Application ---
L0_Value* l0_apply(L0_Value* func, L0_Value* args, L0_Env* env, L0_Arena* arena) {
    // <<< ADDED DETAILED PRE-CHECK LOGGING >>>
    fprintf(stderr, "[DEBUG C APPLY PRE-CHECK] Entering l0_apply. func=%p, args=%p\n", (void*)func, (void*)args);
    // <<< END DETAILED PRE-CHECK LOGGING >>>

    // <<< ADDED NULL CHECK >>>
    if (func == NULL) {
        fprintf(stderr, "[FATAL ERROR in l0_apply] Attempted to apply NULL function! (Detected immediately after entry)\n"); // Modified message
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Attempted to apply NULL function.");
        return NULL;
    }
    // <<< END NULL CHECK >>>

    fprintf(stderr, "[DEBUG C APPLY ENTRY] func=%p, args=%p\n", (void*)func, (void*)args); // <<< ADDED VERY FIRST LINE
    assert(args != NULL);
    assert(env != NULL);
    assert(arena != NULL);

    if (l0_is_primitive(func)) {
        L0_PrimitiveFunc prim_func = func->data.primitive.func;
        const char* prim_name = func->data.primitive.name ? func->data.primitive.name : "<unknown>";
        if (func->type != L0_TYPE_PRIMITIVE) {
             fprintf(stderr, "[FATAL ERROR] Expected primitive type in apply, but got type %d for func!\n", func->type);
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
             l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Primitive object type mismatch in apply.");
             return NULL;
        }
        if (prim_func == NULL) {
             fprintf(stderr, "[FATAL ERROR] Primitive function pointer for '%s' is NULL!\n", prim_name);
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
             l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Primitive function pointer is NULL.");
             return NULL;
        }
        return prim_func(args, env, arena);
    } else if (l0_is_closure(func)) {
        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Applying closure %p\n", (void*)func); // DEBUG ENTRY
        L0_Value* params = func->data.closure.params;
        L0_Value* body = func->data.closure.body;
        L0_Env* closure_env = func->data.closure.env;

        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Extending env %p\n", (void*)closure_env); // DEBUG BEFORE EXTEND
        L0_Env* call_env = l0_env_extend(closure_env);
        if (!call_env) return NULL;
        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Extended env created: %p\n", (void*)call_env); // DEBUG AFTER EXTEND

        // Bind parameters to arguments in the new environment
        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Binding parameters...\n"); // DEBUG BEFORE BINDING
        L0_Value* current_param_node = params;
        L0_Value* current_arg_node = args;
        while (l0_is_pair(current_param_node) && l0_is_pair(current_arg_node)) {
            L0_Value* param_sym = current_param_node->data.pair.car;
            L0_Value* arg_val = current_arg_node->data.pair.car;
            if (!l0_env_define(call_env, param_sym, arg_val)) {
                 if (l0_parser_error_status == L0_PARSE_OK) {
                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                     l0_parser_error_message = l0_arena_strdup(arena, "Failed to bind argument in function call environment.");
                 }
                 return NULL; // Return NULL if define fails
            }
            current_param_node = current_param_node->data.pair.cdr;
            current_arg_node = current_arg_node->data.pair.cdr;
        }
        // Check for argument count mismatch
        if (!l0_is_nil(current_param_node) || !l0_is_nil(current_arg_node)) {
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Function called with incorrect number of arguments.");
            return NULL;
        }
        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Binding complete. Evaluating body...\n"); // DEBUG BEFORE EVAL BODY
        L0_Value* result = l0_eval_sequence(body, call_env, arena);
        fprintf(stderr, "[DEBUG C APPLY CLOSURE] Body evaluation complete. Result=%p, Status=%d\n", (void*)result, l0_parser_error_status); // DEBUG AFTER EVAL BODY
        return result;
    } else {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[100];
        snprintf(err_buf, sizeof(err_buf), "Attempted to apply non-function value (type %d).", func->type);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return NULL;
    }
}

// Static variable to track recursion depth (for debugging stack overflow)
static int eval_list_depth = 0;

L0_Value* l0_eval_list(L0_Value* list, L0_Env* env, L0_Arena* arena) {
    eval_list_depth++;
    // DEBUG: Print entry and depth
    fprintf(stderr, "[DEBUG C eval_list] Enter depth %d, list type %d\n", eval_list_depth, list ? list->type : (L0_ValueType)-1); // Cast -1

    if (eval_list_depth > 1000) { // Limit recursion depth to prevent crash
        fprintf(stderr, "[FATAL ERROR] Maximum recursion depth exceeded in l0_eval_list!\n");
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Stack overflow suspected in argument list evaluation.");
        eval_list_depth--; // Decrement before returning
        return NULL;
    }


    if (l0_is_nil(list)) {
        L0_Value* nil_result = l0_make_nil(arena);
        // DEBUG: Print exit and return value
        fprintf(stderr, "[DEBUG C eval_list] Returning NIL at depth %d: result=%p\n", eval_list_depth, (void*)nil_result);
        eval_list_depth--;
        return nil_result;
    }
    if (!l0_is_pair(list)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Cannot evaluate list: input is not a pair or nil.");
        // DEBUG: Print exit (error)
        fprintf(stderr, "[DEBUG C eval_list] Returning NULL (not pair/nil error) at depth %d\n", eval_list_depth);
        eval_list_depth--;
        return NULL;
    }

    L0_Value* first_evaled = l0_eval(list->data.pair.car, env, arena);
    if (!first_evaled || l0_parser_error_status != L0_PARSE_OK) {
        // DEBUG: Print exit (eval car error)
        fprintf(stderr, "[DEBUG C eval_list] Returning NULL (eval car error) at depth %d\n", eval_list_depth);
        eval_list_depth--;
        return NULL;
    }

    // <<< ADD DEBUG BEFORE CDR EVAL >>>
    fprintf(stderr, "[DEBUG C eval_list] Before recursive call for CDR at depth %d. list->cdr type=%d\n", eval_list_depth, list->data.pair.cdr ? list->data.pair.cdr->type : (L0_ValueType)-1); // Cast -1

    L0_Value* rest_evaled = l0_eval_list(list->data.pair.cdr, env, arena); // Recursive call

    // <<< ADD DEBUG AFTER CDR EVAL >>>
    fprintf(stderr, "[DEBUG C eval_list] After recursive call for CDR at depth %d. rest_evaled=%p, status=%d\n", eval_list_depth, (void*)rest_evaled, l0_parser_error_status);


    if (!rest_evaled || l0_parser_error_status != L0_PARSE_OK) {
         // DEBUG: Print exit (eval cdr error)
        fprintf(stderr, "[DEBUG C eval_list] Returning NULL (eval cdr error) at depth %d. rest_evaled=%p\n", eval_list_depth, (void*)rest_evaled); // Added rest_evaled pointer
        eval_list_depth--;
        return NULL;
    }

    // <<< ADD DEBUG BEFORE MAKE_PAIR >>>
    fprintf(stderr, "[DEBUG C eval_list] Before l0_make_pair at depth %d. first_evaled=%p, rest_evaled=%p\n", eval_list_depth, (void*)first_evaled, (void*)rest_evaled);


    L0_Value* result = l0_make_pair(arena, first_evaled, rest_evaled);
    // DEBUG: Print exit (success) and return value
    fprintf(stderr, "[DEBUG C eval_list] Returning SUCCESS at depth %d: result=%p (car=%p, cdr=%p)\n", eval_list_depth, (void*)result, (void*)first_evaled, (void*)rest_evaled);
    eval_list_depth--;
    return result;
}


// Core Evaluation Function
L0_Value* l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena) {
    fprintf(stderr, "[DEBUG C EVAL ENTRY POINT] expr=%p\n", (void*)expr); // <<< ADDED VERY FIRST LINE
    eval_depth++;
    fprintf(stderr, "[DEBUG C eval ENTRY] Depth: %d, Expr Type: %d, Expr Ptr: %p\n", eval_depth, expr ? expr->type : (L0_ValueType)-1, (void*)expr); // Cast -1

    if (eval_depth > MAX_EVAL_DEPTH) {
        fprintf(stderr, "[FATAL ERROR] Maximum recursion depth (%d) exceeded in l0_eval!\n", MAX_EVAL_DEPTH);
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Stack overflow suspected in l0_eval.");
        eval_depth--; // Decrement before returning NULL
        return NULL; // Or maybe exit(-1) here? For now, return NULL.
    }


    assert(env != NULL);
    assert(arena != NULL);

    if (expr == NULL) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Attempted to evaluate NULL expression.");
        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (NULL expr)\n", eval_depth);
        eval_depth--;
        return NULL;
    }

    L0_Value* result = NULL; // Initialize result

    switch (expr->type) {
        // 1. Self-evaluating atoms
        case L0_TYPE_NIL:
        case L0_TYPE_BOOLEAN:
        case L0_TYPE_INTEGER:
        case L0_TYPE_FLOAT: // Added float
        case L0_TYPE_STRING:
            result = expr; // Self-evaluating
            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Self-Evaluating Type %d, Ptr: %p\n", eval_depth, expr->type, (void*)result);
            eval_depth--;
            return result;

        // 2. Symbol lookup
        case L0_TYPE_SYMBOL: {
            L0_Value* value = l0_env_lookup(env, expr);
            // <<< ADDED DEBUG >>>
            fprintf(stderr, "[DEBUG EVAL l0_eval SYMBOL] Looked up symbol '%s'. Result value=%p\n", expr->data.symbol, (void*)value);
            if (value == NULL) {
                char err_buf[100];
                snprintf(err_buf, sizeof(err_buf), "Unbound variable: %s", expr->data.symbol);
                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (Unbound variable: %s)\n", eval_depth, expr->data.symbol);
                eval_depth--;
                return NULL;
            }
            result = value;
            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Symbol Lookup '%s', Ptr: %p\n", eval_depth, expr->data.symbol, (void*)result);
            eval_depth--;
            return result;
        } // <<< END case L0_TYPE_SYMBOL

        // 3. List evaluation (Special Forms and Function Calls)
        case L0_TYPE_PAIR: { // <<< START case L0_TYPE_PAIR
            // <<< ADDED DETAILED EXPR PRINT >>>
            // Print the address of the pair expression being evaluated to help debug NULL apply.
            fprintf(stderr, "[DEBUG C eval PAIR START] Evaluating pair expression at address: %p\n", (void*)expr);
            // <<< END DETAILED EXPR PRINT >>>
            fprintf(stderr, "[DEBUG C eval PAIR ENTRY] expr=%p\n", (void*)expr); // DEBUG
            if (l0_is_nil(expr)) {
                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                 l0_parser_error_message = l0_arena_strdup(arena, "Cannot evaluate empty list '()'.");
                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (Empty list)\n", eval_depth);
                 eval_depth--;
                 return NULL;
            }
            fprintf(stderr, "[DEBUG C eval PAIR] Accessing car/cdr...\n"); // DEBUG
            L0_Value* op_expr = expr->data.pair.car;
            L0_Value* args_expr = expr->data.pair.cdr;
            fprintf(stderr, "[DEBUG C eval PAIR] Got op_expr=%p, args_expr=%p\n", (void*)op_expr, (void*)args_expr); // Existing debug

            // <<< ADDED OP_EXPR TYPE CHECK DEBUG >>>
            if (op_expr) {
                fprintf(stderr, "[DEBUG C eval PAIR TYPE CHECK] op_expr type: %d (SYMBOL=%d, PAIR=%d)\n", op_expr->type, L0_TYPE_SYMBOL, L0_TYPE_PAIR);
            } else {
                fprintf(stderr, "[DEBUG C eval PAIR TYPE CHECK] op_expr is NULL!\n");
            }
            // <<< END ADDED OP_EXPR TYPE CHECK DEBUG >>>

            // <<< ADDED IMMEDIATE PRE-CHECK DEBUG >>>
            if (op_expr) {
                 fprintf(stderr, "[DEBUG C eval PAIR IMMEDIATE PRE-CHECK] op_expr type before l0_is_symbol check: %d (SYMBOL=%d)\n", op_expr->type, L0_TYPE_SYMBOL);
                 if (op_expr->type == L0_TYPE_SYMBOL) {
                     fprintf(stderr, "[DEBUG C eval PAIR IMMEDIATE PRE-CHECK] op_expr IS symbol. Value: '%s'\n", op_expr->data.symbol ? op_expr->data.symbol : "<NULL>");
                 }
            } else {
                 fprintf(stderr, "[DEBUG C eval PAIR IMMEDIATE PRE-CHECK] op_expr is NULL before l0_is_symbol check!\n");
            }
            // <<< END ADDED IMMEDIATE PRE-CHECK DEBUG >>>


            // <<< NEW DEBUG >>>
            if (op_expr) {
                fprintf(stderr, "[DEBUG C eval PAIR PRE-CHECK] op_expr type: %d\n", op_expr->type);
                if (l0_is_symbol(op_expr)) {
                    fprintf(stderr, "[DEBUG C eval PAIR PRE-CHECK] op_expr IS symbol. Symbol value: '%s'\n", op_expr->data.symbol ? op_expr->data.symbol : "<NULL>");
                } else {
                    fprintf(stderr, "[DEBUG C eval PAIR PRE-CHECK] op_expr is NOT symbol.\n");
                }
            } else {
                fprintf(stderr, "[DEBUG C eval PAIR PRE-CHECK] op_expr is NULL!\n");
            }
            // <<< END NEW DEBUG >>>

            // 3.1 Check if operator is a symbol
            if (l0_is_symbol(op_expr)) { // <<< START if (l0_is_symbol(op_expr))
                const char* op_sym = op_expr->data.symbol;

                // <<< ADDED NULL CHECK FOR op_sym >>>
                if (op_sym == NULL) {
                    fprintf(stderr, "[FATAL ERROR] op_expr is L0_TYPE_SYMBOL but op_expr->data.symbol is NULL!\n");
                    l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                    l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Symbol value is NULL.");
                    eval_depth--;
                    return NULL;
                }
                // <<< END ADDED NULL CHECK >>>

                // <<< NEW DEBUGGING >>>
                fprintf(stderr, "[DEBUG C eval PAIR PRE-GET-SF] op_sym pointer: %p, value: '%s'\n", (void*)op_sym, op_sym);
                // <<< END NEW DEBUGGING >>>

                // 3.1.1 Check for Special Forms using the helper function
                fprintf(stderr, "[DEBUG C eval PAIR] Checking special form for symbol: '%s' (addr: %p)\n", op_sym, (void*)op_sym); // Added addr
                L0_SpecialFormType sf_type = get_special_form_type(op_sym);
                fprintf(stderr, "[DEBUG C eval PAIR] Result from get_special_form_type for '%s': %d (SF_NONE=%d, SF_QUOTE=%d, SF_QUASIQUOTE=%d)\n", op_sym, sf_type, SF_NONE, SF_QUOTE, SF_QUASIQUOTE); // Combined debug

                switch (sf_type) {
                    case SF_QUOTE: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'quote'\n"); // Specific message
                        if (!l0_is_pair(args_expr) || !l0_is_nil(args_expr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'quote' requires exactly one argument."); // Specific message
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (quote arity error)\n", eval_depth); // Specific message
                            eval_depth--;
                            return NULL;
                        }
                        result = args_expr->data.pair.car; // Return the argument unevaluated
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Quote result, Ptr: %p\n", eval_depth, (void*)result); // Specific message
                        eval_depth--;
                        return result;
                    }
                    case SF_QUASIQUOTE: { // New case for quasiquote
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'quasiquote'\n");
                        if (!l0_is_pair(args_expr) || !l0_is_nil(args_expr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'quasiquote' requires exactly one argument.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (quasiquote arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        // TODO: Implement proper quasiquote expansion later.
                        // Call the expansion helper function
                        L0_Value* template_expr = args_expr->data.pair.car;
                        result = l0_expand_quasiquote(template_expr, env, arena, 1); // Start at depth 1
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Quasiquote expansion result, Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result; // Return the expanded result (or NULL on error)
                    }
                    case SF_IF: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'if'\n");
                        L0_Value* condition_expr = NULL;
                        L0_Value* true_expr = NULL;
                        L0_Value* false_expr = NULL;
                        // Arity check: (if condition true-expr [false-expr])
                        if (l0_is_pair(args_expr) && l0_is_pair(args_expr->data.pair.cdr)) {
                            condition_expr = args_expr->data.pair.car;
                            L0_Value* true_branch_pair = args_expr->data.pair.cdr;
                            true_expr = true_branch_pair->data.pair.car;
                            L0_Value* rest_args = true_branch_pair->data.pair.cdr;
                            if (l0_is_nil(rest_args)) { // Two arguments: (if cond true)
                                false_expr = l0_make_nil(arena); // Default false branch is nil
                                if (!false_expr) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (if make_nil failed)\n", eval_depth);
                                    eval_depth--;
                                    return NULL;
                                }
                            } else if (l0_is_pair(rest_args) && l0_is_nil(rest_args->data.pair.cdr)) { // Three arguments: (if cond true false)
                                false_expr = rest_args->data.pair.car;
                            }
                        }
                        // Check if parsing arguments succeeded
                        if (condition_expr && true_expr && false_expr) {
                            L0_Value* condition_result = l0_eval(condition_expr, env, arena);
                            if (!condition_result || l0_parser_error_status != L0_PARSE_OK) {
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (if condition eval error)\n", eval_depth);
                                eval_depth--;
                                return NULL; // Propagate error
                            }
                            // Lisp truthiness: everything is true except #f
                            bool is_true = !(l0_is_boolean(condition_result) && !condition_result->data.boolean);
                            eval_depth--; // Decrement depth *before* tail call
                            fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval for IF %s branch\n", eval_depth + 1, is_true ? "true" : "false");
                            if (is_true) {
                                return l0_eval(true_expr, env, arena); // Tail call
                            } else {
                                return l0_eval(false_expr, env, arena); // Tail call
                            }
                        } else {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'if' requires 2 or 3 arguments: (if condition true-expr [false-expr]).");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (if arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                    } // end case SF_IF
                    case SF_LAMBDA: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'lambda'\n");
                        // Arity check: (lambda (params...) body...)
                        if (!l0_is_pair(args_expr) || !l0_is_pair(args_expr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'lambda' requires parameters list and at least one body expression: (lambda (params...) body...).");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (lambda arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        L0_Value* params = args_expr->data.pair.car;
                        L0_Value* body = args_expr->data.pair.cdr;
                        // Validate params list structure and content
                        L0_Value* current_param = params;
                        while(l0_is_pair(current_param)) {
                            if (!l0_is_symbol(current_param->data.pair.car)) {
                                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                 l0_parser_error_message = l0_arena_strdup(arena, "Lambda parameters must be symbols.");
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (lambda param not symbol)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                            }
                            current_param = current_param->data.pair.cdr;
                        }
                        if (!l0_is_nil(current_param)) { // Must end in nil
                             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                             l0_parser_error_message = l0_arena_strdup(arena, "Lambda parameters list is not a proper list.");
                             fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (lambda params not proper list)\n", eval_depth);
                             eval_depth--;
                             return NULL;
                        }
                        // Create and return the closure
                        result = l0_make_closure(arena, params, body, env);
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Lambda Closure, Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result;
                    } // end case SF_LAMBDA
                    case SF_DEFINE: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'define'\n");
                        // Arity check: (define symbol value-expr)
                         if (!l0_is_pair(args_expr) || !l0_is_pair(args_expr->data.pair.cdr) || !l0_is_nil(args_expr->data.pair.cdr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'define' requires exactly two arguments: (define symbol value-expr).");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        L0_Value* target = args_expr->data.pair.car;
                        L0_Value* rest_args = args_expr->data.pair.cdr;

                        if (l0_is_symbol(target)) {
                            // --- Basic Define: (define symbol value-expr) ---
                            fprintf(stderr, "[DEBUG C define] Basic form (define symbol ...)\n");
                            // Arity check for basic define
                            if (!l0_is_pair(rest_args) || !l0_is_nil(rest_args->data.pair.cdr)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Basic 'define' requires exactly two arguments: (define symbol value-expr).");
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define basic arity error)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }
                            L0_Value* symbol = target;
                            L0_Value* value_expr = rest_args->data.pair.car;

                            // Evaluate the value expression
                            L0_Value* value_result = l0_eval(value_expr, env, arena);
                            if (!value_result || l0_parser_error_status != L0_PARSE_OK) {
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define value eval error)\n", eval_depth);
                                eval_depth--;
                                return NULL; // Propagate error
                            }
                            // Define in the current environment
                            if (!l0_env_define(env, symbol, value_result)) {
                                if (l0_parser_error_status == L0_PARSE_OK) {
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, "Failed to define variable in environment.");
                                }
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define env_define failed)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }
                        } else if (l0_is_pair(target)) {
                            // --- Function Shorthand: (define (name params...) body...) ---
                            fprintf(stderr, "[DEBUG C define] Function shorthand form (define (name ...) ...)\n");
                            // Extract name, params, body
                            if (l0_is_nil(target)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Function definition shorthand requires a non-empty list for (name params...).");
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand target empty)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }
                            L0_Value* func_name_sym = target->data.pair.car;
                            L0_Value* params = target->data.pair.cdr; // Can be nil for zero params
                            L0_Value* body = rest_args; // The rest of the define form is the body

                            // Validate func_name is a symbol
                            if (!l0_is_symbol(func_name_sym)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Function name in definition shorthand must be a symbol.");
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand name not symbol)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }
                            // Validate params is a proper list of symbols (or nil)
                            L0_Value* current_param = params;
                            while(l0_is_pair(current_param)) {
                                if (!l0_is_symbol(current_param->data.pair.car)) {
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, "Function definition parameters must be symbols.");
                                     fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand param not symbol)\n", eval_depth);
                                     eval_depth--;
                                     return NULL;
                                }
                                current_param = current_param->data.pair.cdr;
                            }
                            if (!l0_is_nil(current_param)) { // Must end in nil
                                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                 l0_parser_error_message = l0_arena_strdup(arena, "Function definition parameters list is not a proper list.");
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand params not proper list)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                            }
                            // Validate body is not empty
                            if (l0_is_nil(body)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Function definition requires at least one body expression.");
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand empty body)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }

                            // Desugar into (define func-name (lambda (params...) body...))
                            // Create the lambda closure
                            L0_Value* lambda_closure = l0_make_closure(arena, params, body, env);
                             if (!lambda_closure) {
                                 if (l0_parser_error_status == L0_PARSE_OK) {
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, "Failed to create closure for function definition.");
                                 }
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand closure creation failed)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                             }

                            // Define the function name with the closure
                            if (!l0_env_define(env, func_name_sym, lambda_closure)) {
                                if (l0_parser_error_status == L0_PARSE_OK) {
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, "Failed to define function in environment.");
                                }
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define shorthand env_define failed)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }
                        } else {
                            // First argument is neither symbol nor pair
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "First argument to 'define' must be a symbol or a list for function definition.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (define arg1 not symbol or pair)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }

                        // Define returns an unspecified value (we use nil)
                        result = l0_make_nil(arena);
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Define result (NIL), Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result;
                    } // end case SF_DEFINE
                    case SF_SET: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'set!'\n");
                        // Arity check: (set! symbol value-expr)
                        if (!l0_is_pair(args_expr) || !l0_is_pair(args_expr->data.pair.cdr) || !l0_is_nil(args_expr->data.pair.cdr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'set!' requires exactly two arguments: (set! symbol value-expr).");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (set! arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        L0_Value* symbol = args_expr->data.pair.car;
                        L0_Value* value_expr = args_expr->data.pair.cdr->data.pair.car;
                        // Type check symbol
                        if (!l0_is_symbol(symbol)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "First argument to 'set!' must be a symbol.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (set! arg1 not symbol)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        // Evaluate the value expression
                        L0_Value* value_result = l0_eval(value_expr, env, arena);
                        if (!value_result || l0_parser_error_status != L0_PARSE_OK) {
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (set! value eval error)\n", eval_depth);
                            eval_depth--;
                            return NULL; // Propagate error
                        }
                        // Set in the environment (searches up the chain)
                        if (!l0_env_set(env, symbol, value_result)) {
                            // l0_env_set sets error status/message on failure (unbound variable)
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (set! env_set failed: %s)\n", eval_depth, l0_parser_error_message ? l0_parser_error_message : "Unknown error");
                            eval_depth--;
                            return NULL;
                        }
                        // Set! returns an unspecified value (we use nil)
                        result = l0_make_nil(arena);
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Set! result (NIL), Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result;
                    } // end case SF_SET
                    case SF_LET: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'let'\n");
                        // Arity check: (let (bindings...) body...)
                        if (!l0_is_pair(args_expr) || !l0_is_pair(args_expr->data.pair.cdr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'let' requires bindings list and at least one body expression: (let ((var val)...) body...).");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        L0_Value* bindings = args_expr->data.pair.car;
                        L0_Value* body = args_expr->data.pair.cdr;

                        // Create a new environment extending the current one
                        L0_Env* let_env = l0_env_extend(env);
                        if (!let_env) {
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let env_extend failed)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }

                        // Evaluate values and define in the *new* environment
                        L0_Value* current_binding = bindings;
                        while(l0_is_pair(current_binding)) {
                            L0_Value* binding_pair = current_binding->data.pair.car;
                            // Validate binding format: (symbol value-expr)
                            if (!l0_is_pair(binding_pair) || !l0_is_pair(binding_pair->data.pair.cdr) || !l0_is_nil(binding_pair->data.pair.cdr->data.pair.cdr)) {
                                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                 l0_parser_error_message = l0_arena_strdup(arena, "Let binding must be a list of two elements: (symbol value-expr).");
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let binding format error)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                            }
                            L0_Value* var_symbol = binding_pair->data.pair.car;
                            L0_Value* val_expr = binding_pair->data.pair.cdr->data.pair.car;
                            // Validate variable is a symbol
                             if (!l0_is_symbol(var_symbol)) {
                                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                 l0_parser_error_message = l0_arena_strdup(arena, "Let binding variable must be a symbol.");
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let var not symbol)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                             }
                            // Evaluate value in the *original* environment
                            L0_Value* evaluated_val = l0_eval(val_expr, env, arena);
                            if (!evaluated_val || l0_parser_error_status != L0_PARSE_OK) {
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let value eval error)\n", eval_depth);
                                eval_depth--;
                                return NULL; // Propagate error
                            }
                            // Define the variable in the *new* let environment
                             if (!l0_env_define(let_env, var_symbol, evaluated_val)) {
                                 if (l0_parser_error_status == L0_PARSE_OK) {
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, "Failed to define variable in let environment.");
                                 }
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let env_define failed)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                             }
                            current_binding = current_binding->data.pair.cdr;
                        }
                        // Check if bindings list was proper
                         if (!l0_is_nil(current_binding)) {
                             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                             l0_parser_error_message = l0_arena_strdup(arena, "Let bindings list is not a proper list.");
                             fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (let bindings not proper list)\n", eval_depth);
                             eval_depth--;
                             return NULL;
                         }

                        // Evaluate the body sequence in the new environment
                         eval_depth--; // Decrement depth *before* tail call
                         fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval_sequence for LET body\n", eval_depth + 1);
                        return l0_eval_sequence(body, let_env, arena); // Tail call
                    } // end case SF_LET
                    case SF_DEFMACRO: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'defmacro'\n");
                        // Arity check: (defmacro name (params...) body...)
                        if (!l0_is_pair(args_expr) || // name
                            !l0_is_pair(args_expr->data.pair.cdr) || // params list
                            !l0_is_pair(args_expr->data.pair.cdr->data.pair.cdr)) { // body (at least one expr)
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Special form 'defmacro' requires name, parameters list, and at least one body expression.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro arity error)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        L0_Value* macro_name_sym = args_expr->data.pair.car;
                        L0_Value* params = args_expr->data.pair.cdr->data.pair.car;
                        L0_Value* body = args_expr->data.pair.cdr->data.pair.cdr;

                        // Validate name is a symbol
                        if (!l0_is_symbol(macro_name_sym)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "First argument to 'defmacro' (name) must be a symbol.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro name not symbol)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        // Validate params list structure and content
                        L0_Value* current_param = params;
                        while(l0_is_pair(current_param)) {
                            if (!l0_is_symbol(current_param->data.pair.car)) {
                                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                 l0_parser_error_message = l0_arena_strdup(arena, "Defmacro parameters must be symbols.");
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro param not symbol)\n", eval_depth);
                                 eval_depth--;
                                 return NULL;
                            }
                            current_param = current_param->data.pair.cdr;
                        }
                        if (!l0_is_nil(current_param)) { // Must end in nil
                             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                             l0_parser_error_message = l0_arena_strdup(arena, "Defmacro parameters list is not a proper list.");
                             fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro params not proper list)\n", eval_depth);
                             eval_depth--;
                             return NULL;
                        }

                        // Create the transformer closure (captures current env)
                        fprintf(stderr, "[DEBUG C defmacro] Creating transformer closure for '%s'\n", macro_name_sym->data.symbol);
                        L0_Value* transformer_closure = l0_make_closure(arena, params, body, env);
                        if (!transformer_closure) {
                            if (l0_parser_error_status == L0_PARSE_OK) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Failed to create closure for macro transformer.");
                            }
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro closure creation failed)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        fprintf(stderr, "[DEBUG C defmacro] Transformer closure created: %p\n", (void*)transformer_closure);

                        // Add the macro to the *macro-table* (assumed to be defined in env)
                        fprintf(stderr, "[DEBUG C defmacro] Updating *macro-table*\n");
                        L0_Value* macro_table_sym = l0_make_symbol(arena, "*macro-table*");
                        if (!macro_table_sym) return NULL; // Internal error
                        L0_Value* current_macro_table = l0_env_lookup(env, macro_table_sym);
                        if (!current_macro_table) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Global variable '*macro-table*' is not defined.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro *macro-table* not found)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        fprintf(stderr, "[DEBUG C defmacro] Current *macro-table* value: %p\n", (void*)current_macro_table);

                        // Create the new entry (name . transformer)
                        L0_Value* new_entry = l0_make_pair(arena, macro_name_sym, transformer_closure);
                        if (!new_entry) return NULL; // Internal error
                        fprintf(stderr, "[DEBUG C defmacro] New macro entry created: %p ((%s . %p))\n", (void*)new_entry, macro_name_sym->data.symbol, (void*)transformer_closure);

                        // Prepend the new entry to the existing table (association list)
                        L0_Value* new_macro_table = l0_make_pair(arena, new_entry, current_macro_table);
                        if (!new_macro_table) return NULL; // Internal error
                        fprintf(stderr, "[DEBUG C defmacro] New *macro-table* list created: %p\n", (void*)new_macro_table);

                        // Update the binding in the environment
                        if (!l0_env_set(env, macro_table_sym, new_macro_table)) {
                            // l0_env_set should handle unbound *macro-table* case, but check anyway
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "Failed to update '*macro-table*' binding.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (defmacro set! *macro-table* failed)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        fprintf(stderr, "[DEBUG C defmacro] *macro-table* updated successfully.\n");

                        // Defmacro returns an unspecified value (we use nil)
                        result = l0_make_nil(arena);
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Defmacro result (NIL), Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result;
                    } // end case SF_DEFMACRO
                    case SF_AND: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'and'\n");
                        // Handle empty case: (and) -> #t
                        if (l0_is_nil(args_expr)) {
                            result = l0_make_boolean(arena, true);
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning And result (true for empty), Ptr: %p\n", eval_depth, (void*)result);
                            eval_depth--;
                            return result;
                        }
                        // Ensure args form a proper list
                        if (!l0_is_list(args_expr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "'and' arguments must form a proper list.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (and args not proper list)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        // Evaluate arguments sequentially
                        L0_Value* last_result = l0_make_boolean(arena, true); // Placeholder for loop logic
                        L0_Value* current_arg_node = args_expr;
                        while (l0_is_pair(current_arg_node)) {
                            L0_Value* arg_expr = current_arg_node->data.pair.car;
                            bool is_last_arg = l0_is_nil(current_arg_node->data.pair.cdr);

                            if (is_last_arg) {
                                // Tail call optimization for the last argument
                                eval_depth--;
                                fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval for AND last argument\n", eval_depth + 1);
                                return l0_eval(arg_expr, env, arena);
                            } else {
                                // Evaluate non-last argument
                                last_result = l0_eval(arg_expr, env, arena);
                                if (!last_result || l0_parser_error_status != L0_PARSE_OK) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (and arg eval error)\n", eval_depth);
                                    eval_depth--;
                                    return NULL; // Propagate error
                                }
                                // Check for short-circuiting (#f)
                                if (l0_is_boolean(last_result) && !last_result->data.boolean) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning And short-circuit (#f), Ptr: %p\n", eval_depth, (void*)last_result);
                                    eval_depth--;
                                    return last_result; // Short-circuit
                                }
                            }
                            current_arg_node = current_arg_node->data.pair.cdr;
                        }
                        // Should not be reached if list is proper and non-empty due to tail call
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning And final result (unexpected path?), Ptr: %p\n", eval_depth, (void*)last_result);
                        eval_depth--;
                        return last_result;
                    } // end case SF_AND
                    case SF_OR: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'or'\n");
                        // Handle empty case: (or) -> #f
                        if (l0_is_nil(args_expr)) {
                            result = l0_make_boolean(arena, false);
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Or result (false for empty), Ptr: %p\n", eval_depth, (void*)result);
                            eval_depth--;
                            return result;
                        }
                        // Ensure args form a proper list
                        if (!l0_is_list(args_expr)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "'or' arguments must form a proper list.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (or args not proper list)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }
                        // Evaluate arguments sequentially
                        L0_Value* last_result = l0_make_boolean(arena, false); // Placeholder for loop logic
                        L0_Value* current_arg_node = args_expr;
                        while (l0_is_pair(current_arg_node)) {
                            L0_Value* arg_expr = current_arg_node->data.pair.car;
                            bool is_last_arg = l0_is_nil(current_arg_node->data.pair.cdr);

                            if (is_last_arg) {
                                // Tail call optimization for the last argument
                                eval_depth--;
                                fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval for OR last argument\n", eval_depth + 1);
                                return l0_eval(arg_expr, env, arena);
                            } else {
                                // Evaluate non-last argument
                                last_result = l0_eval(arg_expr, env, arena);
                                if (!last_result || l0_parser_error_status != L0_PARSE_OK) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (or arg eval error)\n", eval_depth);
                                    eval_depth--;
                                    return NULL; // Propagate error
                                }
                                // Check for short-circuiting (true value)
                                bool is_true = !(l0_is_boolean(last_result) && !last_result->data.boolean);
                                if (is_true) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Or short-circuit (true value), Ptr: %p\n", eval_depth, (void*)last_result);
                                    eval_depth--;
                                    return last_result; // Short-circuit
                                }
                            }
                            current_arg_node = current_arg_node->data.pair.cdr;
                        }
                        // Should not be reached if list is proper and non-empty due to tail call
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Or final result (unexpected path?), Ptr: %p\n", eval_depth, (void*)last_result);
                        eval_depth--;
                        return last_result; // Should be #f if loop finished normally
                    } // end case SF_OR
                    case SF_BEGIN: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'begin'\n");
                        // Evaluate sequence of expressions, return the last result
                        eval_depth--; // Decrement depth *before* tail call
                        fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval_sequence for BEGIN body\n", eval_depth + 1);
                        return l0_eval_sequence(args_expr, env, arena); // Tail call
                    } // end case SF_BEGIN
                    case SF_COND: { // Added case for cond
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form: 'cond'\n");
                        L0_Value* current_clause_node = args_expr;
                        while (l0_is_pair(current_clause_node)) {
                            L0_Value* clause = current_clause_node->data.pair.car;
                            if (!l0_is_pair(clause)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "'cond' clause must be a list.");
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (cond clause not list)\n", eval_depth);
                                eval_depth--;
                                return NULL;
                            }

                            L0_Value* test_expr = clause->data.pair.car;
                            L0_Value* body_exprs = clause->data.pair.cdr;

                            bool is_else_clause = false;
                            if (l0_is_symbol(test_expr) && strcmp(test_expr->data.symbol, "else") == 0) {
                                is_else_clause = true;
                                // Ensure 'else' is the last clause
                                if (!l0_is_nil(current_clause_node->data.pair.cdr)) {
                                    l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                    l0_parser_error_message = l0_arena_strdup(arena, "'else' clause must be the last clause in 'cond'.");
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (cond else not last)\n", eval_depth);
                                    eval_depth--;
                                    return NULL;
                                }
                            }

                            L0_Value* test_result = NULL;
                            if (!is_else_clause) {
                                test_result = l0_eval(test_expr, env, arena);
                                if (!test_result || l0_parser_error_status != L0_PARSE_OK) {
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (cond test eval error)\n", eval_depth);
                                    eval_depth--;
                                    return NULL; // Propagate error
                                }
                            }

                            // Check if test is true (not #f) or if it's the else clause
                            bool condition_met = is_else_clause || !(l0_is_boolean(test_result) && !test_result->data.boolean);

                            if (condition_met) {
                                // If body is empty, result is the test result (or #t for else)
                                if (l0_is_nil(body_exprs)) {
                                    result = is_else_clause ? l0_make_boolean(arena, true) : test_result;
                                    fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Cond result (empty body), Ptr: %p\n", eval_depth, (void*)result);
                                    eval_depth--;
                                    return result;
                                }
                                // Evaluate the body sequence (tail call)
                                eval_depth--;
                                fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval_sequence for COND body\n", eval_depth + 1);
                                return l0_eval_sequence(body_exprs, env, arena);
                            }

                            // Move to the next clause
                            current_clause_node = current_clause_node->data.pair.cdr;
                        }

                        // Check if clauses list was proper
                        if (!l0_is_nil(current_clause_node)) {
                            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                            l0_parser_error_message = l0_arena_strdup(arena, "'cond' clauses must form a proper list.");
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (cond clauses not proper list)\n", eval_depth);
                            eval_depth--;
                            return NULL;
                        }

                        // No clause matched, and no else clause
                        result = l0_make_nil(arena); // Return nil (unspecified value)
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Cond result (no match), Ptr: %p\n", eval_depth, (void*)result);
                        eval_depth--;
                        return result;
                    } // end case SF_COND
                    case SF_UNQUOTE: {
                        fprintf(stderr, "[DEBUG C eval PAIR] Special Form Error: '%s'\n", op_sym);
                        // unquote/unquote-splicing are only valid inside quasiquote expansion
                        char err_buf[100];
                        snprintf(err_buf, sizeof(err_buf), "Runtime Error: '%s' cannot appear outside of a quasiquote.", op_sym);
                        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                        fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (%s in eval)\n", eval_depth, op_sym);
                        eval_depth--;
                        return NULL;
                    } // end case SF_UNQUOTE

                    case SF_NONE: { // Not a special form
                        fprintf(stderr, "[DEBUG C eval PAIR] '%s' is not a special form. Checking macro/function...\n", op_sym);

                        // 3.1.2 Check if it's a macro
                        L0_Value* macro_table_sym = l0_make_symbol(arena, "*macro-table*");
                        if (!macro_table_sym) return NULL; // Internal error
                        L0_Value* current_macro_table = l0_env_lookup(env, macro_table_sym);
                        L0_Value* transformer_closure = NULL;

                        // Check if *macro-table* exists and is a list
                        if (current_macro_table && l0_is_list(current_macro_table)) {
                            // Iterate through the macro table (association list)
                            L0_Value* current_entry_node = current_macro_table;
                            while(l0_is_pair(current_entry_node)) {
                                L0_Value* entry_pair = current_entry_node->data.pair.car;
                                if (l0_is_pair(entry_pair)) {
                                    L0_Value* macro_name = entry_pair->data.pair.car;
                                    // Compare the operator symbol with the macro name
                                    if (l0_is_symbol(macro_name) && strcmp(op_sym, macro_name->data.symbol) == 0) {
                                        transformer_closure = entry_pair->data.pair.cdr;
                                        break; // Found the macro
                                    }
                                }
                                current_entry_node = current_entry_node->data.pair.cdr;
                            }
                        } else if (!current_macro_table) {
                             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                             l0_parser_error_message = l0_arena_strdup(arena, "Macro check failed: Global variable '*macro-table*' not found.");
                             fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (macro check: *macro-table* not found)\n", eval_depth);
                             eval_depth--;
                             return NULL;
                        } else { // *macro-table* exists but is not a list
                             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                             l0_parser_error_message = l0_arena_strdup(arena, "Macro check failed: Global variable '*macro-table*' is not a list.");
                             fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (macro check: *macro-table* not list)\n", eval_depth);
                             eval_depth--;
                             return NULL;
                        }


                        if (transformer_closure) {
                            // --- Macro Expansion ---
                            fprintf(stderr, "[DEBUG C eval MACRO EXPANSION] '%s' is a macro. Expanding...\n", op_sym);

                            // <<< ADDED EXPLICIT NULL CHECK >>>
                            if (transformer_closure == NULL) {
                                fprintf(stderr, "[FATAL ERROR] Found macro '%s' but transformer_closure is NULL!\n", op_sym);
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Macro transformer is NULL.");
                                eval_depth--;
                                return NULL;
                            }
                            // <<< END ADDED EXPLICIT NULL CHECK >>>

                            if (!l0_is_closure(transformer_closure)) {
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                char err_buf[150];
                                snprintf(err_buf, sizeof(err_buf), "Macro expansion error: Transformer for '%s' is not a closure (type %d).", op_sym, transformer_closure->type);
                                l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (transformer not closure, type %d)\n", eval_depth, transformer_closure->type);
                                eval_depth--;
                                return NULL;
                            }
                            fprintf(stderr, "[DEBUG C eval MACRO EXPANSION] Got transformer closure: %p\n", (void*)transformer_closure);
                            fprintf(stderr, "[DEBUG C eval MACRO EXPANSION] Applying transformer to args: %p\n", (void*)args_expr);
                            // Apply the transformer closure to the arguments *unevaluated*
                            // <<< ADDED PRE-APPLY DEBUG (MACRO) >>>
                            fprintf(stderr, "[DEBUG C eval PRE-APPLY MACRO] Calling l0_apply with func (transformer_closure) = %p, args (args_expr) = %p\n", (void*)transformer_closure, (void*)args_expr);
                            // <<< END ADDED PRE-APPLY DEBUG (MACRO) >>>
                            L0_Value* expanded_code = l0_apply(transformer_closure, args_expr, env, arena); // Apply in current env
                            if (!expanded_code || l0_parser_error_status != L0_PARSE_OK) {
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (macro transformer application failed)\n", eval_depth);
                                // Error message should be set by l0_apply or the transformer code
                                eval_depth--;
                                return NULL;
                            }
                            fprintf(stderr, "[DEBUG C eval MACRO EXPANSION] Macro expanded to: %p\n", (void*)expanded_code);
                            // Evaluate the expanded code in the current environment (tail call)
                            eval_depth--; // Decrement depth *before* tail call
                            fprintf(stderr, "[DEBUG C eval TAIL CALL] Depth: %d, Calling eval for MACRO EXPANDED code\n", eval_depth + 1);
                            return l0_eval(expanded_code, env, arena); // Tail call
                        } else {
                            // --- Function Call (Operator is Symbol) ---
                            fprintf(stderr, "[DEBUG C eval FUNC CALL] '%s' is NOT special/macro. Looking up function...\n", op_sym);
                            fprintf(stderr, "[DEBUG C eval FUNC CALL PRE-LOOKUP] About to call l0_env_lookup for op_expr: %p (symbol: '%s') in env: %p\n", (void*)op_expr, op_sym, (void*)env); // <<< ADDED DEBUG
                            L0_Value* op_value = l0_env_lookup(env, op_expr); // Lookup the symbol in the environment
                            fprintf(stderr, "[DEBUG C eval FUNC CALL POST-LOOKUP] Lookup finished. op_value = %p.\n", (void*)op_value);
                            fprintf(stderr, "[DEBUG C eval FUNC CALL PRE-NULL-CHECK] Checking if op_value (%p) is NULL.\n", (void*)op_value); // <<< ADDED DEBUG
                            if (op_value == NULL) {
                                 if (l0_parser_error_status == L0_PARSE_OK) { // Check if lookup set an error
                                     char err_buf[100];
                                     snprintf(err_buf, sizeof(err_buf), "Unbound function/variable in operator position: %s", op_sym);
                                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                     l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                                 }
                                 fprintf(stderr, "[DEBUG C eval FUNC CALL NULL CHECK] op_value IS NULL. Preparing to return error.\n"); // <<< ADDED DEBUG
                                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (Unbound function/variable: %s)\n", eval_depth, op_sym);
                                 eval_depth--;
                                 return NULL;
                             }

                            // Check if the looked-up value is applicable (primitive or closure)
                            if (!l0_is_primitive(op_value) && !l0_is_closure(op_value)) {
                                char err_buf[150];
                                snprintf(err_buf, sizeof(err_buf), "Attempted to apply non-function value (type %d) obtained from symbol '%s'.", op_value->type, op_sym);
                                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                                l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (apply non-function - symbol op, type %d)\n", eval_depth, op_value->type);
                                eval_depth--;
                                return NULL;
                             }

                            // Evaluate arguments
                            fprintf(stderr, "[DEBUG C eval] Evaluating arguments for '%s'...\n", op_sym);
                            L0_Value* evaluated_args = l0_eval_list(args_expr, env, arena);
                            fprintf(stderr, "[DEBUG C eval] Arguments evaluated. evaluated_args=%p, status=%d\n", (void*)evaluated_args, l0_parser_error_status);
                            if (!evaluated_args || l0_parser_error_status != L0_PARSE_OK) {
                                fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (args eval error or status %d)\n", eval_depth, l0_parser_error_status);
                                eval_depth--;
                                return NULL; // Propagate error from arg evaluation
                            }

                            // Apply the function/primitive to the evaluated arguments
                            // <<< ADDED PRE-APPLY DEBUG (SYMBOL OP) >>>
                            fprintf(stderr, "[DEBUG C eval PRE-APPLY SYMBOL OP] Calling l0_apply with func (op_value) = %p, args (evaluated_args) = %p\n", (void*)op_value, (void*)evaluated_args);
                            // <<< END ADDED PRE-APPLY DEBUG (SYMBOL OP) >>>
                            L0_Value* apply_result = l0_apply(op_value, evaluated_args, env, arena);
                            fprintf(stderr, "[DEBUG C eval] Returned from apply (symbol op). Result=%p, Status=%d - Depth %d\n", (void*)apply_result, l0_parser_error_status, eval_depth);
                            eval_depth--; // Decrement depth after apply returns
                            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Apply result (symbol op), Ptr: %p\n", eval_depth, (void*)apply_result);
                            return apply_result; // Return result (or NULL if apply failed)
                        } // End Function Call (Symbol Op)
                    } // end case SF_NONE (Not Special Form)
                } // <<< END switch (sf_type)
            } // <<< END if (l0_is_symbol(op_expr))
            else { // <<< START else (op_expr is not a symbol)
                 // --- Function Call (Operator is Expression) ---
                 fprintf(stderr, "[DEBUG C eval PAIR] Operator is NOT a symbol. Evaluating operator expression...\n");
                 L0_Value* evaluated_op = l0_eval(op_expr, env, arena); // Evaluate the operator expression first
                 if (evaluated_op == NULL || l0_parser_error_status != L0_PARSE_OK) {
                     fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (op eval returned NULL or error - non-symbol op)\n", eval_depth);
                     // Error message should be set by the recursive l0_eval call
                     eval_depth--;
                     return NULL; // Propagate error
                 }

                 // Check if the evaluated operator is applicable
                 if (!l0_is_primitive(evaluated_op) && !l0_is_closure(evaluated_op)) {
                     char err_buf[150];
                     snprintf(err_buf, sizeof(err_buf), "Attempted to apply non-function value (type %d) obtained from evaluating operator expression.", evaluated_op->type);
                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                     l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                     fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (apply non-function - non-symbol op, type %d)\n", eval_depth, evaluated_op->type);
                     eval_depth--;
                     return NULL;
                  }

                 // Evaluate arguments
                 fprintf(stderr, "[DEBUG C eval] Evaluating arguments for evaluated operator %p (type %d)...\n", (void*)evaluated_op, evaluated_op->type);
                 L0_Value* evaluated_args = l0_eval_list(args_expr, env, arena);
                 fprintf(stderr, "[DEBUG C eval] Arguments evaluated. evaluated_args=%p, status=%d\n", (void*)evaluated_args, l0_parser_error_status);
                 if (!evaluated_args || l0_parser_error_status != L0_PARSE_OK) {
                     fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (args eval error - non-symbol op)\n", eval_depth);
                     eval_depth--;
                     return NULL; // Propagate error from arg evaluation
                 }

                 // Apply the evaluated operator to the evaluated arguments
                 // <<< ADDED PRE-APPLY DEBUG (NON-SYMBOL OP) >>>
                 fprintf(stderr, "[DEBUG C eval PRE-APPLY NON-SYMBOL OP] Calling l0_apply with func (evaluated_op) = %p, args (evaluated_args) = %p\n", (void*)evaluated_op, (void*)evaluated_args);
                 // <<< END ADDED PRE-APPLY DEBUG (NON-SYMBOL OP) >>>
                 L0_Value* apply_result = l0_apply(evaluated_op, evaluated_args, env, arena);
                 fprintf(stderr, "[DEBUG C eval] Returned from apply (non-symbol op). Result=%p, Status=%d - Depth %d\n", (void*)apply_result, l0_parser_error_status, eval_depth);
                 eval_depth--; // Decrement depth after apply returns
                 fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning Apply result (non-symbol op), Ptr: %p\n", eval_depth, (void*)apply_result);
                 return apply_result; // Return result (or NULL if apply failed)
            } // <<< END else (op_expr is not a symbol)
            break; // <<< ADDED BREAK TO PREVENT FALLTHROUGH
        } // <<< END case L0_TYPE_PAIR:

        // 4. Other types (should not be evaluated directly in valid programs,
        //    but handle defensively). Closure/Primitive shouldn't appear here directly.
        case L0_TYPE_CLOSURE:
        case L0_TYPE_PRIMITIVE:
        default: {
            char err_buf[100];
            snprintf(err_buf, sizeof(err_buf), "Cannot evaluate value of type %d.", expr->type);
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, err_buf);
            fprintf(stderr, "[DEBUG C eval EXIT] Depth: %d, Returning NULL (Cannot evaluate type %d)\n", eval_depth, expr->type);
            eval_depth--;
            return NULL;
        }
    } // <<< END switch (expr->type)

    // Fallback case (should ideally not be reached if all types/cases are handled)
    fprintf(stderr, "[FATAL ERROR] Control reached end of l0_eval function unexpectedly!\n");
    l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
    l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Control reached end of l0_eval function.");
    eval_depth--;
    return NULL; // Return NULL as a fallback
} // <<< END L0_Value* l0_eval(...)


// --- Quasiquote Expansion Helper ---
// depth: current nesting level of quasiquote
static L0_Value* l0_expand_quasiquote(L0_Value* template, L0_Env* env, L0_Arena* arena, int depth) {
    fprintf(stderr, "[DEBUG QQ Expand ENTRY] Depth: %d, Template Type: %d, Ptr: %p\n", depth, template ? template->type : -1, (void*)template);

    if (!l0_is_pair(template)) {
        // Atoms evaluate to themselves (symbols might need quoting depending on scheme standard, but let's keep simple)
        fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning Atom Template, Ptr: %p\n", depth, (void*)template);
        return template;
    }

    if (l0_is_nil(template)) {
        fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning NIL Template, Ptr: %p\n", depth, (void*)template);
        return template; // Empty list expands to empty list
    }

    // Check for unquote, unquote-splicing, or nested quasiquote at the beginning of a list
    L0_Value* op = template->data.pair.car;
    if (l0_is_symbol(op)) {
        const char* op_sym = op->data.symbol;
        if (strcmp(op_sym, "unquote") == 0) {
            fprintf(stderr, "[DEBUG QQ Expand] Found 'unquote' at depth %d\n", depth);
            if (depth == 1) { // Only unquote at the correct depth
                if (!l0_is_pair(template->data.pair.cdr) || !l0_is_nil(template->data.pair.cdr->data.pair.cdr)) {
                    l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                    l0_parser_error_message = l0_arena_strdup(arena, "'unquote' requires exactly one argument.");
                    return NULL;
                }
                L0_Value* expr_to_eval = template->data.pair.cdr->data.pair.car;
                fprintf(stderr, "[DEBUG QQ Expand] Evaluating unquoted expression: %p\n", (void*)expr_to_eval);
                L0_Value* evaluated_expr = l0_eval(expr_to_eval, env, arena); // Evaluate the expression
                 fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning evaluated unquote result, Ptr: %p\n", depth, (void*)evaluated_expr);
                return evaluated_expr;
            } else {
                // Nested unquote: treat like a normal list starting with 'unquote' symbol
                // Fall through to general pair handling, but decrement depth for recursive call
                depth--;
                fprintf(stderr, "[DEBUG QQ Expand] Nested unquote, reducing depth to %d for recursive call\n", depth);
                // Let the general pair handling below reconstruct `(unquote ...)`
            }
        } else if (strcmp(op_sym, "unquote-splicing") == 0) {
            fprintf(stderr, "[DEBUG QQ Expand] Found 'unquote-splicing' at depth %d\n", depth);
             if (depth == 1) {
                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                 l0_parser_error_message = l0_arena_strdup(arena, "TODO: 'unquote-splicing' is not yet implemented.");
                 fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning NULL (unquote-splicing TODO)\n", depth);
                 return NULL; // TODO: Implement splicing
             } else {
                 // Nested unquote-splicing: treat like a normal list
                 depth--;
                 fprintf(stderr, "[DEBUG QQ Expand] Nested unquote-splicing, reducing depth to %d for recursive call\n", depth);
                 // Let the general pair handling below reconstruct `(unquote-splicing ...)`
             }
        } else if (strcmp(op_sym, "quasiquote") == 0) {
            fprintf(stderr, "[DEBUG QQ Expand] Found nested 'quasiquote', increasing depth\n");
            depth++; // Increase depth for nested quasiquote
            // Let the general pair handling below reconstruct `(quasiquote ...)` with increased depth
        }
        // If none of the above, it's just a regular symbol at the start of a list
    }

    // General pair handling: recursively expand car and cdr
    fprintf(stderr, "[DEBUG QQ Expand] General pair handling for template: %p\n", (void*)template);
    L0_Value* expanded_car = l0_expand_quasiquote(template->data.pair.car, env, arena, depth);
    if (!expanded_car || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning NULL (car expansion failed)\n", depth);
        return NULL;
    }
    fprintf(stderr, "[DEBUG QQ Expand] Expanded car: %p\n", (void*)expanded_car);

    L0_Value* expanded_cdr = l0_expand_quasiquote(template->data.pair.cdr, env, arena, depth);
     if (!expanded_cdr || l0_parser_error_status != L0_PARSE_OK) {
         fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning NULL (cdr expansion failed)\n", depth);
         return NULL;
     }
    fprintf(stderr, "[DEBUG QQ Expand] Expanded cdr: %p\n", (void*)expanded_cdr);

    // Optimization: if car and cdr are unchanged, return original pair
    if (expanded_car == template->data.pair.car && expanded_cdr == template->data.pair.cdr) {
         fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning original pair (no change), Ptr: %p\n", depth, (void*)template);
         return template;
    }

    // TODO: Handle unquote-splicing results here. If expanded_car resulted from
    // splicing, it should be a list, and we need to append expanded_cdr to it.
    // This requires an 'append' primitive or similar list manipulation.
    // For now, assume no splicing happened.

    L0_Value* result = l0_make_pair(arena, expanded_car, expanded_cdr);
    fprintf(stderr, "[DEBUG QQ Expand EXIT] Depth: %d, Returning reconstructed pair, Ptr: %p\n", depth, (void*)result);
    return result;
}


// --- Macro Expansion Function ---

// Helper to find a macro transformer in the *macro-table*
// Returns the transformer closure L0_Value* or NULL if not found or error.
static L0_Value* find_macro_transformer(L0_Value* symbol, L0_Env* env, L0_Arena* arena) {
    if (!l0_is_symbol(symbol)) {
        return NULL; // Can only look up symbols
    }
    const char* sym_name = symbol->data.symbol;
    fprintf(stderr, "[DEBUG find_macro_transformer] Looking for macro '%s'\n", sym_name);

    L0_Value* macro_table_sym = l0_make_symbol(arena, "*macro-table*");
    if (!macro_table_sym) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Internal error: Failed to create '*macro-table*' symbol.");
        return NULL;
    }

    L0_Value* current_macro_table = l0_env_lookup(env, macro_table_sym);
    if (!current_macro_table) {
        // It's okay if *macro-table* doesn't exist yet, just means no macros defined.
        fprintf(stderr, "[DEBUG find_macro_transformer] *macro-table* not found in env %p, assuming no macros.\n", (void*)env);
        return NULL;
    }

    if (!l0_is_list(current_macro_table)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Runtime error: '*macro-table*' is not a list.");
        fprintf(stderr, "[ERROR find_macro_transformer] *macro-table* is not a list (type %d)\n", current_macro_table->type);
        return NULL;
    }

    // Iterate through the macro table (association list: ((name . transformer) ...))
    L0_Value* current_entry_node = current_macro_table;
    while(l0_is_pair(current_entry_node)) {
        L0_Value* entry_pair = current_entry_node->data.pair.car;
        if (l0_is_pair(entry_pair)) {
            L0_Value* macro_name = entry_pair->data.pair.car;
            // Compare the input symbol name with the macro name symbol
            if (l0_is_symbol(macro_name) && strcmp(sym_name, macro_name->data.symbol) == 0) {
                L0_Value* transformer = entry_pair->data.pair.cdr;
                if (!l0_is_closure(transformer)) {
                     l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                     char err_buf[150];
                     snprintf(err_buf, sizeof(err_buf), "Internal error: Macro transformer for '%s' is not a closure (type %d).", sym_name, transformer->type);
                     l0_parser_error_message = l0_arena_strdup(arena, err_buf);
                     fprintf(stderr, "[ERROR find_macro_transformer] Transformer for '%s' is not a closure (type %d)\n", sym_name, transformer->type);
                     return NULL; // Error: transformer must be a closure
                }
                fprintf(stderr, "[DEBUG find_macro_transformer] Found transformer for '%s': %p\n", sym_name, (void*)transformer);
                return transformer; // Found the transformer closure
            }
        } else {
             // Malformed entry in *macro-table*
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
             l0_parser_error_message = l0_arena_strdup(arena, "Runtime error: Malformed entry in '*macro-table*'.");
             fprintf(stderr, "[ERROR find_macro_transformer] Malformed entry in *macro-table*: car is not a pair (type %d)\n", entry_pair ? entry_pair->type : -1);
             return NULL;
        }
        current_entry_node = current_entry_node->data.pair.cdr;
    }

    // Check if the list traversal ended properly
    if (!l0_is_nil(current_entry_node)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Runtime error: '*macro-table*' is not a proper list.");
        fprintf(stderr, "[ERROR find_macro_transformer] *macro-table* is not a proper list (cdr is type %d)\n", current_entry_node ? current_entry_node->type : -1);
        return NULL;
    }

    fprintf(stderr, "[DEBUG find_macro_transformer] Macro '%s' not found in table.\n", sym_name);
    return NULL; // Macro not found
}


// Static variable to track macro expansion depth
static int macroexpand_depth = 0;
#define MAX_MACROEXPAND_DEPTH 500 // Limit expansion depth

// Main macro expansion function
L0_Value* l0_macroexpand(L0_Value* expr, L0_Env* env, L0_Arena* arena) {
    macroexpand_depth++;
    fprintf(stderr, "[DEBUG C macroexpand ENTRY] Depth: %d, Expr Type: %d, Ptr: %p\n", macroexpand_depth, expr ? expr->type : -1, (void*)expr);

    if (macroexpand_depth > MAX_MACROEXPAND_DEPTH) {
        fprintf(stderr, "[FATAL ERROR] Maximum recursion depth (%d) exceeded in l0_macroexpand!\n", MAX_MACROEXPAND_DEPTH);
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Stack overflow suspected in macro expansion.");
        macroexpand_depth--;
        return NULL;
    }

    if (!expr) { // Handle NULL input defensively
        macroexpand_depth--;
        return NULL;
    }

    // 1. Handle non-pair atoms: they expand to themselves.
    if (!l0_is_pair(expr)) {
        fprintf(stderr, "[DEBUG C macroexpand EXIT] Depth: %d, Returning Atom, Ptr: %p\n", macroexpand_depth, (void*)expr);
        macroexpand_depth--;
        return expr;
    }

    // Handle empty list
    if (l0_is_nil(expr)) {
        fprintf(stderr, "[DEBUG C macroexpand EXIT] Depth: %d, Returning NIL, Ptr: %p\n", macroexpand_depth, (void*)expr);
        macroexpand_depth--;
        return expr;
    }

    // 2. Handle pairs (lists)
    L0_Value* op = expr->data.pair.car;
    L0_Value* args = expr->data.pair.cdr;

    // 2.1 Check for 'quote' - do not expand inside quote
    if (l0_is_symbol(op) && strcmp(op->data.symbol, "quote") == 0) {
        fprintf(stderr, "[DEBUG C macroexpand] Found 'quote', not expanding inside. Depth: %d\n", macroexpand_depth);
        macroexpand_depth--;
        return expr;
    }

    // 2.2 Check if the operator is a macro symbol
    L0_Value* transformer = find_macro_transformer(op, env, arena);
    if (transformer) {
        // --- Found a Macro ---
        fprintf(stderr, "[DEBUG C macroexpand] Found macro '%s' at depth %d. Applying transformer %p to args %p.\n",
                op->data.symbol, macroexpand_depth, (void*)transformer, (void*)args);

        // Apply the transformer closure to the *unevaluated* arguments
        L0_Value* expanded_code = l0_apply(transformer, args, env, arena);
        if (!expanded_code || l0_parser_error_status != L0_PARSE_OK) {
            fprintf(stderr, "[ERROR C macroexpand] Macro transformer application failed for '%s'. Status: %d. Depth: %d\n",
                    op->data.symbol, l0_parser_error_status, macroexpand_depth);
            // Error message should be set by l0_apply or the transformer code
            macroexpand_depth--;
            return NULL;
        }
        fprintf(stderr, "[DEBUG C macroexpand] Macro '%s' expanded to: %p. Recursively expanding result. Depth: %d\n",
                op->data.symbol, (void*)expanded_code, macroexpand_depth);

        // Recursively expand the result of the macro transformation
        macroexpand_depth--; // Decrement depth *before* recursive call
        return l0_macroexpand(expanded_code, env, arena);

    } else if (l0_parser_error_status != L0_PARSE_OK) {
        // An error occurred during find_macro_transformer (e.g., *macro-table* malformed)
        fprintf(stderr, "[ERROR C macroexpand] Error occurred during macro lookup. Status: %d. Depth: %d\n", l0_parser_error_status, macroexpand_depth);
        macroexpand_depth--;
        return NULL;
    } else {
        // --- Not a Macro (or quote) ---
        // Recursively expand the car and cdr parts of the list.
        fprintf(stderr, "[DEBUG C macroexpand] Not a macro/quote ('%s'). Recursively expanding car/cdr. Depth: %d\n",
                l0_is_symbol(op) ? op->data.symbol : "<non-symbol>", macroexpand_depth);

        L0_Value* expanded_car = l0_macroexpand(op, env, arena);
        if (!expanded_car || l0_parser_error_status != L0_PARSE_OK) {
            fprintf(stderr, "[ERROR C macroexpand] Failed expanding CAR. Status: %d. Depth: %d\n", l0_parser_error_status, macroexpand_depth);
            macroexpand_depth--;
            return NULL;
        }

        L0_Value* expanded_cdr = l0_macroexpand(args, env, arena);
        if (!expanded_cdr || l0_parser_error_status != L0_PARSE_OK) {
            fprintf(stderr, "[ERROR C macroexpand] Failed expanding CDR. Status: %d. Depth: %d\n", l0_parser_error_status, macroexpand_depth);
            macroexpand_depth--;
            return NULL;
        }

        // Optimization: if car and cdr are unchanged, return original pair
        if (expanded_car == op && expanded_cdr == args) {
            fprintf(stderr, "[DEBUG C macroexpand EXIT] Depth: %d, Returning original pair (no change), Ptr: %p\n", macroexpand_depth, (void*)expr);
            macroexpand_depth--;
            return expr;
        }

        // Reconstruct the pair with expanded parts
        L0_Value* result = l0_make_pair(arena, expanded_car, expanded_cdr);
        fprintf(stderr, "[DEBUG C macroexpand EXIT] Depth: %d, Returning reconstructed pair, Ptr: %p\n", macroexpand_depth, (void*)result);
        macroexpand_depth--;
        return result;
    }
}
