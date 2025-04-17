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

    last_result = ({ L0_Value* define_val = l0_make_string(arena, "cheng_c/tests/io_output_test.txt"); l0_env_define(env, l0_make_symbol(arena, "test-filename"), define_val); L0_NIL; });
    last_result = ({ L0_Value* define_val = l0_make_string(arena, "Hello from L0 file IO test!"); l0_env_define(env, l0_make_symbol(arena, "test-content"), define_val); L0_NIL; });
    last_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "Attempting to write to:"), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "test-filename")), L0_NIL)), env, arena);
    last_result = ({ L0_Value* define_val = prim_write_file(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "test-filename")), l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "test-content")), L0_NIL)), env, arena); l0_env_define(env, l0_make_symbol(arena, "write-result"), define_val); L0_NIL; });
    last_result = (l0_is_truthy(l0_env_lookup(env, l0_make_symbol(arena, "write-result"))) ? ((prim_print(l0_make_pair(arena, l0_make_string(arena, "Write successful. Attempting to read back..."), L0_NIL), env, arena), ({ L0_Value* define_val = prim_read_file(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "test-filename")), L0_NIL), env, arena); l0_env_define(env, l0_make_symbol(arena, "read-content"), define_val); L0_NIL; }), (l0_is_truthy(prim_boolean_q(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "read-content")), L0_NIL), env, arena)) ? (prim_print(l0_make_pair(arena, l0_make_string(arena, "Error reading file back."), L0_NIL), env, arena)) : ((prim_print(l0_make_pair(arena, l0_make_string(arena, "Read successful. Content:"), L0_NIL), env, arena), prim_print(l0_make_pair(arena, l0_env_lookup(env, l0_make_symbol(arena, "read-content")), L0_NIL), env, arena)))))) : (prim_print(l0_make_pair(arena, l0_make_string(arena, "Error writing file."), L0_NIL), env, arena)));
    last_result = prim_print(l0_make_pair(arena, l0_make_string(arena, "IO Test finished."), L0_NIL), env, arena);

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
