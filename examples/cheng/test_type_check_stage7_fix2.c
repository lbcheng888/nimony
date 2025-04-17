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

    // --- Initialize required global variables ---
    fprintf(stdout, "[DEBUG C main] Initializing global variables (*macro-table*, *c-declarations*, *c-exports-code*, *global-scope-id*)...\\n"); fflush(stdout);
    L0_Value* nil_list = L0_NIL; // Use the global NIL
    L0_Value* zero_int = l0_make_integer(arena, 0); // For *global-scope-id*
    if (!zero_int) { fprintf(stdout, "Failed to create zero integer for global init.\\n"); fflush(stdout); l0_arena_destroy(arena); return 1; }

    const char* global_vars[] = {"*macro-table*", "*c-declarations*", "*c-exports-code*", "*global-scope-id*"};
    L0_Value* initial_values[] = {nil_list, nil_list, nil_list, zero_int}; // *global-scope-id* starts at 0
    int num_globals = sizeof(global_vars) / sizeof(global_vars[0]);

    for (int i = 0; i < num_globals; ++i) {
        L0_Value* sym = l0_make_symbol(arena, global_vars[i]);
        if (!sym) {
            fprintf(stdout, "Failed to create symbol for global variable '%s'.\\n", global_vars[i]); fflush(stdout);
            l0_arena_destroy(arena);
            return 1;
        }
        if (!l0_env_define(env, sym, initial_values[i])) {
            fprintf(stdout, "Failed to define global variable '%s'.\\n", global_vars[i]); fflush(stdout);
            // Error message might be set by l0_env_define if symbol already exists, though it shouldn't here.
            l0_arena_destroy(arena);
            return 1;
        }
        fprintf(stdout, "[DEBUG C main] Defined global '%s'.\\n", global_vars[i]); fflush(stdout);
    }
    fprintf(stdout, "[DEBUG C main] Global variables initialized.\\n"); fflush(stdout);
    // --- End Global Variable Initialization ---

    L0_Value* last_result = L0_NIL; // Initialize
    L0_Value* temp_result = NULL; // For individual expression results
    int exit_code = 0; // Default success
    (void)last_result; // Avoid unused warning for now

    // --- Block 1: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 1...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_make_integer(arena, 10L); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "x"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    { L0_Value* define_val = l0_make_boolean(arena, true); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "y"), define_val); } else { /* Value evaluation failed */ } }
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
    { L0_Value* define_val = l0_make_string(arena, "hello"); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "z"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    { L0_Value* define_val = l0_make_float(arena, 3.14); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "f"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    { L0_Value* define_val = L0_NIL; if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "n"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    { L0_Value* define_val = l0_make_symbol(arena, "sym"); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "s"), define_val); } else { /* Value evaluation failed */ } }
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
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), L0_NIL), env, arena);
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
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "y")), L0_NIL), env, arena);
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
    // --- Block 9: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 9...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "z")), L0_NIL), env, arena);
    if (9 != 9) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 9, got %d\n", 9); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 10: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 10...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "f")), L0_NIL), env, arena);
    if (10 != 10) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 10, got %d\n", 10); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 11: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 11...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "n")), L0_NIL), env, arena);
    if (11 != 11) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 11, got %d\n", 11); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 12: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 12...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "s")), L0_NIL), env, arena);
    if (12 != 12) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 12, got %d\n", 12); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 13: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 13...\n"); fflush(stdout); // DEBUG
    temp_result = ({ L0_Value* cond_val = l0_env_lookup(env, l0_make_symbol(arena, "y")); L0_Value* if_res = L0_NIL; if (L0_IS_TRUTHY(cond_val)) { if_res = l0_env_lookup(env, l0_make_symbol(arena, "x")); } else { if_res = l0_make_integer(arena, 5L); } if_res; });
    if (13 != 13) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 13, got %d\n", 13); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 14: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 14...\n"); fflush(stdout); // DEBUG
    temp_result = ({ L0_Value* cond_val = prim_less_than(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL)), env, arena); L0_Value* if_res = L0_NIL; if (L0_IS_TRUTHY(cond_val)) { if_res = l0_make_string(arena, "a"); } else { if_res = l0_make_string(arena, "b"); } if_res; });
    if (14 != 14) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 14, got %d\n", 14); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 15: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 15...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = ({ L0_Value* lambda_params = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "a"), l0_make_pair(arena, l0_make_symbol(arena, "int"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "b"), l0_make_pair(arena, l0_make_symbol(arena, "int"), L0_NIL)), L0_NIL)); L0_Value* lambda_body = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "+"), l0_make_pair(arena, l0_make_symbol(arena, "a"), l0_make_pair(arena, l0_make_symbol(arena, "b"), L0_NIL))), L0_NIL); l0_make_closure(arena, lambda_params, lambda_body, env); }); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "add"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (15 != 15) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 15, got %d\n", 15); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 16: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 16...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = ({ L0_Value* lambda_params = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "name"), l0_make_pair(arena, l0_make_symbol(arena, "string"), L0_NIL)), L0_NIL); L0_Value* lambda_body = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "string-append"), l0_make_pair(arena, l0_make_string(arena, "Hello, "), l0_make_pair(arena, l0_make_symbol(arena, "name"), L0_NIL))), L0_NIL); l0_make_closure(arena, lambda_params, lambda_body, env); }); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "greet"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (16 != 16) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 16, got %d\n", 16); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 17: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 17...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = ({ L0_Value* lambda_params = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "p"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "ref"), l0_make_pair(arena, l0_make_symbol(arena, "int"), L0_NIL)), L0_NIL)), L0_NIL); L0_Value* lambda_body = l0_make_pair(arena, l0_make_symbol(arena, "p"), L0_NIL); l0_make_closure(arena, lambda_params, lambda_body, env); }); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "returns-ref"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (17 != 17) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 17, got %d\n", 17); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 18: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 18...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "add")), l0_make_pair(arena, l0_make_integer(arena, 1L), l0_make_pair(arena, l0_make_integer(arena, 2L), L0_NIL)), env, arena), L0_NIL), env, arena);
    if (18 != 18) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 18, got %d\n", 18); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 19: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 19...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "greet")), l0_make_pair(arena, l0_make_string(arena, "world"), L0_NIL), env, arena), L0_NIL), env, arena);
    if (19 != 19) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 19, got %d\n", 19); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 20: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 20...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, l0_apply(l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "a")), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "b")), l0_make_pair(arena, l0_make_string(arena, "hi"), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), l0_make_pair(arena, prim_add(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), l0_make_pair(arena, l0_make_integer(arena, 10L), L0_NIL)), env, arena), L0_NIL)))), env, arena);
    if (20 != 20) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 20, got %d\n", 20); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 21: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 21...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, l0_apply(l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "outer")), l0_make_pair(arena, l0_make_integer(arena, 100L), L0_NIL), env, arena), L0_NIL, env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, l0_apply(l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "inner")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "outer")), L0_NIL), env, arena), L0_NIL, env, arena), l0_make_pair(arena, prim_add(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "inner")), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL)), env, arena), L0_NIL)), env, arena), L0_NIL)), env, arena);
    if (21 != 21) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 21, got %d\n", 21); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 22: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 22...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_symbol(arena, "abc"), L0_NIL), env, arena);
    if (22 != 22) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 22, got %d\n", 22); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 23: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 23...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_pair(arena, l0_make_integer(arena, 1L), l0_make_pair(arena, l0_make_integer(arena, 2L), l0_make_pair(arena, l0_make_integer(arena, 3L), L0_NIL))), L0_NIL), env, arena);
    if (23 != 23) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 23, got %d\n", 23); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 24: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 24...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "string"), L0_NIL), env, arena);
    if (24 != 24) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 24, got %d\n", 24); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 25: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 25...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_integer(arena, 10L), L0_NIL), env, arena);
    if (25 != 25) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 25, got %d\n", 25); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 26: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 26...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_make_integer(arena, 1L); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "count"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (26 != 26) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 26, got %d\n", 26); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 27: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 27...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "ref")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "count")), L0_NIL), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "count-ref"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (27 != 27) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 27, got %d\n", 27); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 28: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 28...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "deref")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "count-ref")), L0_NIL), env, arena), L0_NIL), env, arena);
    if (28 != 28) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 28, got %d\n", 28); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 29: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 29...\n"); fflush(stdout); // DEBUG
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, l0_apply(l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "local-val")), l0_make_pair(arena, l0_make_integer(arena, 50L), L0_NIL), env, arena), L0_NIL, env, arena), l0_make_pair(arena, { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "ref")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "local-val")), L0_NIL), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "local-ref"), define_val); } else { /* Value evaluation failed */ } }, l0_make_pair(arena, prim_print(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "deref")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "local-ref")), L0_NIL), env, arena), L0_NIL), env, arena), L0_NIL))), env, arena);
    if (29 != 29) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 29, got %d\n", 29); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 30: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 30...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_make_integer(arena, 100L); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "bv"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (30 != 30) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 30, got %d\n", 30); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 31: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 31...\n"); fflush(stdout); // DEBUG
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "ref")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "bv")), L0_NIL), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "bv-ref"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (31 != 31) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 31, got %d\n", 31); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 32: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 32...\n"); fflush(stdout); // DEBUG
    { L0_Value* lambda_val = ({ L0_Value* lambda_params = L0_NIL; L0_Value* lambda_body = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "let"), l0_make_pair(arena, l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "x"), l0_make_pair(arena, l0_make_integer(arena, 5L), L0_NIL)), L0_NIL), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "ref"), l0_make_pair(arena, l0_make_symbol(arena, "x"), L0_NIL)), L0_NIL))), L0_NIL); l0_make_closure(arena, lambda_params, lambda_body, env); }); if (lambda_val != NULL) { (void)l0_env_define(env, l0_make_symbol(arena, "bad-lifetime"), lambda_val); } else { /* Lambda creation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
    if (32 != 32) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 32, got %d\n", 32); fflush(stdout); exit(99); } // Check block_num integrity
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stdout, "[DEBUG C main] Error encountered after processing a block.\n"); fflush(stdout); // DEBUG
        fprintf(stdout, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)"); fflush(stdout);
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    fprintf(stdout, "[DEBUG C main] Block finished successfully.\n"); fflush(stdout); // DEBUG
    // --- Block 33: Processing top-level expression ---
    fprintf(stdout, "[DEBUG C main] Executing Block 33...\n"); fflush(stdout); // DEBUG
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Type check test file finished processing (if no errors printed)."), L0_NIL), env, arena);
    if (33 != 33) { fprintf(stdout, "[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected 33, got %d\n", 33); fflush(stdout); exit(99); } // Check block_num integrity
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
    // --- MODIFIED: Don't print L0 result, just use exit_code --- 
    // if (last_result == NULL) {
    //     printf("Result: <RUNTIME_ERROR>\n");
    // } else {
    //     char print_buffer[1024];
    //     l0_value_to_string_recursive(last_result, print_buffer, sizeof(print_buffer), arena, 0);
    //     printf("Result: %s\n", print_buffer);
    // }
    // --- END MODIFICATION --- 

    fprintf(stdout, "[DEBUG C main] Reached cleanup. exit_code = %d\n", exit_code); fflush(stdout);
    l0_arena_destroy(arena); 
    fprintf(stdout, "[DEBUG C main] Arena destroyed. Returning %d\n", exit_code); fflush(stdout);
    return exit_code;
}

// Note: l0_value_to_string_recursive is defined in l0_primitives.c and should be linked.
