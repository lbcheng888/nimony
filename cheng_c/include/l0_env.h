#ifndef L0_ENV_H
#define L0_ENV_H

#include "l0_types.h"
#include "l0_arena.h"

// Environment Structure
// Represents a lexical environment as a linked list of frames.
// Each frame is an association list: a list of (symbol . value) pairs.
// Using L0_Value pairs directly for the association list.
typedef struct L0_Env {
    L0_Value* frame; // Association list for the current frame: ((sym1 . val1) (sym2 . val2) ...)
    struct L0_Env* outer; // Pointer to the enclosing environment
    L0_Arena* arena; // Arena used for allocations within this environment scope (optional, could use global)
} L0_Env;

/**
 * @brief Creates a new, empty environment (typically the global environment).
 * @param arena The memory arena to use for allocations.
 * @param outer The enclosing environment (NULL for the global environment).
 * @return A pointer to the newly created environment, or NULL on allocation failure.
 */
L0_Env* l0_env_create(L0_Arena* arena, L0_Env* outer);

/**
 * @brief Looks up a symbol in the environment chain.
 * Searches the current frame first, then recursively searches outer environments.
 * @param env The environment to start searching from.
 * @param symbol The symbol (as an L0_Value of type L0_TYPE_SYMBOL) to look up.
 * @return A pointer to the L0_Value bound to the symbol, or NULL if not found.
 */
L0_Value* l0_env_lookup(L0_Env* env, L0_Value* symbol);

/**
 * @brief Defines a variable in the *current* environment frame.
 * If the variable already exists in the current frame, its value is updated.
 * @param env The environment whose current frame will be modified.
 * @param symbol The symbol (L0_TYPE_SYMBOL) to define.
 * @param value The value to bind to the symbol.
 * @return True if the definition was successful, false otherwise (e.g., allocation failure).
 *         Note: This simple version doesn't prevent redefining existing variables.
 */
bool l0_env_define(L0_Env* env, L0_Value* symbol, L0_Value* value);

/**
 * @brief Sets an existing variable in the environment chain.
 * Searches for the variable starting from the current frame up to the global scope.
 * If found, updates the binding in the frame where it was found.
 * If not found, it's an error.
 * @param env The environment to start searching from.
 * @param symbol The symbol (L0_TYPE_SYMBOL) whose binding to set.
 * @param value The new value.
 * @return True if the variable was found and set, false otherwise (variable not found).
 */
bool l0_env_set(L0_Env* env, L0_Value* symbol, L0_Value* value); // For later, if needed for set!

/**
 * @brief Creates a new environment that extends an existing one.
 * Useful for creating local scopes (e.g., for let or function calls).
 * The new environment's outer pointer points to the given environment.
 * @param outer_env The environment to extend.
 * @return A pointer to the newly created extended environment, or NULL on failure.
 *         The new frame starts empty. Bindings are added using l0_env_define.
 */
L0_Env* l0_env_extend(L0_Env* outer_env);


#endif // L0_ENV_H
