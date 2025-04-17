#include "l0_primitives.h"
#include "l0_types.h"
#include "l0_env.h"
#include "l0_arena.h"
#include "l0_parser.h" // Include parser for l0_parse_string and error status definitions
#include "l0_codegen.h" // <<< Include codegen for l0_codegen_program
#include "l0_eval.h"    // <<< Include eval for prim_eval
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h> // <-- Add this include for isprint
#include <assert.h>

// --- Global argc/argv (to be set by main) ---
// WARNING: This is a simplification for Stage 0. A better approach
// would avoid globals, perhaps passing them through the initial env setup.
int g_argc = 0;
char **g_argv = NULL;

// --- Helper Macro for Argument Checking ---
// Gets the Nth argument (0-indexed) from the args list.
// Returns NULL and sets error if args list is too short or malformed.
static L0_Value* get_arg(L0_Value* args, int n, L0_Arena* arena, const char* prim_name) {
    L0_Value* current_arg_pair = args;
    for (int i = 0; i < n; ++i) {
        if (!l0_is_pair(current_arg_pair)) {
             // Error: Not enough arguments
             char err_buf[100];
             snprintf(err_buf, sizeof(err_buf), "Primitive '%s': Expected at least %d arguments, got fewer.", prim_name, n + 1);
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME; // Use a runtime error status
             l0_parser_error_message = l0_arena_strdup(arena, err_buf); // Allocate error message in arena
             return NULL;
        }
        current_arg_pair = current_arg_pair->data.pair.cdr;
    }
    // At the nth pair, check if it's actually a pair
     if (!l0_is_pair(current_arg_pair)) {
         char err_buf[100];
         snprintf(err_buf, sizeof(err_buf), "Primitive '%s': Expected at least %d arguments, got fewer (malformed arg list?).", prim_name, n + 1);
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         l0_parser_error_message = l0_arena_strdup(arena, err_buf);
         return NULL;
     }
    return current_arg_pair->data.pair.car; // Return the actual argument value
}

// Helper to check if the argument list has *exactly* N arguments.
static bool check_arg_count(L0_Value* args, int n, L0_Arena* arena, const char* prim_name) {
    int count = 0;
    L0_Value* current = args;
    while (l0_is_pair(current)) {
        count++;
        current = current->data.pair.cdr;
    }
    // After the loop, current should be NIL for a proper list
    if (!l0_is_nil(current)) {
         char err_buf[100];
         snprintf(err_buf, sizeof(err_buf), "Primitive '%s': Argument list is not a proper list.", prim_name);
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         l0_parser_error_message = l0_arena_strdup(arena, err_buf);
         return false;
    }
    if (count != n) {
        char err_buf[100];
        snprintf(err_buf, sizeof(err_buf), "Primitive '%s': Expected exactly %d arguments, got %d.", prim_name, n, count);
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return false;
    }
    return true;
}


// --- Primitive Implementations ---

// (cons a b)
L0_Value* prim_cons(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // env usually not needed for cons
    if (!check_arg_count(args, 2, arena, "cons")) return NULL;
    L0_Value* car_val = get_arg(args, 0, arena, "cons"); // Already checked count
    L0_Value* cdr_val = get_arg(args, 1, arena, "cons"); // Already checked count
    // No type checking needed for cons arguments
    return l0_make_pair(arena, car_val, cdr_val);
}

// (list item ...) - Returns a new list containing the evaluated arguments
L0_Value* prim_list(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // env not needed
    (void)arena; // arena not directly needed, args are already allocated
    // The arguments passed to a primitive are already evaluated.
    // The 'list' primitive simply returns the list of evaluated arguments it received.
    // We need to ensure the args list itself is proper.
    L0_Value* current = args;
    while (l0_is_pair(current)) {
        current = current->data.pair.cdr;
    }
    if (!l0_is_nil(current)) {
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'list': Internal error - received improper argument list structure.");
         return NULL;
    }
    return args; // Return the list of evaluated arguments directly
}

// (car p)
L0_Value* prim_car(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "car")) return NULL;
    L0_Value* pair = get_arg(args, 0, arena, "car");
    if (!l0_is_pair(pair)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'car': Argument must be a pair.");
        return NULL;
    }
    return pair->data.pair.car;
}

// (cdr p)
L0_Value* prim_cdr(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "cdr")) return NULL;
    L0_Value* pair = get_arg(args, 0, arena, "cdr");
    if (!l0_is_pair(pair)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'cdr': Argument must be a pair.");
        return NULL;
    }
    return pair->data.pair.cdr;
}

// (pair? x)
L0_Value* prim_pair_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "pair?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "pair?");
    return l0_make_boolean(arena, l0_is_pair(val));
}

// (null? x)
L0_Value* prim_null_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "null?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "null?");
    return l0_make_boolean(arena, l0_is_nil(val));
}

// Helper to get numeric value as double, returns false on type error
static bool get_numeric_as_double(L0_Value* val, double* out, L0_Arena* arena, const char* prim_name) {
    if (l0_is_integer(val)) {
        *out = (double)val->data.integer;
        return true;
    } else if (l0_is_float(val)) {
        *out = val->data.float_num;
        return true;
    } else {
        char err_buf[100];
        snprintf(err_buf, sizeof(err_buf), "Primitive '%s': Expected integer or float argument.", prim_name);
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return false;
    }
}


// (+) - Variadic sum (handles integers and floats)
L0_Value* prim_add(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    long int_sum = 0;
    double float_sum = 0.0;
    bool has_float = false;
    L0_Value* current = args;

    while (l0_is_pair(current)) {
        L0_Value* arg = current->data.pair.car;
        if (l0_is_float(arg)) {
            if (!has_float) {
                // Convert previous integer sum to float
                float_sum = (double)int_sum;
                has_float = true;
            }
            float_sum += arg->data.float_num;
        } else if (l0_is_integer(arg)) {
            if (has_float) {
                float_sum += (double)arg->data.integer;
            } else {
                int_sum += arg->data.integer;
            }
        } else {
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive '+': Arguments must be numbers (integer or float).");
            return NULL;
        }
        current = current->data.pair.cdr;
    }
    if (!l0_is_nil(current)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '+': Argument list is not a proper list.");
        return NULL;
    }

    if (has_float) {
        return l0_make_float(arena, float_sum);
    } else {
        return l0_make_integer(arena, int_sum);
    }
}

// (-) - Subtraction (handles integers, floats, one or more args)
L0_Value* prim_subtract(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (l0_is_nil(args)) { // (-): Error, needs at least one argument
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '-': Requires at least one argument.");
        return NULL;
    }

    L0_Value* first_arg = args->data.pair.car;
    double first_val_d;
    if (!get_numeric_as_double(first_arg, &first_val_d, arena, "-")) return NULL;
    bool has_float = l0_is_float(first_arg);

    L0_Value* current = args->data.pair.cdr;

    if (l0_is_nil(current)) { // (- x): Negation
        if (has_float) {
            return l0_make_float(arena, -first_val_d);
        } else {
            return l0_make_integer(arena, -(long)first_val_d); // Cast back if it was originally int
        }
    } else { // (- x y z ...): Subtraction
        double float_result = first_val_d;
        long int_result = (long)first_val_d; // Keep track of int result if possible

        while (l0_is_pair(current)) {
            L0_Value* arg = current->data.pair.car;
            double arg_val_d;
            if (!get_numeric_as_double(arg, &arg_val_d, arena, "-")) return NULL;

            if (l0_is_float(arg) && !has_float) {
                // Promote current result to float
                float_result = (double)int_result;
                has_float = true;
            }

            if (has_float) {
                float_result -= arg_val_d;
            } else { // Both current result and arg are integers
                int_result -= (long)arg_val_d;
            }
            current = current->data.pair.cdr;
        }
        if (!l0_is_nil(current)) { // Check for improper list
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive '-': Argument list is not a proper list.");
            return NULL;
        }

        if (has_float) {
            return l0_make_float(arena, float_result);
        } else {
            return l0_make_integer(arena, int_result);
        }
    }
}


