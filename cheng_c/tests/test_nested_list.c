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
    temp_result = l0_make_pair(arena, l0_make_symbol(arena, "a"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "b"), L0_NIL), L0_NIL));
    if (1 != 1) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 1, got %d\n", 1); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error after Block %d.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block %d finished successfully.\n"); fflush(stdout); // DEBUG

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
