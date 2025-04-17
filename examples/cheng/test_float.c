#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "l0_arena.h" // Include first for l0_arena_alloc_type etc.
#include "l0_types.h" // Defines L0_Value, L0_NIL etc.
#include "l0_parser.h" // Needed for error status/message
#include "l0_env.h"   // Defines L0_Env, l0_env_create etc.
#include "l0_primitives.h" // Declares prim_add etc.
#include "l0_eval.h" // Needed for potential closure application (though not used in basic codegen)

// Make argc/argv globally accessible for the command-line-args primitive
// (Defined in l0_primitives.c)
extern int g_argc;
extern char **g_argv;

// Forward declare print helper (implementation might be linked separately or included)
// static int l0_value_to_string_recursive(L0_Value* value, char* buffer, size_t buf_size, L0_Arena* arena, int depth);

int main(int argc, char *argv[]) {
    // Store argc/argv globally for command-line-args primitive
    g_argc = argc;
    g_argv = argv;

    // --- DEBUG PRINT ADDED ---
    fprintf(stderr, "[DEBUG C main] argc = %d\n", argc);
    for (int i = 0; i < argc; ++i) {
        fprintf(stderr, "[DEBUG C main] argv[%d] = %s\n", i, argv[i]);
    }
    fflush(stderr);
    // --- END DEBUG PRINT ---

    L0_Arena* arena = l0_arena_create(1024 * 1024); // 1MB initial size
    if (!arena) { fprintf(stderr, "Failed to create memory arena.\n"); return 1; }

    L0_Env* env = l0_env_create(arena, NULL);
    if (!env) { fprintf(stderr, "Failed to create global environment.\n"); l0_arena_destroy(arena); return 1; }

    if (!l0_register_primitives(env, arena)) {
         fprintf(stderr, "Failed to register primitives.\n");
         l0_arena_destroy(arena);
         return 1;
    }

    L0_Value* last_result = L0_NIL; // Initialize
    L0_Value* temp_result = NULL; // For individual expression results
    int exit_code = 0; // Default success
    (void)last_result; // Avoid unused warning for now

    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "--- Float Test ---"), L0_NIL), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = ({ L0_Value* define_val = l0_make_float(arena, 3.14159); L0_Value* define_res; if (define_val == NULL || l0_parser_error_status != L0_PARSE_OK) { define_res = NULL; } else { (void)l0_env_define(env, l0_make_symbol(arena, "pi"), define_val); define_res = L0_NIL; } define_res; });
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = ({ L0_Value* define_val = l0_make_float(arena, 10); L0_Value* define_res; if (define_val == NULL || l0_parser_error_status != L0_PARSE_OK) { define_res = NULL; } else { (void)l0_env_define(env, l0_make_symbol(arena, "radius"), define_val); define_res = L0_NIL; } define_res; });
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = ({ L0_Value* define_val = prim_multiply(l0_make_pair(arena, l0_make_float(arena, 2), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "pi")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "radius")), L0_NIL))), env, arena); L0_Value* define_res; if (define_val == NULL || l0_parser_error_status != L0_PARSE_OK) { define_res = NULL; } else { (void)l0_env_define(env, l0_make_symbol(arena, "circumference"), define_val); define_res = L0_NIL; } define_res; });
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Pi:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "pi")), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Radius:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "radius")), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Circumference:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "circumference")), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Is pi a float?"), l0_make_pair(arena, prim_float_q(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "pi")), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Is radius a float?"), l0_make_pair(arena, prim_float_q(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "radius")), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Is 10 an integer?"), l0_make_pair(arena, prim_integer_q(l0_make_pair(arena, l0_make_integer(arena, 10L), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Is 10.0 a float?"), l0_make_pair(arena, prim_float_q(l0_make_pair(arena, l0_make_float(arena, 10), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Is 10.0 an integer?"), l0_make_pair(arena, prim_integer_q(l0_make_pair(arena, l0_make_float(arena, 10), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Simple float addition:"), l0_make_pair(arena, prim_add(l0_make_pair(arena, l0_make_float(arena, 1.5), l0_make_pair(arena, l0_make_float(arena, 2.5), L0_NIL)), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Simple float subtraction:"), l0_make_pair(arena, prim_subtract(l0_make_pair(arena, l0_make_float(arena, 5), l0_make_pair(arena, l0_make_float(arena, 1.5), L0_NIL)), env, arena), L0_NIL)), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "--- Float Test End ---"), L0_NIL), env, arena);
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result

cleanup: // Label for jumping on error
    // Print the result of the last successful expression, or indicate error
    if (last_result == NULL) {
        // Error already printed above
        printf("Result: <RUNTIME_ERROR>\n");
    } else {
        char print_buffer[1024];
        // Assuming l0_value_to_string_recursive is available
        l0_value_to_string_recursive(last_result, print_buffer, sizeof(print_buffer), arena, 0);
        printf("Result: %s\n", print_buffer);
    }

    l0_arena_destroy(arena);
    return exit_code;
}

// Note: l0_value_to_string_recursive is defined in l0_primitives.c and should be linked.
