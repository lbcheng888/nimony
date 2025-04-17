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

    last_result = ((l0_env_define(env, l0_make_symbol(arena, "x"), l0_make_integer(arena, 10L)), L0_NIL));
    last_result = ((l0_env_define(env, l0_make_symbol(arena, "y"), l0_make_integer(arena, 5L)), L0_NIL));
    last_result = prim_add(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "x")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "y")), L0_NIL)), env, arena);

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