// (*) - Variadic product (handles integers and floats)
L0_Value* prim_multiply(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    long int_product = 1;
    double float_product = 1.0;
    bool has_float = false;
    L0_Value* current = args;

    while (l0_is_pair(current)) {
        L0_Value* arg = current->data.pair.car;
        if (l0_is_float(arg)) {
            if (!has_float) {
                // Convert previous integer product to float
                float_product = (double)int_product;
                has_float = true;
            }
            float_product *= arg->data.float_num;
        } else if (l0_is_integer(arg)) {
            if (has_float) {
                float_product *= (double)arg->data.integer;
            } else {
                int_product *= arg->data.integer;
            }
        } else {
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive '*': Arguments must be numbers (integer or float).");
            return NULL;
        }
        current = current->data.pair.cdr;
    }
    if (!l0_is_nil(current)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '*': Argument list is not a proper list.");
        return NULL;
    }

    if (has_float) {
        return l0_make_float(arena, float_product);
    } else {
        return l0_make_integer(arena, int_product);
    }
}

// (/) - Division (always returns float, handles integers and floats)
L0_Value* prim_divide(L0_Value* args, L0_Env* env, L0_Arena* arena) {
     (void)env;
    if (l0_is_nil(args)) { // Needs at least one argument for (/ x) -> 1/x
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '/': Requires at least one argument.");
        return NULL;
    }

    L0_Value* first_arg = args->data.pair.car;
    double first_val_d;
    if (!get_numeric_as_double(first_arg, &first_val_d, arena, "/")) return NULL;

    L0_Value* current = args->data.pair.cdr;
    double result_d;

    if (l0_is_nil(current)) { // (/ x) -> 1/x
        if (first_val_d == 0.0) {
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
             l0_parser_error_message = l0_arena_strdup(arena, "Primitive '/': Division by zero (1/0).");
             return NULL;
        }
        result_d = 1.0 / first_val_d;
    } else { // (/ x y z ...)
        result_d = first_val_d;
        while (l0_is_pair(current)) {
            L0_Value* arg = current->data.pair.car;
            double arg_val_d;
             if (!get_numeric_as_double(arg, &arg_val_d, arena, "/")) return NULL;

            if (arg_val_d == 0.0) {
                l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                l0_parser_error_message = l0_arena_strdup(arena, "Primitive '/': Division by zero.");
                return NULL;
            }
            result_d /= arg_val_d;
            current = current->data.pair.cdr;
        }
        if (!l0_is_nil(current)) { // Check for improper list
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive '/': Argument list is not a proper list.");
            return NULL;
        }
    }
    return l0_make_float(arena, result_d);
}


// (= a b ...) - Equality check (handles numbers, booleans, nil, symbols)
// Note: Scheme's = is numeric equality. eq? or equal? handle other types.
// This implements numeric equality for integers and floats.
L0_Value* prim_equal(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (l0_is_nil(args) || l0_is_nil(args->data.pair.cdr)) {
        // = requires at least two arguments for comparison
        return l0_make_boolean(arena, true); // (=) or (= x) is true
    }

    L0_Value* first = args->data.pair.car;
    L0_Value* current = args->data.pair.cdr;
    double first_d;
    bool first_is_num = get_numeric_as_double(first, &first_d, arena, "="); // Check if first is numeric

    while (l0_is_pair(current)) {
        L0_Value* next = current->data.pair.car;
        double next_d;
        bool next_is_num = get_numeric_as_double(next, &next_d, arena, "=");

        if (!first_is_num || !next_is_num) {
             // If either is not numeric, they can't be numerically equal
             // Reset error state potentially set by get_numeric_as_double if non-numeric comparison is intended
             l0_parser_error_status = L0_PARSE_OK;
             l0_parser_error_message = NULL;
             return l0_make_boolean(arena, false);
        }

        // Direct float comparison (consider epsilon for real-world scenarios)
        if (first_d != next_d) {
            return l0_make_boolean(arena, false);
        }

        // Prepare for next iteration (no need, we compare each to the first)
        // first = next; // This would check pairwise a=b, b=c, etc.
        // first_d = next_d;
        // first_is_num = next_is_num;

        current = current->data.pair.cdr;
    }
     if (!l0_is_nil(current)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '=': Argument list is not a proper list.");
        return NULL;
     }

    return l0_make_boolean(arena, true); // All compared elements were equal
}


// (< a b ...) - Less than (handles numbers)
L0_Value* prim_less_than(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
     if (l0_is_nil(args) || l0_is_nil(args->data.pair.cdr)) {
        // < requires at least two arguments for comparison
        return l0_make_boolean(arena, true); // (<) or (< x) is true
    }

    L0_Value* prev = args->data.pair.car;
    L0_Value* current_pair = args->data.pair.cdr;
    double prev_d;
    if (!get_numeric_as_double(prev, &prev_d, arena, "<")) return NULL; // Error if first is not number

    while (l0_is_pair(current_pair)) {
        L0_Value* current = current_pair->data.pair.car;
        double current_d;
        if (!get_numeric_as_double(current, &current_d, arena, "<")) return NULL; // Error if subsequent is not number

        if (!(prev_d < current_d)) {
            return l0_make_boolean(arena, false); // Not strictly increasing
        }
        prev_d = current_d; // Update previous value for next comparison
        current_pair = current_pair->data.pair.cdr;
    }
     if (!l0_is_nil(current_pair)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '<': Argument list is not a proper list.");
        return NULL;
     }

    return l0_make_boolean(arena, true); // Strictly increasing order
}

// (> a b ...) - Greater than (handles numbers)
L0_Value* prim_greater_than(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
     if (l0_is_nil(args) || l0_is_nil(args->data.pair.cdr)) {
        // > requires at least two arguments for comparison
        return l0_make_boolean(arena, true); // (>) or (> x) is true
    }

    L0_Value* prev = args->data.pair.car;
    L0_Value* current_pair = args->data.pair.cdr;
    double prev_d;
    if (!get_numeric_as_double(prev, &prev_d, arena, ">")) return NULL; // Error if first is not number

    while (l0_is_pair(current_pair)) {
        L0_Value* current = current_pair->data.pair.car;
        double current_d;
        if (!get_numeric_as_double(current, &current_d, arena, ">")) return NULL; // Error if subsequent is not number

        if (!(prev_d > current_d)) {
            return l0_make_boolean(arena, false); // Not strictly decreasing
        }
        prev_d = current_d; // Update previous value for next comparison
        current_pair = current_pair->data.pair.cdr;
    }
     if (!l0_is_nil(current_pair)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive '>': Argument list is not a proper list.");
        return NULL;
     }

    return l0_make_boolean(arena, true); // Strictly decreasing order
}

