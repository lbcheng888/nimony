#include "l0_types.h"
#include "l0_arena.h" // Need arena definitions for allocators
#include "l0_parser.h" // Include parser header for error status definitions
#include <stdio.h>    // For NULL
#include <string.h>   // For strcmp in l0_is_list cycle check (if symbols are used)
#include <assert.h>   // For assertions (if needed)

// --- Global Nil Definition ---
// Define the actual nil value.
// We define a static instance and point L0_NIL to it.
static L0_Value nil_instance = { .type = L0_TYPE_NIL };
L0_Value* const L0_NIL = &nil_instance; // L0_NIL points to the static instance


// --- Arena-based Constructor Implementations ---

// Define global error variables needed by constructors if they fail allocation
// These should match the definitions in l0_parser.c (consider moving error handling to its own module later)
extern L0_ParseStatus l0_parser_error_status;
extern const char* l0_parser_error_message;

L0_Value* l0_make_nil(L0_Arena* arena) {
    (void)arena; // Arena is not needed, nil is a global singleton
    // Always return the global L0_NIL constant.
    // No allocation needed.
    return L0_NIL;
}

L0_Value* l0_make_boolean(L0_Arena* arena, bool b_val) {
    // Consider global TRUE/FALSE if arena persists.
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for BOOLEAN";
        return NULL;
    }
    val->type = L0_TYPE_BOOLEAN;
    val->data.boolean = b_val;
    return val;
}

L0_Value* l0_make_integer(L0_Arena* arena, long i_val) {
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for INTEGER";
        return NULL;
    }
    val->type = L0_TYPE_INTEGER;
    val->data.integer = i_val;
    return val;
}

L0_Value* l0_make_symbol(L0_Arena* arena, const char* name) {
    if (!name) {
        // Maybe set error? Or return specific error value?
        return NULL; // Or handle error appropriately
    }
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for SYMBOL value";
        return NULL;
    }
    val->type = L0_TYPE_SYMBOL;
    // Duplicate the string using the arena's allocator
    val->data.symbol = l0_arena_strdup(arena, name);
    if (!val->data.symbol) {
        // l0_arena_strdup handles its own allocation error message.
        // We don't need to free 'val' as it's in the arena.
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for symbol name duplication";
        // Return NULL, but the partially allocated L0_Value for the symbol
        // remains in the arena until the arena is destroyed/reset. This is acceptable.
        return NULL;
    }
    return val;
}

L0_Value* l0_make_string(L0_Arena* arena, const char* value) {
    if (!value) {
        return NULL; // Or handle error appropriately
    }
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
    if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for STRING value";
        return NULL;
    }
    val->type = L0_TYPE_STRING;
    // Duplicate the string using the arena's allocator
    val->data.string = l0_arena_strdup(arena, value);
    if (!val->data.string) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for string content duplication";
        return NULL;
    }
    return val;
}

L0_Value* l0_make_float(L0_Arena* arena, double f_val) {
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for FLOAT";
         return NULL;
    }
    val->type = L0_TYPE_FLOAT;
    val->data.float_num = f_val; // Use float_num here
    return val;
}

L0_Value* l0_make_pair(L0_Arena* arena, L0_Value* car, L0_Value* cdr) {
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);

     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for PAIR";
        return NULL;
    }
    val->type = L0_TYPE_PAIR;
    val->data.pair.car = car;
    val->data.pair.cdr = cdr;
    return val;
}

L0_Value* l0_make_primitive(L0_Arena* arena, L0_PrimitiveFunc func, const char* name) { // Added name parameter
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for PRIMITIVE";
        return NULL;
    }
    val->type = L0_TYPE_PRIMITIVE;
    val->data.primitive.func = func;
    // Copy the name into the arena if provided
    val->data.primitive.name = name ? l0_arena_strdup(arena, name) : NULL;
    // Note: We don't explicitly check for allocation failure of the name string here.
    // If l0_arena_strdup fails, it sets its own error and returns NULL,
    // which is acceptable for an optional debug name.
    return val;
}

