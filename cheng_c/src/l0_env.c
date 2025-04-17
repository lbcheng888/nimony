#include "l0_env.h"
#include "l0_types.h"
#include "l0_arena.h"
#include "l0_parser.h" // Include parser header for error status/message
#include <string.h> // For strcmp
#include <assert.h> // For assert
#include <stdio.h>  // For error printing (optional)

L0_Env* l0_env_create(L0_Arena* arena, L0_Env* outer) {
    if (!arena) {
        // Cannot create environment without an arena
        fprintf(stderr, "Error: Cannot create environment without an arena.\n");
        return NULL;
    }
    // Allocate the environment struct itself within the arena
    L0_Env* env = (L0_Env*)l0_arena_alloc(arena, sizeof(L0_Env), alignof(L0_Env));
    if (!env) {
        fprintf(stderr, "Error: Failed to allocate environment struct in arena.\n");
        return NULL;
    }

    // Initialize the frame to an empty list (NIL) allocated in the arena
    env->frame = l0_make_nil(arena);
    if (!env->frame) {
         fprintf(stderr, "Error: Failed to allocate initial NIL frame in arena.\n");
         // Note: The env struct itself is already allocated and won't be freed here,
         // relying on arena reset/destroy for cleanup. This might leak the env struct
         // if the caller doesn't handle the NULL return properly with arena reset.
         // A more robust approach might involve a dedicated arena per env or careful cleanup.
         return NULL;
    }

    env->outer = outer;
    env->arena = arena; // Store the arena pointer

    return env;
}

L0_Value* l0_env_lookup(L0_Env* env, L0_Value* symbol) {
    assert(env != NULL);
    assert(symbol != NULL && symbol->type == L0_TYPE_SYMBOL);

    const char* target_symbol_name = symbol->data.symbol; // Get the name once
    // fprintf(stderr, "[DEBUG ENV LOOKUP] Looking for symbol '%s' starting in env %p\n", target_symbol_name, (void*)env); fflush(stderr); // DEBUG ENTRY

    L0_Env* current_env = env;
    int env_level = 0;
    while (current_env != NULL) {
        // fprintf(stderr, "[DEBUG ENV LOOKUP]  Searching env %p (level %d)\n", (void*)current_env, env_level); fflush(stderr); // DEBUG ENV LEVEL
        L0_Value* current_frame = current_env->frame;
        int binding_index = 0;

        // <<< CYCLE DETECTION SETUP >>>
        L0_Value* tortoise = current_env->frame;
        L0_Value* hare = current_env->frame;
        // Need at least 2 nodes initially for hare to move twice and potentially meet tortoise
        bool cycle_possible = l0_is_pair(hare) && l0_is_pair(hare->data.pair.cdr);

        // Iterate through the association list (frame)
        while (l0_is_pair(current_frame)) {

            // <<< CYCLE DETECTION STEP >>>
            // Advance pointers *before* processing the current_frame node
            // This ensures tortoise and hare move correctly relative to each other
            if (cycle_possible) {
                // Advance hare twice
                hare = hare->data.pair.cdr; // First step
                if (l0_is_pair(hare)) {
                    hare = hare->data.pair.cdr; // Second step
                    // Advance tortoise once
                    tortoise = tortoise->data.pair.cdr;

                    // Check for meeting
                    if (hare == tortoise && hare != NULL) { // Ensure they meet at a valid node
                        fprintf(stderr, "[FATAL ENV LOOKUP CYCLE] Cycle detected in environment frame list for env %p! Hare/Tortoise met at %p. Aborting lookup for '%s'.\n",
                                (void*)current_env, (void*)hare, target_symbol_name);
                        fflush(stderr);
                        // Set error status
                        l0_parser_error_status = L0_PARSE_ERROR_RUNTIME; // Or a more specific error code
                        // Use env->arena which should be valid if env is valid
                        l0_parser_error_message = l0_arena_strdup(env->arena, "Cycle detected in environment frame during lookup");
                        return NULL; // Indicate error: cycle detected
                    }
                     // Update cycle_possible check for next iteration: can hare move twice again?
                     cycle_possible = l0_is_pair(hare) && l0_is_pair(hare->data.pair.cdr);
                } else {
                    cycle_possible = false; // Hare reached end, no cycle possible from here
                }
            }
            // <<< END CYCLE DETECTION STEP >>>


            // <<< DETAILED FRAME LOGGING >>>
            // Log the current_frame node *after* potentially moving tortoise/hare
            // fprintf(stderr, "[DEBUG ENV LOOKUP]   Frame Node Addr: %p, Node CAR Addr: %p, Node CDR Addr: %p\n",
            //         (void*)current_frame,
            //         (void*)current_frame->data.pair.car,
            //         (void*)current_frame->data.pair.cdr);
            // fflush(stderr);
            // <<< END DETAILED FRAME LOGGING >>>

            L0_Value* binding = current_frame->data.pair.car; // binding is (sym . val)
            // fprintf(stderr, "[DEBUG ENV LOOKUP]   Checking binding %d in frame %p (Binding Addr: %p)\n", binding_index, (void*)current_frame, (void*)binding); fflush(stderr); // DEBUG BINDING CHECK
            if (l0_is_pair(binding)) {
                 // <<< DETAILED BINDING LOGGING >>>
                 // fprintf(stderr, "[DEBUG ENV LOOKUP]    Binding Pair Addr: %p, Binding CAR Addr: %p, Binding CDR Addr: %p\n",
                 //         (void*)binding,
                 //         (void*)binding->data.pair.car,
                 //         (void*)binding->data.pair.cdr);
                 // fflush(stderr);
                 // <<< END DETAILED BINDING LOGGING >>>
                L0_Value* frame_symbol = binding->data.pair.car;
                if (l0_is_symbol(frame_symbol)) {
                    const char* frame_symbol_name = frame_symbol->data.symbol;
                    int cmp_result = strcmp(frame_symbol_name, target_symbol_name);
                    // fprintf(stderr, "[DEBUG ENV LOOKUP]    Comparing '%s' with frame symbol '%s'. strcmp result: %d\n", target_symbol_name, frame_symbol_name, cmp_result); fflush(stderr); // DEBUG COMPARE
                    if (cmp_result == 0) {
                        // Found the symbol in this frame, return the value (cdr of the binding pair)
                        // fprintf(stderr, "[DEBUG ENV LOOKUP]    Symbol '%s' FOUND in env %p! Returning value %p (type %d)\n", target_symbol_name, (void*)current_env, (void*)binding->data.pair.cdr, binding->data.pair.cdr ? binding->data.pair.cdr->type : -1); fflush(stderr); // DEBUG FOUND
                        return binding->data.pair.cdr;
                    }
                } else {
                     // fprintf(stderr, "[DEBUG ENV LOOKUP]    Warning: Frame symbol is not a symbol (type %d)\n", frame_symbol ? frame_symbol->type : -1); fflush(stderr); // DEBUG MALFORMED SYM
                }
            } else {
                 // Malformed frame, should not happen if constructed correctly
                 // fprintf(stderr, "[DEBUG ENV LOOKUP]   Warning: Malformed binding (not a pair) found in environment frame %p at index %d.\n", (void*)current_frame, binding_index); fflush(stderr); // DEBUG MALFORMED BINDING
                 fprintf(stderr, "Warning: Malformed binding found in environment frame.\n"); // Keep this non-debug warning
            }
            current_frame = current_frame->data.pair.cdr; // Move to the next binding in the frame
        }
        // Symbol not found in the current frame, move to the outer environment
        current_env = current_env->outer;
    }

    // Symbol not found in any environment
    return NULL;
}
// --- ADDED DEBUG ---