// (integer? x)
L0_Value* prim_integer_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "integer?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "integer?");
    return l0_make_boolean(arena, l0_is_integer(val));
}

// (boolean? x)
L0_Value* prim_boolean_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "boolean?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "boolean?");
    return l0_make_boolean(arena, l0_is_boolean(val));
}

// (symbol? x)
L0_Value* prim_symbol_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "symbol?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "symbol?");
    return l0_make_boolean(arena, l0_is_symbol(val));
}

// (string? x)
L0_Value* prim_string_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "string?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "string?");
    return l0_make_boolean(arena, l0_is_string(val));
}

// (float? x)
L0_Value* prim_float_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "float?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "float?");
    return l0_make_boolean(arena, l0_is_float(val));
}

// (not x) -> boolean
// Returns #t if x is #f, otherwise returns #f.
L0_Value* prim_not(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "not")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "not");

    // The 'not' function returns true (#t) only if the argument is false (#f).
    // For all other input values (including #t), it returns false (#f).
    bool is_l0_false = l0_is_boolean(val) && !val->data.boolean;
    return l0_make_boolean(arena, is_l0_false);
}


// (closure? x) - Renamed from prim_closure_q to match direct C call
L0_Value* prim_closure_p(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "closure?")) return NULL;
    L0_Value* val = get_arg(args, 0, arena, "closure?");
    return l0_make_boolean(arena, l0_is_closure(val));
}


// (string-append str1 str2 ...) - Variadic string concatenation
L0_Value* prim_string_append(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    size_t total_len = 0;
    int arg_count = 0;
    L0_Value* current = args;

    // First pass: calculate total length and check types
    while (l0_is_pair(current)) {
        L0_Value* arg = current->data.pair.car;
        if (!l0_is_string(arg)) {
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-append': All arguments must be strings.");
            return NULL;
        }
        total_len += strlen(arg->data.string);
        arg_count++;
        current = current->data.pair.cdr;
    }
    if (!l0_is_nil(current)) { // Check for improper list
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-append': Argument list is not a proper list.");
        return NULL;
    }

    // Allocate buffer in arena (total length + null terminator)
    char* buffer = l0_arena_alloc(arena, total_len + 1, alignof(char));
    if (!buffer) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for string-append buffer";
        return NULL;
    }
    buffer[0] = '\0'; // Start with an empty string for strcat

    // Second pass: concatenate strings
    current = args;
    while (l0_is_pair(current)) {
        L0_Value* arg = current->data.pair.car;
        // We already checked type, just append
        strcat(buffer, arg->data.string); // strcat is safe here because we calculated total length
        current = current->data.pair.cdr;
    }

    return l0_make_string(arena, buffer);
}

// (string->symbol str)
L0_Value* prim_string_to_symbol(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "string->symbol")) return NULL;
    L0_Value* str_val = get_arg(args, 0, arena, "string->symbol");
    if (!l0_is_string(str_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string->symbol': Argument must be a string.");
        return NULL;
    }
    // l0_make_symbol will copy the string into the arena
    return l0_make_symbol(arena, str_val->data.string);
}

// (symbol->string sym)
L0_Value* prim_symbol_to_string(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "symbol->string")) return NULL;
    L0_Value* sym_val = get_arg(args, 0, arena, "symbol->string");
    if (!l0_is_symbol(sym_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'symbol->string': Argument must be a symbol.");
        return NULL;
    }
    // l0_make_string will copy the string into the arena
    return l0_make_string(arena, sym_val->data.symbol);
}

// Helper function to convert an L0 value to a string representation (recursive)
// Returns the number of characters written (excluding null terminator), or -1 on error.
// Needs careful buffer management and cycle detection.
// For simplicity in L0, we'll use a fixed-size buffer initially.
// A more robust version would dynamically allocate or use the arena.
#define PRINT_BUFFER_SIZE 8192 // Increased buffer size significantly
// NOTE: Removed 'static' to make it externally visible for linking
int l0_value_to_string_recursive(L0_Value* value, char* buffer, size_t buf_size, L0_Arena* arena, int depth) {
    if (depth > 10) { // Simple cycle/depth limit
        return snprintf(buffer, buf_size, "...");
    }
    if (!value) return snprintf(buffer, buf_size, "<NULL_VALUE>"); // Should not happen

    int written = 0;
    switch (value->type) {
        case L0_TYPE_NIL:
            written = snprintf(buffer, buf_size, "()");
            break;
        case L0_TYPE_BOOLEAN:
            written = snprintf(buffer, buf_size, "%s", value->data.boolean ? "#t" : "#f");
            break;
        case L0_TYPE_INTEGER:
            written = snprintf(buffer, buf_size, "%ld", value->data.integer);
            break;
        case L0_TYPE_SYMBOL:
            written = snprintf(buffer, buf_size, "%s", value->data.symbol);
            break;
        case L0_TYPE_STRING:
            // Need to escape special characters within the string for display
            written = snprintf(buffer, buf_size, "\"");
            const char* s = value->data.string;
            size_t remaining = buf_size - written - 1; // Space for quote and null term
            while (*s && remaining > 1) { // Need space for char and potential escape
                char c = *s++;
                int w = 0;
                if (c == '"' || c == '\\') {
                    if (remaining >= 2) {
                       buffer[written++] = '\\';
                       buffer[written++] = c;
                       remaining -= 2;
                    } else break; // Not enough space
                } else if (c == '\n') {
                     if (remaining >= 2) {
                       buffer[written++] = '\\';
                       buffer[written++] = 'n';
                       remaining -= 2;
                    } else break;
                } else if (c == '\t') {
                     if (remaining >= 2) {
                       buffer[written++] = '\\';
                       buffer[written++] = 't';
                       remaining -= 2;
                    } else break;
                } else if (isprint((unsigned char)c)) {
                    if (remaining >= 1) {
                        buffer[written++] = c;
                        remaining--;
                    } else break;
                } else {
                    // Handle non-printable chars if needed (e.g., \xHH)
                    if (remaining >= 4) {
                        snprintf(buffer + written, 5, "\\x%02x", (unsigned char)c);
                        written += 4;
                        remaining -=4;
                    } else break;
                }
            }
            if (remaining >= 1) {
                buffer[written++] = '"';
            }
            buffer[written] = '\0'; // Ensure null termination
            break;
        case L0_TYPE_PAIR: {
            written = snprintf(buffer, buf_size, "(");
            L0_Value* current = value;
            bool first = true;
            while (l0_is_pair(current)) {
                 if (!first) {
                    if (written < buf_size - 1) buffer[written++] = ' '; else break;
                 }
                 int w = l0_value_to_string_recursive(current->data.pair.car, buffer + written, buf_size - written, arena, depth + 1);
                 if (w < 0 || written + w >= buf_size) { written = -1; break; } // Error or buffer full
                 written += w;
                 current = current->data.pair.cdr;
                 first = false;
            }
            if (written < 0) break; // Propagate error

            if (!l0_is_nil(current)) { // Improper list
                 if (written < buf_size - 3) { // Need space for " . "
                    buffer[written++] = ' ';
                    buffer[written++] = '.';
                    buffer[written++] = ' ';
                 } else { written = -1; break; } // Buffer full

                 int w = l0_value_to_string_recursive(current, buffer + written, buf_size - written, arena, depth + 1);
                 if (w < 0 || written + w >= buf_size) { written = -1; break; } // Error or buffer full
                 written += w;
            }
             if (written < 0) break;

            if (written < buf_size - 1) buffer[written++] = ')'; else written = -1; // Buffer full
            buffer[written] = '\0';
            break;
        }
        case L0_TYPE_PRIMITIVE:
            written = snprintf(buffer, buf_size, "<primitive:%p>", (void*)value->data.primitive.func);
            break;
        case L0_TYPE_CLOSURE:
             written = snprintf(buffer, buf_size, "<closure:%p>", (void*)value);
            break;
        case L0_TYPE_FLOAT: // <<< ADDED CASE FOR FLOAT
            // Use %g for a general representation, potentially adjust precision if needed
            written = snprintf(buffer, buf_size, "%.15g", value->data.float_num);
            break;
        default:
            written = snprintf(buffer, buf_size, "<unknown_type:%d>", value->type);
            break;
    }
    return (written >= 0 && (size_t)written < buf_size) ? written : -1;
}


