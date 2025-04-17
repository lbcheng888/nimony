#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "l0_arena.h" // Include first for l0_arena_alloc_type etc.
#include "l0_types.h" // Defines L0_Value, L0_NIL etc.
#include "l0_env.h"   // Defines L0_Env, l0_env_create etc.
#include "l0_primitives.h" // Declares prim_add etc.
#include "l0_eval.h" // Needed for potential closure application (though not used in basic codegen)

// Forward declare print helper (implementation might be linked separately or included)
// static int l0_value_to_string_recursive(L0_Value* value, char* buffer, size_t buf_size, L0_Arena* arena, int depth);

int main(int argc, char *argv[]) {
    (void)argc; (void)argv; // Unused for now

    L0_Arena* arena = l0_arena_create(1024 * 1024); // 1MB initial size
    if (!arena) { fprintf(stderr, "Failed to create memory arena.\n"); return 1; }

    L0_Env* env = l0_env_create(arena, NULL);
    if (!env) { fprintf(stderr, "Failed to create global environment.\n"); l0_arena_destroy(arena); return 1; }

    if (!l0_register_primitives(env, arena)) {
         fprintf(stderr, "Failed to register primitives.\n");
         l0_arena_destroy(arena);
         return 1;
    }

    L0_Value* last_result = L0_NIL;
    (void)last_result; // Avoid unused warning

    last_result = ((l0_env_define(env, l0_make_symbol(arena, "compile-l0"), l0_make_closure(arena, l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL)), l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Reading input file: "), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "source-content"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "read-file"), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "boolean?"), l0_make_pair(arena, l0_make_symbol(arena, "source-content"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Could not read input file "), l0_make_pair(arena, l0_make_symbol(arena, "input-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "begin"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Parsing L0 code..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "quote"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "x"), l0_make_pair(arena, l0_make_integer(arena, 10L), L0_NIL))), L0_NIL)), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Parsing successful (placeholder). AST:"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_symbol(arena, "ast"), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Generating C code..."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "c-code"), l0_make_pair(arena, l0_make_string(arena, "// Placeholder C code
int main() { return 0; }
"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Code generation successful (placeholder)."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Writing output file: "), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "define"), l0_make_pair(arena, l0_make_symbol(arena, "write-ok"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "write-file"), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), l0_make_pair(arena, l0_make_symbol(arena, "c-code"), L0_NIL))), L0_NIL))), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "if"), l0_make_pair(arena, l0_make_symbol(arena, "write-ok"), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Output written successfully."), L0_NIL)), l0_make_pair(arena, l0_make_pair(arena, l0_make_symbol(arena, "print"), l0_make_pair(arena, l0_make_string(arena, "Error: Could not write output file "), l0_make_pair(arena, l0_make_symbol(arena, "output-filename"), L0_NIL))), L0_NIL)))), L0_NIL))))))))))), L0_NIL)))), L0_NIL)))), env)), L0_NIL));
    last_result = /* ERROR: Unknown function or primitive: compile-l0 */;
    last_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "L0 Compiler finished."), L0_NIL), env, arena);

    // Print the result of the last expression
    char print_buffer[1024];
    if (last_result != L0_NIL) { 
       // Assuming l0_value_to_string_recursive is available (defined in l0_primitives.c)
       l0_value_to_string_recursive(last_result, print_buffer, sizeof(print_buffer), arena, 0);
       printf("Result: %s\n", print_buffer);
    }

    l0_arena_destroy(arena);
    return 0;
}

// Note: l0_value_to_string_recursive is defined in l0_primitives.c and should be linked.
