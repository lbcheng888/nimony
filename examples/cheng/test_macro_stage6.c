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

    L0_Arena* arena = l0_arena_create(1024 * 1024); // 1MB initial size
    if (!arena) { fprintf(stdout, "Failed to create memory arena.\n"); fflush(stdout); return 1; }

    L0_Env* env = l0_env_create(arena, NULL);
    if (!env) { fprintf(stdout, "Failed to create global environment.\n"); fflush(stdout); l0_arena_destroy(arena); return 1; }

    if (!l0_register_primitives(env, arena)) {
         fprintf(stdout, "Failed to register primitives.\n"); fflush(stdout);
         l0_arena_destroy(arena);
         return 1;
    }

    L0_Value* last_result = L0_NIL; // Initialize
    L0_Value* temp_result = NULL; // For individual expression results
    int exit_code = 0; // Default success
    (void)last_result; // Avoid unused warning for now

    // --- Block 1: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 1...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "when")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "condition")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "body")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "list")), l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "condition")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "body")), l0_make_pair(arena, l0_make_boolean(arena, false), L0_NIL)))), env, arena), L0_NIL))), env, arena);
    if (1 != 1) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 1, got %d\n", 1); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 2: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 2...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_make_integer(arena, 10L); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "x"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (2 != 2) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 2, got %d\n", 2); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 3: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 3...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "when")), l0_make_pair(arena, prim_greater_than(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL)), env, arena), l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_make_string(arena, "x is greater than 5"), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (3 != 3) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 3, got %d\n", 3); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 4: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 4...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "when")), l0_make_pair(arena, prim_less_than(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL)), env, arena), l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_make_string(arena, "x is less than 5"), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (4 != 4) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 4, got %d\n", 4); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 5: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 5...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "when")), l0_make_pair(arena, l0_make_boolean(arena, false), l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_make_string(arena, "This should not print"), L0_NIL), env, arena), L0_NIL)), env, arena);
    if (5 != 5) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 5, got %d\n", 5); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 6: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 6...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "when")), l0_make_pair(arena, prim_equal(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_make_integer(arena, 10L), L0_NIL)), env, arena), l0_make_pair(arena, l0_make_integer(arena, 100L), L0_NIL)), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "y"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (6 != 6) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 6, got %d\n", 6); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 7: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 7...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "y is:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "y")), L0_NIL)), env, arena);
    if (7 != 7) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 7, got %d\n", 7); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 8: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 8...\n"); fflush(stdout); // DEBUG
    temp_result = l0_make_symbol(arena, "macro-test-complete");
    if (8 != 8) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 8, got %d\n", 8); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG

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