// (print expr ...) - Prints expressions to stdout
L0_Value* prim_print(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    fprintf(stderr, "[DEBUG PRIM print] Entered prim_print.\n"); fflush(stderr); // DEBUG ENTRY
    (void)env;
    L0_Value* current = args;
    bool first = true;
    // Allocate buffer from the arena instead of using a fixed stack buffer.
    char* buffer = l0_arena_alloc(arena, PRINT_BUFFER_SIZE, alignof(char));
    if (!buffer) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for print buffer";
        fprintf(stderr, "[DEBUG PRIM print] Arena allocation failed for buffer!\n"); fflush(stderr); // DEBUG ALLOC FAIL
        return NULL; // Indicate error
    }
    fprintf(stderr, "[DEBUG PRIM print] Allocated print buffer (%zu bytes) at %p\n", (size_t)PRINT_BUFFER_SIZE, (void*)buffer); fflush(stderr); // DEBUG ALLOC SUCCESS


    fprintf(stderr, "[DEBUG PRIM print] Starting loop. args=%p\n", (void*)args); fflush(stderr); // DEBUG LOOP START
    while (l0_is_pair(current)) {
        fprintf(stderr, "[DEBUG PRIM print] Loop iteration. current=%p\n", (void*)current); fflush(stderr); // DEBUG ITERATION
        if (!first) {
            fprintf(stderr, "[DEBUG PRIM print] Printing space.\n"); fflush(stderr); // DEBUG SPACE
            printf(" ");
        }
        L0_Value* value_to_print = current->data.pair.car;
        fprintf(stderr, "[DEBUG PRIM print] Calling l0_value_to_string_recursive for value=%p (type=%d)\n", (void*)value_to_print, value_to_print ? value_to_print->type : -1); fflush(stderr); // DEBUG BEFORE CALL
        // Use PRINT_BUFFER_SIZE instead of sizeof(buffer) as buffer is now a pointer
        int written = l0_value_to_string_recursive(value_to_print, buffer, PRINT_BUFFER_SIZE, arena, 0);
        fprintf(stderr, "[DEBUG PRIM print] l0_value_to_string_recursive returned: %d\n", written); fflush(stderr); // DEBUG AFTER CALL

        if (written < 0) {
            fprintf(stderr, "[DEBUG PRIM print] Print error occurred during string conversion.\n"); fflush(stderr); // DEBUG PRINT ERROR
            printf("<print_error>"); // Indicate error converting value to string
        } else {
            fprintf(stderr, "[DEBUG PRIM print] Printing buffer content: '%.50s...'\n", buffer); fflush(stderr); // DEBUG PRINT BUFFER
            printf("%s", buffer);
        }
        current = current->data.pair.cdr;
        first = false;
    }
    fprintf(stderr, "[DEBUG PRIM print] Loop finished. current=%p\n", (void*)current); fflush(stderr); // DEBUG LOOP END

     if (!l0_is_nil(current)) { // Check for improper list
        fprintf(stderr, "[DEBUG PRIM print] Improper list detected!\n"); fflush(stderr); // DEBUG IMPROPER LIST
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'print': Argument list is not a proper list.");
        return NULL;
     }

    fprintf(stderr, "[DEBUG PRIM print] Printing newline.\n"); fflush(stderr); // DEBUG NEWLINE
    printf("\n"); // Print newline at the end
    fflush(stdout); // Ensure output is visible immediately

    fprintf(stderr, "[DEBUG PRIM print] Returning #t.\n"); fflush(stderr); // DEBUG RETURN
    return l0_make_boolean(arena, true); // Return #t to indicate success
}

// (read-file filename) -> string or #f on error
L0_Value* prim_read_file(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    // --- ADD ENTRY DEBUG PRINT ---
    fprintf(stderr, "[DEBUG PRIM read-file] Entered function.\n");
    fflush(stderr);
    // --- END ENTRY DEBUG PRINT ---
    (void)env;
    if (!check_arg_count(args, 1, arena, "read-file")) return NULL;
    L0_Value* filename_val = get_arg(args, 0, arena, "read-file");
    if (!l0_is_string(filename_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'read-file': Argument must be a string filename.");
        return NULL;
    }
    const char* filename = filename_val->data.string;

    FILE* file = fopen(filename, "rb"); // Open in binary read mode
    if (!file) {
        // Could set a more specific error message here
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[200];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'read-file': Could not open file '%s'.", filename);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return l0_make_boolean(arena, false); // Return #f on error
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(file);
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[200];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'read-file': Could not determine size of file '%s'.", filename);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return l0_make_boolean(arena, false);
    }

    // Allocate buffer in arena (+1 for null terminator)
    char* buffer = l0_arena_alloc(arena, file_size + 1, alignof(char));
    if (!buffer) {
        fclose(file);
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for read-file buffer";
        // Return #f to L0 on allocation failure, consistent with other errors
        return l0_make_boolean(arena, false);
    }

    // Read file content
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    fprintf(stderr, "[DEBUG PRIM read-file] After fread, bytes_read = %zu, file_size = %ld\n", bytes_read, file_size); // DEBUG 1

    if (bytes_read != (size_t)file_size) {
        // Read error occurred
        fprintf(stderr, "[DEBUG PRIM read-file] Read error detected.\n"); // DEBUG (error path)
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[200];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'read-file': Error reading file '%s'.", filename);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        // Buffer is allocated in arena, no need to free, but content is invalid
        return l0_make_boolean(arena, false); // Return #f on error
    }

    buffer[file_size] = '\0'; // Null-terminate the buffer
    fprintf(stderr, "[DEBUG PRIM read-file] Buffer null-terminated. First 10 chars: '%.10s'\n", buffer); // DEBUG 2

    // Create the L0 string value, checking for allocation failure
    fprintf(stderr, "[DEBUG PRIM read-file] Calling l0_make_string...\n"); // DEBUG 3
    L0_Value* result_string = l0_make_string(arena, buffer);
    fprintf(stderr, "[DEBUG PRIM read-file] l0_make_string returned %p\n", (void*)result_string); // DEBUG 4

    if (!result_string) {
        // Allocation failed when creating the final L0 string value
        fprintf(stderr, "[DEBUG PRIM read-file] Failed to allocate final L0 string in arena, returning #f.\n"); // Existing debug print
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for read-file result string";
        return l0_make_boolean(arena, false); // Return #f on error
    }
    fprintf(stderr, "[DEBUG PRIM read-file] Successfully created L0 string, returning value.\n"); // DEBUG 5
    return result_string;
}

// (write-file filename content) -> #t or #f
L0_Value* prim_write_file(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 2, arena, "write-file")) return NULL;
    L0_Value* filename_val = get_arg(args, 0, arena, "write-file");
    L0_Value* content_val = get_arg(args, 1, arena, "write-file");

    if (!l0_is_string(filename_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'write-file': First argument must be a string filename.");
        return NULL;
    }
     if (!l0_is_string(content_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'write-file': Second argument must be a string content.");
        return NULL;
    }

    const char* filename = filename_val->data.string;
    const char* content = content_val->data.string;
    size_t content_len = strlen(content);

    FILE* file = fopen(filename, "wb"); // Open in binary write mode
    if (!file) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[200];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'write-file': Could not open file '%s' for writing.", filename);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return l0_make_boolean(arena, false); // Return #f on error
    }

    size_t bytes_written = fwrite(content, 1, content_len, file);
    int close_status = fclose(file); // Capture fclose result

    if (bytes_written != content_len || close_status == EOF) { // Check both fwrite and fclose
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[200];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'write-file': Error writing to file '%s'.", filename);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return l0_make_boolean(arena, false); // Return #f on error
    }

    return l0_make_boolean(arena, true); // Return #t on success
}


