#ifndef L0_TYPES_H
#define L0_TYPES_H

#include <stdbool.h>
#include <stddef.h> // For size_t

// Forward declarations
typedef struct L0_Value L0_Value;
typedef struct L0_Env L0_Env;     // Needed for Closure
typedef struct L0_Arena L0_Arena; // Needed for constructors
typedef L0_Value* (*L0_PrimitiveFunc)(L0_Value* args, L0_Env* env, L0_Arena* arena); // Needed for Primitive

// Enum to distinguish different types of L0 values (Atoms and Pairs/Lists)
typedef enum {
    L0_TYPE_NIL,      // Represents the empty list '()
    L0_TYPE_BOOLEAN,
    L0_TYPE_INTEGER,
    L0_TYPE_SYMBOL,
    L0_TYPE_PAIR,     // Building block for lists (cons cell)
    L0_TYPE_STRING,   // String type
    L0_TYPE_PRIMITIVE,// Built-in function implemented in C
    L0_TYPE_CLOSURE,  // User-defined function (lambda)
    L0_TYPE_FLOAT,    // Floating point number type
    L0_TYPE_REF       // Reference (&T) - Always mutable in this simplified model
    // L0_TYPE_BOX    // Owned heap pointer (Box<T>) - Removed in simplification
} L0_ValueType;

// Structure for a Pair (cons cell)
typedef struct {
    L0_Value* car; // First element
    L0_Value* cdr; // Rest of the list (or another value in a general pair)
} L0_Pair;

// Structure for an immutable reference
typedef struct {
    L0_Value* referred; // Pointer to the value being referenced
    // TODO: Add lifetime information? - Removed in simplification
} L0_Ref;

// Structure for a Primitive function
typedef struct {
    const char* name; // Optional: name for debugging/printing
    L0_PrimitiveFunc func;
} L0_Primitive;

// Structure for a Closure (user-defined function)
typedef struct {
    L0_Value* params; // List of parameter symbols
    L0_Value* body;   // List of body expressions
    L0_Env* env;      // The environment where the lambda was defined (captured environment)
} L0_Closure;


// Structure representing any L0 value
// Using a tagged union approach
struct L0_Value {
    L0_ValueType type;
    union {
        bool boolean;
        long integer;       // Using long for integers
        char* symbol;       // Store symbol names as strings (owned by Arena)
        char* string;       // Store string content (owned by Arena)
        double float_num; // Store float value
        L0_Pair pair;
        L0_Primitive primitive;
        L0_Closure closure;
        L0_Ref ref;           // Data for reference (&T)
        // No data needed for NIL type
    } data;
    // Potential future additions: memory management info (e.g., arena ptr, ownership flags)
};

// --- Constructor Helpers (Now require an Arena) ---
// These functions allocate L0_Value objects within the given Arena.
// The memory is managed by the Arena and freed when the Arena is destroyed or reset.

L0_Value* l0_make_nil(L0_Arena* arena); // Note: NIL doesn't strictly need arena, but consistent API
L0_Value* l0_make_boolean(L0_Arena* arena, bool value);
L0_Value* l0_make_integer(L0_Arena* arena, long value);
L0_Value* l0_make_symbol(L0_Arena* arena, const char* name); // Copies the string into the arena
L0_Value* l0_make_string(L0_Arena* arena, const char* value); // Copies the string into the arena
L0_Value* l0_make_float(L0_Arena* arena, double value);
L0_Value* l0_make_pair(L0_Arena* arena, L0_Value* car, L0_Value* cdr);
L0_Value* l0_make_primitive(L0_Arena* arena, L0_PrimitiveFunc func, const char* name); // Added name parameter
L0_Value* l0_make_closure(L0_Arena* arena, L0_Value* params, L0_Value* body, L0_Env* env);
L0_Value* l0_make_ref(L0_Arena* arena, L0_Value* referred); // Only one make_ref needed


// --- Type Predicates ---
bool l0_is_nil(const L0_Value* value);
bool l0_is_boolean(const L0_Value* value);
bool l0_is_integer(const L0_Value* value);
bool l0_is_symbol(const L0_Value* value);
bool l0_is_string(const L0_Value* value);
bool l0_is_float(const L0_Value* value);
bool l0_is_pair(const L0_Value* value);
bool l0_is_primitive(const L0_Value* value);
bool l0_is_closure(const L0_Value* value);
bool l0_is_ref(const L0_Value* value); // Only one is_ref needed
bool l0_is_atom(const L0_Value* value); // Not a pair or nil
bool l0_is_list(const L0_Value* value); // Proper list check (ends in nil)
bool l0_is_truthy(const L0_Value* value); // Checks if a value is considered true in conditional contexts

// Macro for C code generation convenience (same logic as l0_is_truthy)
#define L0_IS_TRUTHY(v) (!((v) && (v)->type == L0_TYPE_BOOLEAN && !(v)->data.boolean))

// --- Global Nil Value ---
// Declare L0_NIL as an external constant pointer.
// It will be defined and initialized in l0_types.c.
extern L0_Value* const L0_NIL;


#endif // L0_TYPES_H
