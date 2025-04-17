#ifndef L0_CODEGEN_H
#define L0_CODEGEN_H

#include "l0_types.h"
#include "l0_arena.h"

/**
 * @brief Generates C code from a list of L0 AST expressions.
 *
 * Takes an L0 list where each element is a parsed L0 expression (AST).
 * Generates a complete, compilable C program string that, when run,
 * will execute the logic defined by the input L0 expressions.
 *
 * The generated C code relies on the L0 runtime functions (l0_types, l0_env, l0_primitives).
 *
 * @param arena The memory arena used for allocating the generated C code string
 *              and any intermediate strings during generation.
 * @param ast_list An L0_Value representing a proper list of L0 AST expressions.
 * @return A pointer to a null-terminated C code string allocated within the arena,
 *         or NULL if code generation fails (e.g., memory allocation error, invalid AST).
 *         Error details might be set globally or returned via another mechanism if needed.
 */
char* l0_codegen_program(L0_Arena* arena, L0_Value* ast_list);

#endif // L0_CODEGEN_H