// --- String Primitives ---

// (string-length str)
L0_Value* primitive_string_length(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "string-length")) return NULL;
    L0_Value* str_val = get_arg(args, 0, arena, "string-length");
    if (!l0_is_string(str_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-length': Argument must be a string.");
        return NULL;
    }
    // String is null-terminated, strlen is safe
    size_t len = strlen(str_val->data.string);
    // Need to cast size_t to long for L0 integer type
    if (len > (size_t)LONG_MAX) { // Basic overflow check
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-length': String length exceeds maximum integer value.");
         return NULL;
    }
    return l0_make_integer(arena, (long)len);
}

// (string-ref str k)
L0_Value* primitive_string_ref(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 2, arena, "string-ref")) return NULL;
    L0_Value* str_val = get_arg(args, 0, arena, "string-ref");
    L0_Value* idx_val = get_arg(args, 1, arena, "string-ref");

    if (!l0_is_string(str_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-ref': First argument must be a string.");
        return NULL;
    }
    if (!l0_is_integer(idx_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'string-ref': Second argument must be an integer index.");
        return NULL;
    }

    const char* str = str_val->data.string;
    long k = idx_val->data.integer;
    size_t len = strlen(str);

    if (k < 0 || (size_t)k >= len) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        char err_buf[100];
        snprintf(err_buf, sizeof(err_buf), "Primitive 'string-ref': Index %ld out of bounds for string of length %zu.", k, len);
        l0_parser_error_message = l0_arena_strdup(arena, err_buf);
        return NULL;
    }

    // Return the character as an integer
    return l0_make_integer(arena, (long)(unsigned char)str[k]); // Cast to unsigned char first then long
}

// (substring str start [end])
L0_Value* primitive_substring(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    // Check for 2 or 3 arguments
    int arg_count = 0;
    L0_Value* current = args;
    while (l0_is_pair(current)) {
        arg_count++;
        current = current->data.pair.cdr;
    }
    if (!l0_is_nil(current) || (arg_count != 2 && arg_count != 3)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'substring': Expected 2 or 3 arguments (string start [end]).");
        return NULL;
    }

    L0_Value* str_val = get_arg(args, 0, arena, "substring");
    L0_Value* start_val = get_arg(args, 1, arena, "substring");
    L0_Value* end_val = (arg_count == 3) ? get_arg(args, 2, arena, "substring") : NULL;

    if (!l0_is_string(str_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'substring': First argument must be a string.");
        return NULL;
    }
    if (!l0_is_integer(start_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'substring': Second argument (start) must be an integer.");
        return NULL;
    }
    if (end_val && !l0_is_integer(end_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'substring': Third argument (end) must be an integer if provided.");
        return NULL;
    }

    const char* str = str_val->data.string;
    size_t len = strlen(str);
    long start_idx = start_val->data.integer;
    long end_idx = len; // Default end index is string length

    if (end_val) {
        end_idx = end_val->data.integer;
    }

    // Validate indices
    if (start_idx < 0 || (size_t)start_idx > len || end_idx < start_idx || (size_t)end_idx > len) {
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         char err_buf[150];
         snprintf(err_buf, sizeof(err_buf), "Primitive 'substring': Index out of bounds (start=%ld, end=%ld, len=%zu).", start_idx, end_idx, len);
         l0_parser_error_message = l0_arena_strdup(arena, err_buf);
         return NULL;
    }

    size_t sub_len = (size_t)(end_idx - start_idx);
    char* buffer = l0_arena_alloc(arena, sub_len + 1, alignof(char));
    if (!buffer) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for substring buffer";
        return NULL;
    }

    strncpy(buffer, str + start_idx, sub_len);
    buffer[sub_len] = '\0';

    return l0_make_string(arena, buffer);
}

// (number->string num) - Handles integers and floats
L0_Value* primitive_number_to_string(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 1, arena, "number->string")) return NULL;
    L0_Value* num_val = get_arg(args, 0, arena, "number->string");

    char temp_buffer[64]; // Increased buffer size for floats
    int len = -1;

    if (l0_is_integer(num_val)) {
        long num = num_val->data.integer;
        len = snprintf(temp_buffer, sizeof(temp_buffer), "%ld", num);
    } else if (l0_is_float(num_val)) {
        double num = num_val->data.float_num;
        // Use %g for general float format (avoids trailing zeros like %f)
        // Adjust precision as needed, e.g., "%.15g" for more precision
        len = snprintf(temp_buffer, sizeof(temp_buffer), "%g", num);
    } else {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'number->string': Argument must be an integer or float.");
        return NULL;
    }


    if (len < 0 || (size_t)len >= sizeof(temp_buffer)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'number->string': Error converting number to string or buffer overflow.");
        return NULL;
    }

    // Allocate exact size in arena
    char* buffer = l0_arena_alloc(arena, len + 1, alignof(char));
     if (!buffer) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for number->string buffer";
        return NULL;
    }
    strcpy(buffer, temp_buffer);

    return l0_make_string(arena, buffer);
}

// (command-line-args) -> list of strings
L0_Value* prim_command_line_args(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Not needed
    // Check that no arguments were passed to the primitive itself
    if (!check_arg_count(args, 0, arena, "command-line-args")) return NULL;

    if (g_argc <= 1 || g_argv == NULL) {
        return L0_NIL; // No arguments other than program name, or not initialized
    }

    // Build the list of arguments in reverse order first
    L0_Value* arg_list = L0_NIL;
    for (int i = g_argc - 1; i >= 1; --i) { // Iterate backwards, skip argv[0]
        L0_Value* arg_str = l0_make_string(arena, g_argv[i]);
        if (!arg_str) {
            l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
            l0_parser_error_message = "Arena allocation failed for command-line argument string";
            return NULL; // Allocation failure
        }
        arg_list = l0_make_pair(arena, arg_str, arg_list);
         if (!arg_list) {
            l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
            l0_parser_error_message = "Arena allocation failed for command-line argument list pair";
            return NULL; // Allocation failure
        }
    }
    return arg_list;
}