bool l0_env_define(L0_Env* env, L0_Value* symbol, L0_Value* value) {
    assert(env != NULL);
    assert(symbol != NULL && symbol->type == L0_TYPE_SYMBOL);
    // value can be NULL, representing an unbound state initially? Or should always be valid? Assume valid for now.
    assert(value != NULL);
    assert(env->arena != NULL); // Ensure env has a valid arena

    const char* target_symbol_name = symbol->data.symbol; // Get the name once
    fprintf(stderr, "[DEBUG ENV DEFINE] Defining symbol '%s' in env %p\n", target_symbol_name, (void*)env); fflush(stderr); // DEBUG ENTRY

    L0_Value* current_frame = env->frame;
    L0_Value* prev_cons = NULL; // Keep track of the previous cons cell for potential update

    // Check if symbol already exists in the *current* frame
    while (l0_is_pair(current_frame)) {
        L0_Value* binding = current_frame->data.pair.car; // binding is (sym . val)
        if (l0_is_pair(binding)) {
            L0_Value* frame_symbol = binding->data.pair.car;
            if (l0_is_symbol(frame_symbol) &&
                strcmp(frame_symbol->data.symbol, target_symbol_name) == 0) {
                // Found existing binding in this frame, update the value
                fprintf(stderr, "[DEBUG ENV DEFINE]  Symbol '%s' already exists in env %p. Updating value.\n", target_symbol_name, (void*)env); fflush(stderr); // DEBUG UPDATE
                binding->data.pair.cdr = value;
                return true; // Definition successful (update)
            }
        }
         prev_cons = current_frame;
         current_frame = current_frame->data.pair.cdr;
    }

    // Symbol not found in the current frame, add a new binding
    fprintf(stderr, "[DEBUG ENV DEFINE]  Symbol '%s' not found in env %p. Adding new binding.\n", target_symbol_name, (void*)env); fflush(stderr); // DEBUG ADD NEW
    // Create the new binding pair (symbol . value)
    L0_Value* new_binding = l0_make_pair(env->arena, symbol, value);
    if (!new_binding) {
        fprintf(stderr, "Error: Failed to allocate new binding pair in arena for symbol '%s'.\n", target_symbol_name); fflush(stderr); // DEBUG ALLOC FAIL 1
        return false;
    }
    fprintf(stderr, "[DEBUG ENV DEFINE]   Created new binding pair %p ((%s . %p))\n", (void*)new_binding, target_symbol_name, (void*)value); fflush(stderr); // DEBUG BINDING CREATED

    // Create the new cons cell to add the binding to the frame list
    L0_Value* new_frame_cons = l0_make_pair(env->arena, new_binding, env->frame);
     if (!new_frame_cons) {
        fprintf(stderr, "Error: Failed to allocate new frame cons cell in arena for symbol '%s'.\n", target_symbol_name); fflush(stderr); // DEBUG ALLOC FAIL 2
        // new_binding is already in arena, managed there.
        return false;
    }
     fprintf(stderr, "[DEBUG ENV DEFINE]   Created new frame cons %p. Old frame head was %p.\n", (void*)new_frame_cons, (void*)env->frame); fflush(stderr); // DEBUG CONS CREATED

    // Update the environment's frame pointer to the new head of the list
    env->frame = new_frame_cons;
    fprintf(stderr, "[DEBUG ENV DEFINE]  Updated env %p frame head to %p.\n", (void*)env, (void*)env->frame); fflush(stderr); // DEBUG FRAME UPDATED

    return true; // Definition successful (new binding)
}