L0_Value* l0_make_closure(L0_Arena* arena, L0_Value* params, L0_Value* body, L0_Env* env) {
     L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
     if (!val) {
        l0_parser_error_status = L0_PARSE_ERROR_MEMORY;
        l0_parser_error_message = "Arena allocation failed for CLOSURE";
        return NULL;
    }
    val->type = L0_TYPE_CLOSURE;
    val->data.closure.params = params;
    val->data.closure.body = body;
    val->data.closure.env = env; // Capture the environment
    return val;
}

L0_Value* l0_make_ref(L0_Arena* arena, L0_Value* referred) {
    if (!referred) {
        // Cannot create a reference to NULL.
        // TODO: How should type errors be reported? For now, return NULL.
        fprintf(stderr, "Error: Attempted to create a reference to NULL.\n");
        return NULL;
    }
    L0_Value* val = l0_arena_alloc_type(arena, L0_Value);
    if (!val) {
        // Arena allocation failure is handled by l0_arena_alloc_type setting its own error message.
        // l0_parser_error_status = L0_PARSE_ERROR_MEMORY; // Remove direct dependency
        // l0_parser_error_message = "Arena allocation failed for REF"; // Remove direct dependency
        return NULL; // Arena allocation failed
    }
    val->type = L0_TYPE_REF;
    val->data.ref.referred = referred;
    return val;
}

// --- Type Predicate Implementations ---
// Note: l0_is_nil now checks against the global L0_NIL constant
bool l0_is_nil(const L0_Value* value) { return value == L0_NIL; }
bool l0_is_boolean(const L0_Value* value) { return value && value->type == L0_TYPE_BOOLEAN; }
bool l0_is_integer(const L0_Value* value) { return value && value->type == L0_TYPE_INTEGER; }
bool l0_is_symbol(const L0_Value* value) { return value && value->type == L0_TYPE_SYMBOL; }
bool l0_is_string(const L0_Value* value) { return value && value->type == L0_TYPE_STRING; }
bool l0_is_float(const L0_Value* value) { return value && value->type == L0_TYPE_FLOAT; }
bool l0_is_pair(const L0_Value* value) { return value && value->type == L0_TYPE_PAIR; }
bool l0_is_primitive(const L0_Value* value) { return value && value->type == L0_TYPE_PRIMITIVE; }
bool l0_is_closure(const L0_Value* value) { return value && value->type == L0_TYPE_CLOSURE; }
bool l0_is_ref(const L0_Value* value) { return value && value->type == L0_TYPE_REF; }
// Removed l0_is_mut_ref

bool l0_is_atom(const L0_Value* value) {
    // Atoms are anything that isn't a pair or nil (includes primitives, closures, strings, refs)
    return value && value->type != L0_TYPE_PAIR && value->type != L0_TYPE_NIL;
}

bool l0_is_list(const L0_Value* value) {
    if (!value) return false;
    if (l0_is_nil(value)) return true; // Empty list is a list
    if (!l0_is_pair(value)) return false; // Must be a pair to be a non-empty list

    // Optimization: Avoid deep recursion for long lists by iterating
    const L0_Value* current = value;
    while (l0_is_pair(current)) {
        // Check for improper list ending in self-reference cycle
        if (current->data.pair.cdr == current) return false;
        current = current->data.pair.cdr;
    }
    return l0_is_nil(current); // Must end in NIL
}


// --- Runtime Helper Implementations ---

// Checks if a value is considered "true" in Lisp semantics (anything other than #f)
bool l0_is_truthy(const L0_Value* value) {
    // In Scheme/Lisp, only #f is false. Everything else is true.
    if (l0_is_boolean(value) && value->data.boolean == false) {
        return false; // Only #f is false
    }
    return true; // Everything else is true
}