// (parse-string str) -> list (AST) or #f on error
L0_Value* prim_parse_string(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Not needed for parsing itself
    if (!check_arg_count(args, 1, arena, "parse-string")) return NULL; // Returns NULL on error, error status set by check_arg_count
    L0_Value* str_val = get_arg(args, 0, arena, "parse-string");
    if (!l0_is_string(str_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'parse-string': Argument must be a string.");
        return NULL; // Return NULL on type error
    }

    const char* input_string = str_val->data.string;
    // Reset parser error state before calling
    l0_parser_error_status = L0_PARSE_OK;
    l0_parser_error_message = NULL;
    l0_parser_error_line = 0;
    l0_parser_error_col = 0;

    // Use l0_parse_string_all to parse all expressions in the string
    L0_Value* ast_list = l0_parse_string_all(arena, input_string, NULL); // Pass NULL for filename

    if (l0_parser_error_status != L0_PARSE_OK || ast_list == NULL) {
        // Parsing failed. l0_parse_string_all should have set the error message.
        // We return #f to the L0 caller to indicate failure.
        // The C caller (evaluator) will see the NULL and propagate the error status/message.
        return l0_make_boolean(arena, false);
    }

    // Parsing succeeded
    return ast_list;
}

// (codegen-program ast-list) -> string (C code) or #f on error
L0_Value* prim_codegen_program(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Not needed for codegen itself
    if (!check_arg_count(args, 1, arena, "codegen-program")) return NULL;
    L0_Value* ast_list = get_arg(args, 0, arena, "codegen-program");

    // The argument should be a list (potentially nil) representing the program AST
    if (!l0_is_list(ast_list)) { // l0_is_list checks for pair or nil
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'codegen-program': Argument must be a list (program AST).");
        return NULL; // Return NULL on type error
    }

    // Reset codegen error state (assuming codegen has its own error reporting similar to parser)
    // TODO: Define and implement codegen error reporting mechanism if not already done.
    // For now, we assume l0_codegen_program returns NULL on error.

    char* c_code = l0_codegen_program(arena, ast_list);

    if (c_code == NULL) {
        // Codegen failed. Assume error message is set by l0_codegen_program.
        // Return #f to the L0 caller.
        // TODO: Ensure l0_codegen_program sets appropriate error status/message.
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME; // Use a generic runtime error for now
        l0_parser_error_message = l0_arena_strdup(arena, "Codegen failed (internal error)."); // Placeholder message
        return l0_make_boolean(arena, false);
    }

    // Codegen succeeded, return the C code as an L0 string
    return l0_make_string(arena, c_code);
}

// --- Error Reporting Primitives ---

// (get-last-error-message) -> string or #f
L0_Value* prim_get_last_error_message(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 0, arena, "get-last-error-message")) return NULL;

    if (l0_parser_error_message) {
        // Return a copy of the message as an L0 string
        return l0_make_string(arena, l0_parser_error_message);
    } else {
        // No error message set, return #f
        return l0_make_boolean(arena, false);
    }
}

// (get-last-error-line) -> integer
L0_Value* prim_get_last_error_line(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 0, arena, "get-last-error-line")) return NULL;
    // Error line is unsigned long, cast to long for L0 integer
    return l0_make_integer(arena, (long)l0_parser_error_line);
}

// (get-last-error-col) -> integer
L0_Value* prim_get_last_error_col(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (!check_arg_count(args, 0, arena, "get-last-error-col")) return NULL;
    // Error column is unsigned long, cast to long for L0 integer
    return l0_make_integer(arena, (long)l0_parser_error_col);
}


// (eval expr) - Evaluates an L0 expression in the current environment
L0_Value* prim_eval(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    if (!check_arg_count(args, 1, arena, "eval")) return NULL;
    L0_Value* expr_to_eval = get_arg(args, 0, arena, "eval");

    // We need the actual l0_eval function from l0_eval.c
    // This requires including l0_eval.h in l0_primitives.c
    // and ensuring l0_eval is not static.
    // Forward declaration might be needed if headers are complex.
    // Assuming l0_eval.h is included and l0_eval is accessible:

    // --- Need to include l0_eval.h ---
    // Let's assume it's included for now.
    extern L0_Value* l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena); // Add extern declaration
    // --- End include assumption ---


    return l0_eval(expr_to_eval, env, arena);
}

// Forward declaration for l0_apply (defined in l0_eval.c)
// Ensure l0_eval.h includes this or l0_apply is not static.
// Corrected signature to match l0_eval.h
extern L0_Value* l0_apply(L0_Value* func, L0_Value* args, L0_Env* env, L0_Arena* arena);

// (apply func arg-list)
L0_Value* prim_apply(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    if (!check_arg_count(args, 2, arena, "apply")) return NULL;

    L0_Value* func_val = get_arg(args, 0, arena, "apply");
    L0_Value* arg_list_val = get_arg(args, 1, arena, "apply");

    // Check argument types
    if (!l0_is_closure(func_val) && !l0_is_primitive(func_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'apply': First argument must be a function (closure or primitive).");
        return NULL;
    }
    if (!l0_is_list(arg_list_val)) { // Checks for pair or nil
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'apply': Second argument must be a list.");
        return NULL;
    }

    // <<< ADDED EXPLICIT NULL CHECK >>>
    if (func_val == NULL) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'apply': First argument (function) evaluated to NULL.");
        return NULL;
    }
    // <<< END ADDED EXPLICIT NULL CHECK >>>


    // Call the core apply function from the evaluator
    // Corrected call signature: l0_apply(func, args, env, arena)
    return l0_apply(func_val, arg_list_val, env, arena);
}


// (eval-in-compiler-env expr) - Evaluates an L0 expression in the *current* (compiler's) environment
L0_Value* prim_eval_in_compiler_env(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] Entered.\n"); fflush(stderr); // DEBUG ENTRY

    if (!check_arg_count(args, 1, arena, "eval-in-compiler-env")) {
         fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] Arg count check failed.\n"); fflush(stderr);
         return NULL; // Error set by check_arg_count
    }
    L0_Value* expr_to_eval = get_arg(args, 0, arena, "eval-in-compiler-env");
    if (!expr_to_eval) {
        fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] Failed to get argument.\n"); fflush(stderr);
        // Error should have been set by get_arg if check_arg_count passed, but double check
        if (l0_parser_error_status == L0_PARSE_OK) {
             l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
             l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'eval-in-compiler-env': Internal error getting argument.");
        }
        return NULL;
    }

    fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] Expression to eval type: %d\n", expr_to_eval->type); fflush(stderr);
    // We need the actual l0_eval function from l0_eval.c
    // Ensure l0_eval.h is included and l0_eval is accessible (already done for prim_eval)
    extern L0_Value* l0_eval(L0_Value* expr, L0_Env* env, L0_Arena* arena);

    fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] Calling l0_eval...\n"); fflush(stderr);
    L0_Value* result = l0_eval(expr_to_eval, env, arena); // Use the *current* env
    fprintf(stderr, "[DEBUG PRIM eval-in-compiler-env] l0_eval returned. Result type: %d\n", result ? result->type : -1); fflush(stderr);

    // l0_eval will set error status on failure
    return result;
}