L0_Env* l0_env_extend(L0_Env* outer_env) {
    assert(outer_env != NULL);
    assert(outer_env->arena != NULL); // Outer env must have an arena

    fprintf(stderr, "[DEBUG ENV EXTEND] Extending env %p\n", (void*)outer_env); fflush(stderr); // DEBUG EXTEND
    // Create a new environment whose outer pointer is the given outer_env.
    // Use the same arena as the outer environment.
    L0_Env* new_env = l0_env_create(outer_env->arena, outer_env);
    fprintf(stderr, "[DEBUG ENV EXTEND]  New env created: %p (outer=%p)\n", (void*)new_env, (void*)outer_env); fflush(stderr); // DEBUG EXTEND RESULT
    return new_env;
}

// Implementation for l0_env_set (optional, if needed for set!)
bool l0_env_set(L0_Env* env, L0_Value* symbol, L0_Value* value) {
    assert(env != NULL);
    assert(symbol != NULL && symbol->type == L0_TYPE_SYMBOL);
    assert(value != NULL); // Value should be valid

    const char* target_symbol_name = symbol->data.symbol;
    fprintf(stderr, "[DEBUG ENV SET] Setting symbol '%s' starting in env %p\n", target_symbol_name, (void*)env); fflush(stderr); // DEBUG SET ENTRY

    L0_Env* current_env = env;
    int env_level = 0;
    while (current_env != NULL) {
         fprintf(stderr, "[DEBUG ENV SET]  Searching env %p (level %d)\n", (void*)current_env, env_level); fflush(stderr); // DEBUG SET SEARCH
        L0_Value* current_frame = current_env->frame;
        while (l0_is_pair(current_frame)) {
            L0_Value* binding = current_frame->data.pair.car;
            if (l0_is_pair(binding)) {
                L0_Value* frame_symbol = binding->data.pair.car;
                if (l0_is_symbol(frame_symbol) &&
                    strcmp(frame_symbol->data.symbol, target_symbol_name) == 0) {
                    // Found the binding, update the value in this frame
                    fprintf(stderr, "[DEBUG ENV SET]   Symbol '%s' FOUND in env %p. Updating value.\n", target_symbol_name, (void*)current_env); fflush(stderr); // DEBUG SET FOUND
                    binding->data.pair.cdr = value;
                    return true; // Set successful
                }
            }
            current_frame = current_frame->data.pair.cdr;
        }
        // Not found in current frame, check outer
        fprintf(stderr, "[DEBUG ENV SET]  Symbol '%s' not found in env %p. Moving to outer env %p\n", target_symbol_name, (void*)current_env, (void*)current_env->outer); fflush(stderr); // DEBUG SET MOVE OUTER
        current_env = current_env->outer;
        env_level++;
    }

    // Variable not found in any environment
    fprintf(stderr, "[DEBUG ENV SET] Symbol '%s' NOT FOUND for set!.\n", target_symbol_name); fflush(stderr); // DEBUG SET NOT FOUND
    // Error message is handled by the caller (l0_eval)
    // fprintf(stderr, "Error: Attempted to set unbound variable '%s'.\n", symbol->data.symbol);
    return false; // Indicate variable not found
}
