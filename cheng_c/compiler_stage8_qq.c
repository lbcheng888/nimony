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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "let*")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "bindings")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "body")), L0_NIL), env, arena), l0_make_pair(arena, ({ L0_Value* cond_val = prim_null_q(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "bindings")), L0_NIL), env, arena); L0_Value* if_res = L0_NIL; if (L0_IS_TRUTHY(cond_val)) { if_res = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, (l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "body")), L0_NIL), env, arena)), L0_NIL), env, arena); } else { if_res = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, /* ERROR: Operator in call must be a symbol */, l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let")), l0_make_pair(arena, /* ERROR: Operator in call must be a symbol */, l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let*")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "rest-bindings")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "body")), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL)), env, arena); } if_res; }), L0_NIL))), env, arena);
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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "assert-equal")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "a")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, ({ L0_Value* cond_val = prim_equal(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), L0_NIL)), env, arena); L0_Value* if_res = L0_NIL; if (L0_IS_TRUTHY(cond_val)) { if_res = prim_print(l0_make_pair(arena, l0_make_string(arena, "Assertion passed:"), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, l0_make_string(arena, "="), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, l0_make_string(arena, "->"), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), L0_NIL)))))), env, arena); } else { if_res = prim_print(l0_make_pair(arena, l0_make_string(arena, "Assertion FAILED:"), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, l0_make_string(arena, "!="), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, l0_make_string(arena, "->"), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), l0_make_pair(arena, l0_make_string(arena, "vs"), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), L0_NIL)))))))), env, arena); } if_res; }), L0_NIL), env, arena), L0_NIL))), env, arena);
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
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "let*")), l0_make_pair(arena, /* ERROR: Operator in call must be a symbol */, l0_make_pair(arena, prim_add(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "y")), L0_NIL)), env, arena), L0_NIL)), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "result1"), define_val); } else { /* Value evaluation failed */ } }
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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "assert-equal")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "result1")), l0_make_pair(arena, l0_make_integer(arena, 25L), L0_NIL)), env, arena);
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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "make-list-macro")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "prefix")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, ".")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "items")), L0_NIL)), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "prefix")), L0_NIL), env, arena), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote-splicing")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "items")), L0_NIL), env, arena), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL))), env, arena);
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
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "make-list-macro")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "data")), L0_NIL), env, arena), l0_make_pair(arena, l0_make_integer(arena, 10L), l0_make_pair(arena, l0_make_integer(arena, 20L), l0_make_pair(arena, l0_make_integer(arena, 30L), L0_NIL)))), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "result2"), define_val); } else { /* Value evaluation failed */ } }
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
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Result 2:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "result2")), L0_NIL)), env, arena);
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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "nested-qq")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "a")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "outer")), L0_NIL), env, arena), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "inner")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), L0_NIL), env, arena), L0_NIL), env, arena), L0_NIL)), env, arena), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL))), env, arena);
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
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "nested-qq")), l0_make_pair(arena, prim_add(l0_make_pair(arena, l0_make_integer(arena, 1L), l0_make_pair(arena, l0_make_integer(arena, 1L), L0_NIL)), env, arena), l0_make_pair(arena, l0_make_integer(arena, 3L), L0_NIL)), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "result3"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Result 3:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "result3")), L0_NIL)), env, arena);
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
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "defmacro")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "nested-unquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "a")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "b")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "outer")), L0_NIL), env, arena), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "inner")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, prim_list(l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "quasiquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "level2")), L0_NIL), env, arena), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "a")), L0_NIL), env, arena), L0_NIL), env, arena), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL)), env, arena), L0_NIL)), env, arena), L0_NIL), env, arena), L0_NIL))), env, arena);
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
    { L0_Value* define_val = l0_make_integer(arena, 5L); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "x"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    { L0_Value* define_val = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "nested-unquote")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_make_integer(arena, 99L), L0_NIL)), env, arena); if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { (void)l0_env_define(env, l0_make_symbol(arena, "result4"), define_val); } else { /* Value evaluation failed */ } }
    temp_result = L0_NIL; // Define returns NIL
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
    temp_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Result 4:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "result4")), L0_NIL)), env, arena);
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