// (deref ref-val) -> value
L0_Value* prim_deref(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Not needed
    if (!check_arg_count(args, 1, arena, "deref")) return NULL;
    L0_Value* ref_val = get_arg(args, 0, arena, "deref");

    if (!l0_is_ref(ref_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'deref': Argument must be a reference (&T).");
        return NULL;
    }

    // Return the value the reference points to
    return ref_val->data.ref.referred;
}


// Helper function to copy a list structure (shallow copy of pairs)
// Returns the head of the new list, or NULL on allocation failure.
static L0_Value* l0_copy_list(L0_Arena* arena, L0_Value* list) {
    if (l0_is_nil(list)) {
        return L0_NIL;
    }
    if (!l0_is_pair(list)) {
        // Should not happen if called correctly, but handle defensively
        return NULL; // Indicate error (not a proper list to copy)
    }

    L0_Value* new_head = NULL;
    L0_Value* new_tail = NULL;
    L0_Value* current = list;

    while (l0_is_pair(current)) {
        L0_Value* elem = current->data.pair.car;
        L0_Value* new_node = l0_make_pair(arena, elem, L0_NIL); // Create new pair with original element
        if (!new_node) return NULL; // Allocation failed

        if (new_head == NULL) {
            new_head = new_node;
            new_tail = new_node;
        } else {
            new_tail->data.pair.cdr = new_node; // Link previous node
            new_tail = new_node;                // Move tail pointer
        }
        current = current->data.pair.cdr;
    }

    // Check if the original list was proper
    if (!l0_is_nil(current)) {
        // The original list was improper, the copy is also improper.
        // The last cdr of the new list should point to the same non-nil cdr.
        if (new_tail) { // Ensure list wasn't empty
             new_tail->data.pair.cdr = current;
        } else {
            // This case (improper list starting with non-pair) shouldn't be reached
            // if the initial check passed, but handle defensively.
             return NULL;
        }
    }
    // If original was proper, new_tail->data.pair.cdr is already L0_NIL.

    return new_head;
}


// (append list1 list2 ...) -> new list
L0_Value* prim_append(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env;
    if (l0_is_nil(args)) {
        return L0_NIL; // (append) -> ()
    }

    L0_Value* result_head = NULL;
    L0_Value* result_tail = NULL;
    L0_Value* current_arg_node = args;

    while (l0_is_pair(current_arg_node)) {
        L0_Value* current_list = current_arg_node->data.pair.car;
        L0_Value* next_arg_node = current_arg_node->data.pair.cdr;

        if (!l0_is_list(current_list)) { // Check if it's a proper or improper list (pair or nil)
            l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
            l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'append': All arguments must be lists.");
            return NULL;
        }

        if (l0_is_nil(current_list)) {
            // Appending an empty list does nothing to the structure yet
            current_arg_node = next_arg_node;
            continue;
        }

        // If it's the *last* argument, we don't copy it.
        if (l0_is_nil(next_arg_node)) {
            if (result_head == NULL) {
                // If this is the only non-empty list, return it directly (no copy needed)
                return current_list;
            } else {
                // Append the last list (no copy) to the tail of the result
                result_tail->data.pair.cdr = current_list;
                return result_head; // Return the head of the combined list
            }
        } else {
            // Not the last argument, copy the current list
            L0_Value* copied_list_head = l0_copy_list(arena, current_list);
            if (copied_list_head == NULL) {
                 // Check if error was allocation or improper list source for copy
                 if (l0_parser_error_status == L0_PARSE_OK) { // l0_copy_list might not set error
                    l0_parser_error_status = L0_PARSE_ERROR_MEMORY; // Assume memory error if not set
                    l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'append': Failed to copy list argument (allocation failed?).");
                 }
                 return NULL;
            }
             if (l0_is_nil(copied_list_head)) { // Copying an empty list results in nil
                 current_arg_node = next_arg_node;
                 continue;
             }


            if (result_head == NULL) {
                result_head = copied_list_head;
            } else {
                result_tail->data.pair.cdr = copied_list_head; // Link previous tail to new copy
            }

            // Find the new tail of the *copied* list
            result_tail = copied_list_head;
            while (l0_is_pair(result_tail->data.pair.cdr)) {
                result_tail = result_tail->data.pair.cdr;
            }
             // After the loop, result_tail points to the last pair of the copied segment.
             // Check if the copied list segment itself was improper. If so, error.
             // l0_copy_list should preserve improperness, but append requires proper lists except maybe the last.
             // Let's enforce proper lists for copied segments for simplicity now.
             if (!l0_is_nil(result_tail->data.pair.cdr)) {
                 l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
                 l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'append': Cannot append improper list before the last argument.");
                 return NULL;
             }
        }
        current_arg_node = next_arg_node;
    }

     if (!l0_is_nil(current_arg_node)) { // Check if the args list itself was proper
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'append': Argument list is not a proper list.");
        return NULL;
     }

    // If we exit the loop, it means all lists were empty or we handled the last one inside.
    // If result_head is still NULL, all args were empty lists.
     return (result_head != NULL) ? result_head : L0_NIL;
 }
 
// --- Macro Support Primitives ---

// (is-macro? symbol table) -> boolean
// Checks if a symbol is defined as a macro in the given table.
L0_Value* prim_is_macro_q(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Environment not needed for this version
    if (!check_arg_count(args, 2, arena, "is-macro?")) return NULL;
    L0_Value* sym_val = get_arg(args, 0, arena, "is-macro?");
    L0_Value* macro_table = get_arg(args, 1, arena, "is-macro?"); // Get table from args

    if (!l0_is_symbol(sym_val)) {
        // Macros are keyed by symbols.
        return l0_make_boolean(arena, false);
    }
    if (!l0_is_list(macro_table)) { // Check if table is a list (pair or nil)
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'is-macro?': Second argument (table) must be a list.");
        return NULL;
    }

    // Iterate through the provided macro_table (association list)
    L0_Value* current_entry_node = macro_table;
    while (l0_is_pair(current_entry_node)) {
        L0_Value* entry = current_entry_node->data.pair.car;
        // Expecting entry to be (macro-name . transformer-closure)
        if (l0_is_pair(entry) && l0_is_symbol(entry->data.pair.car)) {
            if (strcmp(entry->data.pair.car->data.symbol, sym_val->data.symbol) == 0) {
                // Found the macro name!
                return l0_make_boolean(arena, true);
            }
        } else {
             // Malformed entry in *macro-table*
             fprintf(stderr, "Warning: Malformed entry found in *macro-table* while checking is-macro?\n");
        }
        current_entry_node = current_entry_node->data.pair.cdr;
    }

    // Symbol not found in the macro table
    return l0_make_boolean(arena, false);
}

// (get-macro-transformer symbol table) -> closure or #f
// Retrieves the transformer closure associated with a macro symbol from the given table.
L0_Value* prim_get_macro_transformer(L0_Value* args, L0_Env* env, L0_Arena* arena) {
    (void)env; // Environment not needed for this version
    if (!check_arg_count(args, 2, arena, "get-macro-transformer")) return NULL;
    L0_Value* sym_val = get_arg(args, 0, arena, "get-macro-transformer");
    L0_Value* macro_table = get_arg(args, 1, arena, "get-macro-transformer"); // Get table from args

    if (!l0_is_symbol(sym_val)) {
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'get-macro-transformer': First argument must be a symbol.");
        return NULL; // Return NULL for error
    }
     if (!l0_is_list(macro_table)) { // Check if table is a list (pair or nil)
        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
        l0_parser_error_message = l0_arena_strdup(arena, "Primitive 'get-macro-transformer': Second argument (table) must be a list.");
        return NULL;
    }

    // Iterate through the provided macro_table.
    L0_Value* current_entry_node = macro_table;
    while (l0_is_pair(current_entry_node)) {
        L0_Value* entry = current_entry_node->data.pair.car;
        if (l0_is_pair(entry) && l0_is_symbol(entry->data.pair.car)) {
            if (strcmp(entry->data.pair.car->data.symbol, sym_val->data.symbol) == 0) {
                // Found the macro name! Return the transformer (cdr of the entry).
                L0_Value* transformer = entry->data.pair.cdr;
                // Basic check: is it actually a closure?
                if (!l0_is_closure(transformer)) {
                     fprintf(stderr, "Warning: Macro transformer for '%s' in *macro-table* is not a closure (type %d).\n", sym_val->data.symbol, transformer->type);
                     // Fall through to return #f, or maybe error? Let's return #f.
                     break; // Exit loop, will return #f below
                }
                return transformer;
            }
        } else {
             fprintf(stderr, "Warning: Malformed entry found in *macro-table* while getting transformer.\n");
        }
        current_entry_node = current_entry_node->data.pair.cdr;
    }

    // Macro not found or transformer was not a closure
    return l0_make_boolean(arena, false); // Return #f if not found
}

 
 // --- Primitive Registration ---
 

// --- Primitive Registration ---

// Helper to register a single primitive
static bool register_single_primitive(L0_Env* env, L0_Arena* arena, const char* name, L0_PrimitiveFunc func) {
    L0_Value* prim_symbol = l0_make_symbol(arena, name);
    if (!prim_symbol) return false; // Allocation failed

    // Pass the name to l0_make_primitive
    L0_Value* prim_value = l0_make_primitive(arena, func, name);
    if (!prim_value) return false; // Allocation failed

    return l0_env_define(env, prim_symbol, prim_value);
}

bool l0_register_primitives(L0_Env* env, L0_Arena* arena) {
    // --- Initialize Macro Table ---
    L0_Value* macro_table_sym = l0_make_symbol(arena, "*macro-table*");
    if (!macro_table_sym) {
        fprintf(stderr, "Error: Failed to create symbol '*macro-table*'.\n");
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Failed to create symbol '*macro-table*'";
        return false;
    }
    // Initialize with NIL (empty list)
    if (!l0_env_define(env, macro_table_sym, L0_NIL)) {
         fprintf(stderr, "Error: Failed to define '*macro-table*' in global env.\n");
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME; // Or maybe a specific init error?
         l0_parser_error_message = "Failed to define '*macro-table*'";
         return false;
    }
    // --- End Initialize Macro Table ---

    bool success = true; // Start success tracking *after* essential init

    // List operations
    success &= register_single_primitive(env, arena, "cons", prim_cons);
    success &= register_single_primitive(env, arena, "car", prim_car);
    success &= register_single_primitive(env, arena, "cdr", prim_cdr);
    success &= register_single_primitive(env, arena, "list", prim_list); // <<< ADDED
    success &= register_single_primitive(env, arena, "pair?", prim_pair_q);
    success &= register_single_primitive(env, arena, "null?", prim_null_q);
    success &= register_single_primitive(env, arena, "append", prim_append); // <<< Register append

    // Arithmetic operations
    success &= register_single_primitive(env, arena, "+", prim_add);
    success &= register_single_primitive(env, arena, "-", prim_subtract);
    success &= register_single_primitive(env, arena, "*", prim_multiply);
    success &= register_single_primitive(env, arena, "/", prim_divide);

    // Comparison operations
    success &= register_single_primitive(env, arena, "=", prim_equal);
    success &= register_single_primitive(env, arena, "<", prim_less_than);
    success &= register_single_primitive(env, arena, ">", prim_greater_than);

    // Type predicates
    success &= register_single_primitive(env, arena, "integer?", prim_integer_q);
    success &= register_single_primitive(env, arena, "boolean?", prim_boolean_q);
    success &= register_single_primitive(env, arena, "symbol?", prim_symbol_q);
    success &= register_single_primitive(env, arena, "string?", prim_string_q);
    success &= register_single_primitive(env, arena, "float?", prim_float_q); // <<< Register float?
    success &= register_single_primitive(env, arena, "not", prim_not); // <<< Register not
    success &= register_single_primitive(env, arena, "closure?", prim_closure_p); // <<< Register closure? (using renamed function)

    // String operations
    success &= register_single_primitive(env, arena, "string-append", prim_string_append);
    success &= register_single_primitive(env, arena, "string->symbol", prim_string_to_symbol);
    success &= register_single_primitive(env, arena, "symbol->string", prim_symbol_to_string);
    success &= register_single_primitive(env, arena, "string-length", primitive_string_length);
    success &= register_single_primitive(env, arena, "string-ref", primitive_string_ref);
    success &= register_single_primitive(env, arena, "substring", primitive_substring);
    success &= register_single_primitive(env, arena, "number->string", primitive_number_to_string); // Register number->string

    // I/O and Print
    success &= register_single_primitive(env, arena, "print", prim_print);
    success &= register_single_primitive(env, arena, "read-file", prim_read_file);
    success &= register_single_primitive(env, arena, "write-file", prim_write_file);
    success &= register_single_primitive(env, arena, "command-line-args", prim_command_line_args); // <<< Register command-line-args
    success &= register_single_primitive(env, arena, "parse-string", prim_parse_string); // <<< Register parse-string
    success &= register_single_primitive(env, arena, "codegen-program", prim_codegen_program); // <<< Register codegen-program

    // Error reporting primitives
    success &= register_single_primitive(env, arena, "get-last-error-message", prim_get_last_error_message);
    success &= register_single_primitive(env, arena, "get-last-error-line", prim_get_last_error_line);
    success &= register_single_primitive(env, arena, "get-last-error-col", prim_get_last_error_col);

    // Evaluation primitives
     success &= register_single_primitive(env, arena, "eval", prim_eval); // <<< Register eval
     success &= register_single_primitive(env, arena, "apply", prim_apply); // <<< Register apply
     success &= register_single_primitive(env, arena, "eval-in-compiler-env", prim_eval_in_compiler_env); // <<< Register eval-in-compiler-env
 
     // Macro support primitives
     success &= register_single_primitive(env, arena, "is-macro?", prim_is_macro_q);
     success &= register_single_primitive(env, arena, "get-macro-transformer", prim_get_macro_transformer);

     // Reference/Dereference primitives
     success &= register_single_primitive(env, arena, "deref", prim_deref); // <<< Register deref

     if (!success) {
          fprintf(stderr, "Error: Failed to register one or more primitives.\n");
         // Set a generic error?
         l0_parser_error_status = L0_PARSE_ERROR_RUNTIME;
         l0_parser_error_message = "Primitive registration failed";
    }

    return success;
}
