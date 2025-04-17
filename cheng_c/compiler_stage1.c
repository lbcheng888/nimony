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

    temp_result = ({ L0_Value* lambda_val = ({ L0_Value* lambda_params = l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL)); L0_Value* lambda_body = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Reading input file: "), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "source-content"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "read-file"), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "boolean?"), l0_make_pair(arena, l0_make_symbol(arena, "source-content"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Could not read input file "), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "read-error"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Parsing L0 code..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "parse-string"), l0_make_pair(arena, l0_make_symbol(arena, "source-content"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "boolean?"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Parsing failed."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "parse-error"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Parsing successful. AST:"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Generating C code..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "c-code"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "codegen-program"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "boolean?"), l0_make_pair(arena, l0_make_symbol(arena, "c-code"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Code generation failed."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "codegen-error"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Code generation successful."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Writing output file: "), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "write-ok"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "write-file"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), l0_make_pair(arena, l0_make_symbol(arena, "c-code"), L0_NIL))), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_symbol(arena, "write-ok"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Output written successfully."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "success"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Could not write output file "), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "write-error"), L0_NIL)), L0_NIL))), L0_NIL)))), L0_NIL))))), L0_NIL)))), L0_NIL)))))), L0_NIL)))), L0_NIL)))), L0_NIL)))), L0_NIL))); l0_make_closure(arena, lambda_params, lambda_body, env); }); L0_Value* define_res; if (lambda_val == NULL) { define_res = NULL; } else { (void)l0_env_define(env, l0_make_symbol(arena, "compile-l0"), lambda_val); define_res = L0_NIL; } define_res; });
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = ({ L0_Value* define_val = ({ L0_Value* lambda_params = L0_NIL; L0_Value* lambda_body = l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "L0 Compiler (Stage 1) starting..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Attempting early read-file test..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "test-content"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "read-file"), l0_make_pair(arena, l0_make_string(arena, "src/l0_compiler/compiler.l0"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "boolean?"), l0_make_pair(arena, l0_make_symbol(arena, "test-content"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Early read-file FAILED."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Early read-file SUCCEEDED. Content length (approx): "), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "string-length"), l0_make_pair(arena, l0_make_symbol(arena, "test-content"), L0_NIL)), L0_NIL))), L0_NIL)))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "--- End of early test ---"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "args"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "command-line-args"), L0_NIL), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Command line args received:"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "result"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "init-error"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "and"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "pair?"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "pair?"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "cdr"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL)), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "null?"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "cdr"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "cdr"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL)), L0_NIL)), L0_NIL)), L0_NIL)))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "car"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "car"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "cdr"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL)), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "and"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "string?"), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "string?"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "set!"), l0_make_pair(arena, l0_make_symbol(arena, "result"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "compile-l0"), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL))), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Command line arguments must be strings (filenames)."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "set!"), l0_make_pair(arena, l0_make_symbol(arena, "result"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "arg-type-error"), L0_NIL)), L0_NIL))), L0_NIL))), L0_NIL)))), L0_NIL)))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Expected exactly two command line arguments (input-file output-file), got:"), l0_make_pair(arena, l0_make_symbol(arena, "args"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "set!"), l0_make_pair(arena, l0_make_symbol(arena, "result"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_symbol(arena, "arg-count-error"), L0_NIL)), L0_NIL))), L0_NIL))), L0_NIL)))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "L0 Compiler finished with result: "), l0_make_pair(arena, l0_make_symbol(arena, "result"), L0_NIL))), l0_make_pair(arena, l0_make_symbol(arena, "result"), L0_NIL))))))))))); l0_make_closure(arena, lambda_params, lambda_body, env); }); L0_Value* define_res; if (define_val == NULL || l0_parser_error_status != L0_PARSE_OK) { define_res = NULL; } else { (void)l0_env_define(env, l0_make_symbol(arena, "main"), define_val); define_res = L0_NIL; } define_res; });
    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {
        // Error occurred during evaluation
        fprintf(stderr, "Runtime Error: %s\n", l0_parser_error_message ? l0_parser_error_message : "(unknown)");
        last_result = NULL; // Mark overall result as error
        exit_code = 1;
        goto cleanup; // Skip remaining expressions
    }
    last_result = temp_result; // Store successful result
    temp_result = l0_apply(l0_env_lookup(env, l0_make_symbol(arena, "main")), L0_NIL, env, arena);
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
