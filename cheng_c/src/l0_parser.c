#include "l0_parser.h" // Include the header we just created
#include "l0_types.h"  // Include the core types
#include "l0_arena.h"  // Include the arena allocator
#include <stdio.h>
#include <stdlib.h>   // For strtol etc. (malloc/free will be removed for L0_Value)
#include <string.h>   // For strncpy, strcmp, strdup etc.
#include <ctype.h>    // For isspace, isdigit etc.
#include <assert.h>   // For basic assertions
#include <errno.h>    // For errno checking with strtod
#include <stdbool.h>  // Include for bool type
#include <stdalign.h> // Include for alignof operator

// --- Global Error Variables Definition ---
// These are defined here and declared as extern in the header.
L0_ParseStatus l0_parser_error_status = L0_PARSE_OK;
const char* l0_parser_error_message = NULL;
unsigned long l0_parser_error_line = 1;
unsigned long l0_parser_error_col = 1;

// --- Internal Parser State (Example - might need refinement) ---
// It might be better to pass a state struct around instead of using globals,
// especially for stream parsing and error reporting.
typedef struct {
    const char* current_pos; // For string parsing
    FILE* stream;            // For stream parsing
    const char* filename;    // For error reporting
    unsigned long line;      // Current line number
    unsigned long col;       // Current column number
    L0_Arena* arena;         // Arena for allocations
    // Add lookahead buffer if needed for stream parsing
} ParserState;

// --- Forward Declarations for Internal Parsing Functions ---
static L0_Value* parse_sexpr(ParserState* state);
static L0_Value* parse_list(ParserState* state);
static L0_Value* parse_atom(ParserState* state);
static L0_Value* parse_string_literal(ParserState* state); // <-- Add forward declaration here
// Add helpers for skipping whitespace, comments, reading tokens etc.
static void skip_whitespace_and_comments(ParserState* state);
// ... other static helper functions ...

// --- Helper to set parser error ---
static void set_parser_error(ParserState* state, L0_ParseStatus status, const char* message) {
    l0_parser_error_status = status;
    l0_parser_error_message = message; // Note: message should ideally be persistent (e.g., string literal or arena allocated)
    if (state) {
        l0_parser_error_line = state->line;
        l0_parser_error_col = state->col;
    } else {
        // Default position if state is not available
        l0_parser_error_line = 1;
        l0_parser_error_col = 1;
    }
}


// --- Public API Function Implementations ---

L0_Value* l0_parse_string(L0_Arena* arena, const char* input, const char** end_ptr) {
    // Reset global error state (might be better to return error info directly)
    l0_parser_error_status = L0_PARSE_OK;
    l0_parser_error_message = NULL;
    // Line/col will be set by set_parser_error on error

    if (!arena) {
        set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Arena pointer is NULL");
        return NULL;
    }
    if (!input) {
        set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Input string is NULL");
        return NULL;
    }

    ParserState state = {0};
    state.current_pos = input;
    state.line = 1;
    state.col = 1;
    state.filename = "<string>"; // Default filename for string input
    state.arena = arena;         // Pass the arena

    skip_whitespace_and_comments(&state); // Pass address of state

    if (*state.current_pos == '\0') {
        // Empty input or only whitespace/comments
        if (end_ptr) *end_ptr = state.current_pos;
        return NULL; // Or maybe return NIL? Depends on desired behavior.
                     // Let's return NULL for "no expression found".
    }

    L0_Value* result = parse_sexpr(&state); // Pass address of state

    if (result && end_ptr) {
        // Skip any trailing whitespace AFTER the parsed expression
        skip_whitespace_and_comments(&state);
        *end_ptr = state.current_pos; // Update end pointer after successful parse AND skipping trailing whitespace
    }

    // Error position is set within set_parser_error if needed

    return result;
}
L0_Value* l0_parse_stream(L0_Arena* arena, FILE* stream, const char* filename) {
    // Reset global error state
    l0_parser_error_status = L0_PARSE_OK;
    l0_parser_error_message = NULL;

    if (!arena) {
        set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Arena pointer is NULL");
        return NULL;
    }
    if (!stream) {
         set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Input stream is NULL");
         return NULL;
    }

    ParserState state = {0};
    state.stream = stream;
    state.line = 1;
    state.col = 1;
    state.filename = filename ? filename : "<stream>";
    state.arena = arena;

    // TODO: Implement stream parsing logic, potentially using lookahead/buffering.
    // skip_whitespace_and_comments(&state);
    // L0_Value* result = parse_sexpr(&state);
    // ... handle EOF and errors ...

    set_parser_error(&state, L0_PARSE_ERROR_INVALID_SYNTAX, "Stream parsing not yet implemented"); // Pass address of state
    return NULL; // Placeholder
}

L0_Value* l0_parse_string_all(L0_Arena* arena, const char* input, const char* filename) {
    // fprintf(stderr, "[DEBUG PSA] Entered l0_parse_string_all. arena=%p, input=%p\n", (void*)arena, (void*)input); fflush(stderr); // REMOVED DEBUG
     // Reset global error state
    l0_parser_error_status = L0_PARSE_OK;
    l0_parser_error_message = NULL;

    if (!arena) {
        set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Arena pointer is NULL");
        return NULL;
    }
    if (!input) {
        set_parser_error(NULL, L0_PARSE_ERROR_INVALID_SYNTAX, "Input string is NULL");
        return NULL;
    }

    ParserState state = {0};
    state.current_pos = input;
    state.line = 1;
    state.col = 1;
    state.filename = filename ? filename : "<string>";
    state.arena = arena;

    // --- VERY EARLY DEBUG ---
    // fprintf(stderr, "[DEBUG string_all] ENTERED function. state.current_pos=%p, state.arena=%p\n", (void*)state.current_pos, (void*)state.arena); fflush(stderr); // REMOVED DEBUG
    // --- END DEBUG ---


    // Use a dummy head node to simplify appending
    // fprintf(stderr, "[DEBUG string_all] Allocating dummy head...\n"); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG PSA DH] Before l0_make_nil for car. arena=%p\n", (void*)state.arena); fflush(stderr); // REMOVED DEBUG
    L0_Value* dummy_head_car = l0_make_nil(state.arena);
    // fprintf(stderr, "[DEBUG PSA DH] After l0_make_nil for car. dummy_head_car=%p\n", (void*)dummy_head_car); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG string_all] dummy_head_car allocated: %p\n", (void*)dummy_head_car); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG PSA DH] Before l0_make_nil for cdr. arena=%p\n", (void*)state.arena); fflush(stderr); // REMOVED DEBUG
    L0_Value* dummy_head_cdr = l0_make_nil(state.arena);
    // fprintf(stderr, "[DEBUG PSA DH] After l0_make_nil for cdr. dummy_head_cdr=%p\n", (void*)dummy_head_cdr); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG string_all] dummy_head_cdr allocated: %p\n", (void*)dummy_head_cdr); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG PSA DH] Before l0_make_pair for head. arena=%p, car=%p, cdr=%p\n", (void*)state.arena, (void*)dummy_head_car, (void*)dummy_head_cdr); fflush(stderr); // REMOVED DEBUG
    L0_Value* dummy_head = l0_make_pair(state.arena, dummy_head_car, dummy_head_cdr);
    // fprintf(stderr, "[DEBUG PSA DH] After l0_make_pair for head. dummy_head=%p\n", (void*)dummy_head); fflush(stderr); // REMOVED DEBUG
    // fprintf(stderr, "[DEBUG string_all] dummy_head allocated: %p\n", (void*)dummy_head); fflush(stderr); // REMOVED DEBUG

    L0_Value* tail = dummy_head;
    // fprintf(stderr, "[DEBUG PSA DH] Before dummy head check. dummy_head=%p\n", (void*)dummy_head); fflush(stderr); // REMOVED DEBUG
    if (!dummy_head || !l0_is_pair(dummy_head) || !l0_is_nil(dummy_head->data.pair.car) || !l0_is_nil(dummy_head->data.pair.cdr)) {
        // fprintf(stderr, "[DEBUG string_all] ERROR allocating dummy head!\n"); fflush(stderr); // REMOVED DEBUG
        set_parser_error(&state, L0_PARSE_ERROR_MEMORY, "Failed to allocate dummy head for result list");
        return NULL;
    }
    // fprintf(stderr, "[DEBUG string_all] Dummy head allocation successful. Entering loop...\n"); fflush(stderr); // REMOVED DEBUG

    // fprintf(stderr, "[DEBUG PSA] Before main parsing loop.\n"); fflush(stderr); // REMOVED DEBUG
    while (true) {
        // fprintf(stderr, "[DEBUG PSA] Top of main loop.\n"); fflush(stderr); // REMOVED DEBUG
        // fprintf(stderr, "[DEBUG string_all] Loop start. Calling skip_whitespace_and_comments...\n"); fflush(stderr); // REMOVED DEBUG
        skip_whitespace_and_comments(&state);
        // fprintf(stderr, "[DEBUG string_all] After skip_whitespace_and_comments. Next char: '%c'\n", *state.current_pos); fflush(stderr); // REMOVED DEBUG

        // --- ADD DEBUG HERE ---
        // fprintf(stderr, "[DEBUG string_all] Top of loop: char='%c'(%d), pos=%p\n", // REMOVED DEBUG
        //         isprint((unsigned char)*state.current_pos) ? *state.current_pos : '?', // REMOVED DEBUG
        //         (int)*state.current_pos, (void*)state.current_pos); // REMOVED DEBUG
        // --- END DEBUG ---

        if (*state.current_pos == '\0') {
            // fprintf(stderr, "[DEBUG string_all] EOF check passed, breaking loop.\n"); // REMOVED DEBUG
            break; // End of input
        }
        // fprintf(stderr, "[DEBUG string_all] EOF check failed, calling parse_sexpr.\n"); // REMOVED DEBUG


        L0_Value* expr = parse_sexpr(&state); // Pass address of state

        if (!expr) {
            // Check if parse_sexpr returned NULL because of a real error,
            // or simply because it hit EOF when expecting an expression.
            if (l0_parser_error_status != L0_PARSE_OK) {
                // A real parsing error occurred inside parse_sexpr or its children.
                // Error message/status should already be set. Propagate the error.
                return NULL;
            } else {
                // parse_sexpr returned NULL, but no error status was set.
                // This means it hit the end of the input gracefully when looking
                // for the next expression. This is unexpected *inside* the loop,
                // as the EOF check at the top should have caught it.
                // However, let's treat it as a clean EOF signal from parse_sexpr
                // and break the loop, assuming the top-level check might have missed
                 // EOF after skipping comments/whitespace right at the end.
                 // This prevents reporting a false error.
                 // fprintf(stderr, "[DEBUG string_all] parse_sexpr returned NULL without error, breaking loop.\n"); // REMOVED DEBUG
                 break; // Exit the loop cleanly
             }
         }

        // --- Append the successfully parsed expression ---
        L0_Value* next_nil = l0_make_nil(state.arena); // Terminating nil for the new pair
        if (!next_nil) {
             set_parser_error(&state, L0_PARSE_ERROR_MEMORY, "Failed to allocate NIL for list pair in string_all");
             return NULL;
        }
        L0_Value* new_pair = l0_make_pair(state.arena, expr, next_nil);
        if (!new_pair) {
             set_parser_error(&state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for result list in string_all");
             return NULL;
        }

        // Always append to the current tail's cdr and update tail
        // Since tail starts as dummy_head (a pair), this should always work.
        assert(l0_is_pair(tail)); // Assert that tail is always a pair here
        tail->data.pair.cdr = new_pair;
        tail = new_pair; // Move tail to the newly added pair
    }

    // Return the actual list starting from the dummy head's cdr
    return dummy_head->data.pair.cdr;
}


// --- Internal Parsing Function Implementations (Stubs) ---

static void skip_whitespace_and_comments(ParserState* state) {
    // Needs to handle both string and stream input and update line/col numbers.
    // For string input:
    if (state->current_pos) {
        while (true) {
            // Skip whitespace
            while (*state->current_pos != '\0' && isspace((unsigned char)*state->current_pos)) {
                if (*state->current_pos == '\n') {
                    state->line++;
                    state->col = 1;
                } else {
                    state->col++;
                }
                state->current_pos++;
            }

            // Skip comment (from ';' to end of line or EOF)
            if (*state->current_pos == ';') {
                while (*state->current_pos != '\0' && *state->current_pos != '\n') {
                    state->current_pos++;
                    state->col++;
                }
                // If we stopped at newline, loop again to skip it and potentially more whitespace/comments
                if (*state->current_pos == '\n') {
                    continue; // Let the outer loop handle the newline
                } else {
                    break; // EOF or end of comment not followed by newline
                }
            } else {
                break; // Not whitespace, not comment
            }
        }
        // --- DEBUG ---
        // Only print if something was actually skipped
        // if (state->current_pos != before_skip) { // Temporarily disable to reduce noise
        //      fprintf(stderr, "[DEBUG skip_ws] Skipped from %p to %p. Next char='%c'(%d)\n",
        //              (void*)before_skip, (void*)state->current_pos,
        //              isprint((unsigned char)*state->current_pos) ? *state->current_pos : '?',
        //              (int)*state->current_pos);
        // }
        // --- END DEBUG ---
    } else {
        // TODO: Implement for stream input
    }
}

static L0_Value* parse_sexpr(ParserState* state) {
    skip_whitespace_and_comments(state);

    // --- ADD DEBUG ---
    // fprintf(stderr, "[DEBUG parse_sexpr] After skip_ws: char='%c'(%d), pos=%p\n", // REMOVED DEBUG
    //         isprint((unsigned char)*state->current_pos) ? *state->current_pos : '?', // REMOVED DEBUG
    //         (int)*state.current_pos, (void*)state->current_pos); // REMOVED DEBUG
    // --- END DEBUG ---


    // Needs to handle both string and stream input.
    if (state->current_pos) { // String parsing
        char next_char = *state->current_pos;
        if (next_char == '\0') {
            // fprintf(stderr, "[DEBUG parse_sexpr] Detected EOF, returning NULL.\n"); // REMOVED DEBUG
            // Reached EOF when expecting an expression. This is not an error for
            // parse_sexpr itself, but signals to the caller (like l0_parse_string_all)
            // that there are no more expressions. The caller handles the top-level EOF.
            // Do NOT set error status here.
            return NULL; // Signal no expression found / EOF
        } else if (next_char == '(') {
            return parse_list(state);
        } else if (next_char == ')') {
            set_parser_error(state, L0_PARSE_ERROR_INVALID_SYNTAX, "Unexpected closing parenthesis ')'");
            return NULL;
        } else if (next_char == '`') { // Handle quasiquote `expr
             state->current_pos++; // Consume '`'
             state->col++;
             L0_Value* quoted_expr = parse_sexpr(state);
             if (!quoted_expr) {
                 // Error occurred during parsing the quoted expression OR EOF after '`'
                 if (l0_parser_error_status == L0_PARSE_OK) { // Check if EOF was the cause
                     set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unexpected end of input after quasiquote '`'");
                 }
                 return NULL;
             }
             // Construct (quasiquote <expr>)
             L0_Value* qq_sym = l0_make_symbol(state->arena, "quasiquote");
             if (!qq_sym) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate 'quasiquote' symbol"); return NULL; }
             L0_Value* expr_nil = l0_make_nil(state->arena);
             if (!expr_nil) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate nil for quasiquoted list"); return NULL; }
             L0_Value* quoted_list = l0_make_pair(state->arena, quoted_expr, expr_nil);
             if (!quoted_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for quasiquoted list"); return NULL; }
             L0_Value* result_list = l0_make_pair(state->arena, qq_sym, quoted_list);
             if (!result_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for (quasiquote ...) list"); return NULL; }
             return result_list;
        } else if (next_char == ',') { // Handle unquote ,expr and unquote-splicing ,@expr
             state->current_pos++; // Consume ','
             state->col++;
             const char* symbol_name;
             if (*state->current_pos == '@') {
                 state->current_pos++; // Consume '@'
                 state->col++;
                 symbol_name = "unquote-splicing";
             } else {
                 symbol_name = "unquote";
             }
             L0_Value* unquoted_expr = parse_sexpr(state);
             if (!unquoted_expr) {
                 // Error occurred during parsing the unquoted expression OR EOF after ',' or ',@'
                 if (l0_parser_error_status == L0_PARSE_OK) { // Check if EOF was the cause
                     set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unexpected end of input after unquote ',' or ',@'");
                 }
                 return NULL;
             }
             // Construct (symbol <expr>)
             L0_Value* uq_sym = l0_make_symbol(state->arena, symbol_name);
             if (!uq_sym) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate unquote symbol"); return NULL; }
             L0_Value* expr_nil = l0_make_nil(state->arena);
             if (!expr_nil) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate nil for unquoted list"); return NULL; }
             L0_Value* unquoted_list = l0_make_pair(state->arena, unquoted_expr, expr_nil);
             if (!unquoted_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for unquoted list"); return NULL; }
             L0_Value* result_list = l0_make_pair(state->arena, uq_sym, unquoted_list);
             if (!result_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for (unquote...) list"); return NULL; }
             return result_list;
        } else if (next_char == '\'') { // <<< CORRECTED BLOCK START
             // Handle quote syntax sugar '(quote ...)'
             state->current_pos++; // Consume '''
             state->col++;
             L0_Value* quoted_expr = parse_sexpr(state);
             if (!quoted_expr) {
                 // Error occurred during parsing the quoted expression OR EOF after '''
                 if (l0_parser_error_status == L0_PARSE_OK) { // Check if EOF was the cause
                     set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unexpected end of input after quote '''"); // Corrected error message context
                 }
                 return NULL;
             }
             // Construct (quote <expr>)
             L0_Value* quote_sym = l0_make_symbol(state->arena, "quote"); // Corrected symbol name
             if (!quote_sym) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate 'quote' symbol"); return NULL; } // Corrected error message context
             L0_Value* expr_nil = l0_make_nil(state->arena);
             if (!expr_nil) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate nil for quoted list"); return NULL; } // Corrected error message context
             L0_Value* quoted_list = l0_make_pair(state->arena, quoted_expr, expr_nil);
             if (!quoted_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for quoted list"); return NULL; } // Corrected error message context
             L0_Value* result_list = l0_make_pair(state->arena, quote_sym, quoted_list); // Use corrected symbol
             if (!result_list) { set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for (quote ...) list"); return NULL; } // Corrected error message context
             return result_list;
        } // <<< CORRECTED BLOCK END
        else if (next_char == '"') {
             return parse_string_literal(state);
        } else {
            return parse_atom(state);
        }
    } else { // Stream parsing
        // TODO: Implement stream logic
        l0_parser_error_status = L0_PARSE_ERROR_INVALID_SYNTAX; // Mark as not implemented yet
        l0_parser_error_message = "Stream parsing in parse_sexpr not yet implemented";
        return NULL;
    }
} // <<< END OF parse_sexpr function

static L0_Value* parse_list(ParserState* state) {
    // Assumes '(' has just been consumed or is the current character.
    assert(*state->current_pos == '(');
    state->current_pos++; // Consume '('
    state->col++;

    L0_Value* head = l0_make_nil(state->arena); // Allocate initial NIL in arena
    L0_Value* tail = head;
    if (!head) {
        set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate initial NIL for list");
        return NULL;
    }

    while (true) {
        skip_whitespace_and_comments(state);
        char next_char = *state->current_pos;

        // --- DEBUG ---
        // fprintf(stderr, "[DEBUG parse_list] Loop top: next_char='%c'(%d), pos=%p, line=%lu, col=%lu\n", // REMOVED DEBUG
        //         isprint((unsigned char)next_char) ? next_char : '?', (int)next_char, (void*)state->current_pos, state->line, state->col); // REMOVED DEBUG
        // --- END DEBUG ---

        // Check for closing parenthesis FIRST
        if (next_char == ')') {
            state->current_pos++; // Consume ')'
            state->col++;
            return head; // Successfully parsed list (possibly empty)
        }
        // THEN check for EOF
        else if (next_char == '\0') {
            // fprintf(stderr, "[DEBUG parse_list] EOF detected inside list!\n"); // REMOVED DEBUG
            set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unexpected end of input inside list");
            // Arena handles memory cleanup
            return NULL;
        }
        // OTHERWISE, parse an element
        else {
            // Parse the next element in the list
            L0_Value* element = parse_sexpr(state);

            if (!element) {
                // Error occurred parsing element OR parse_sexpr hit EOF gracefully.
                // If parse_sexpr hit EOF, status will be OK, but it's an error here.
                if (l0_parser_error_status == L0_PARSE_OK) {
                    // parse_sexpr returned NULL without setting an error (meaning it hit EOF)
                    // This is unexpected inside a list that hasn't been closed.
                    set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unexpected end of input inside list (expecting element or ')')");
                }
                // If status was already set by parse_sexpr, just propagate NULL.
                return NULL;
            }

            // Append element to the list
            L0_Value* next_nil = l0_make_nil(state->arena); // Allocate terminating nil for the new pair // FIX: Use ->
            if (!next_nil) {
                 set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate NIL for list pair in parse_list"); // FIX: Pass state directly
                 return NULL;
            }
            L0_Value* new_pair = l0_make_pair(state->arena, element, next_nil); // FIX: Use ->
             if (!new_pair) {
                 set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Failed to allocate pair for list element in parse_list"); // FIX: Pass state directly
                 // element and next_nil are in arena
                 return NULL;
            }

            if (l0_is_nil(head)) {
                head = new_pair;
                tail = head;
            } else {
                 if (l0_is_pair(tail)) {
                    tail->data.pair.cdr = new_pair;
                    tail = new_pair;
                 } else {
                    // Should not happen
                    set_parser_error(state, L0_PARSE_ERROR_INVALID_SYNTAX, "Internal error: tail is not a pair in list construction"); // FIX: Pass state directly
                    // Memory managed by arena
                    return NULL;
                 }
            }
        }
    }
}

// Helper to check if a character is valid for a symbol (after the first char)
static bool is_symbol_char(char c) {
    // Allow '.' within symbols (e.g., for '...')
    return isalnum((unsigned char)c) || strchr("_+-*/=!?<>:.", c) != NULL;
}

// Helper to check if a character can start a symbol
static bool is_symbol_start_char(char c) {
     // Allow '.' as a starting character for symbols like '...'
     return isalpha((unsigned char)c) || strchr("_+-*/=!?<>:.", c) != NULL;
}

// Helper to parse a string literal, handling escapes
static L0_Value* parse_string_literal(ParserState* state) {
    assert(*state->current_pos == '"');
    state->current_pos++; // Consume opening '"'
    state->col++;
    const char* start = state->current_pos;
    unsigned long start_col = state->col;

    // First pass: find the end and calculate needed buffer size, handling escapes
    size_t content_len = 0;
    bool escaped = false;
    const char* p = start;
    while (*p != '\0') { // Loop until null terminator
        if (escaped) {
            // We are processing the character *after* a backslash
            if (*p == '\\' || *p == '"' || *p == 'n' || *p == 't') { // Valid escapes count as 1 char
                content_len++;
            } else {
                // Invalid escape sequence (e.g., "\z"), treat as literal backslash + char
                content_len += 2;
            }
            escaped = false; // Consume the escaped status
        } else { // Not currently escaped
            if (*p == '\\') {
                escaped = true; // Next character is escaped
            } else if (*p == '"') {
                break; // Found the *unescaped* closing quote - this is the end
            } else {
                content_len++; // Normal character counts as 1
            }
        }
        p++; // Move to the next character in the input string
    }
    // --- DEBUG PRINT ---
    // fprintf(stderr, "[DEBUG parse_string_literal] Calculated content_len: %zu for input starting near: %.10s...\n", content_len, start); // REMOVED DEBUG
    // --- END DEBUG ---


    if (*p != '"') {
        // Unterminated string
        set_parser_error(state, L0_PARSE_ERROR_UNEXPECTED_EOF, "Unterminated string literal");
        // Reset position for error reporting? state->current_pos is already at EOF.
        state->col = start_col; // Approximate error column
        return NULL;
    }

    // Second pass: allocate buffer and copy content, processing escapes
    // Add alignment argument
    char* buffer = l0_arena_alloc(state->arena, content_len + 1, alignof(char));
    // --- DEBUG PRINT ---
    // fprintf(stderr, "[DEBUG parse_string_literal] Allocated buffer at: %p (requested size: %zu)\n", (void*)buffer, content_len + 1);
    // --- END DEBUG ---
    if (!buffer) {
        set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Arena allocation failed for string literal content");
        return NULL;
    }

    // Find the actual end quote pointer again to use as loop limit
    const char* end_quote_ptr = start;
    bool first_pass_escaped = false;
    while (*end_quote_ptr != '\0') {
        if (first_pass_escaped) {
            first_pass_escaped = false;
        } else {
            if (*end_quote_ptr == '\\') {
                first_pass_escaped = true;
            } else if (*end_quote_ptr == '"') {
                break; // Found the unescaped closing quote
            }
        }
        end_quote_ptr++;
    }
    // 'end_quote_ptr' now points to the closing quote

    char* buf_p = buffer;
    escaped = false;
    p = start; // Reset pointer for second pass
    state->col = start_col; // Reset column for accurate tracking in second pass

    // Loop until the pointer reaches the position of the closing quote
    while (p < end_quote_ptr) {
        if (escaped) {
            switch (*p) {
                case '\\': *buf_p++ = '\\'; break;
                case '"':  *buf_p++ = '"';  break;
                case 'n':  *buf_p++ = '\n'; break; // Handle newline escape
                case 't':  *buf_p++ = '\t'; break; // Handle tab escape
                default:
                    // Invalid escape, copy both backslash and char
                    *buf_p++ = '\\';
                    *buf_p++ = *p;
                    break;
            }
            escaped = false;
        } else if (*p == '\\') {
            escaped = true;
        } else {
            *buf_p++ = *p;
        }
        p++;
        // state->col++; // Column tracking needs refinement here if strict accuracy is needed during copy
    }
    *buf_p = '\0'; // Null-terminate the buffer
    // --- DEBUG PRINT ---
    // fprintf(stderr, "[DEBUG parse_string_literal] Final buffer content before make_string: \"%s\" (len=%zu)\n", buffer, strlen(buffer));
    // --- END DEBUG ---


    // Advance state->current_pos past the closing quote
    state->current_pos = end_quote_ptr + 1;
    // Update column based on the *final* position relative to the start
    // This is simpler than tracking during the loop with escapes.
    state->col = start_col + (end_quote_ptr - start) + 1; // +1 for the closing quote


    L0_Value* str_val = l0_make_string(state->arena, buffer);
    // No need to free buffer, it's in the arena.
    if (!str_val) {
        // Error should be set by l0_make_string if internal allocation fails
        return NULL;
    }
    return str_val;
}


static L0_Value* parse_atom(ParserState* state) {
    const char* start = state->current_pos;
    unsigned long start_col = state->col;

    // Try parsing as Integer
    char* end_num_ptr;
    long int_val = strtol(start, &end_num_ptr, 10);
    if (end_num_ptr > start) { // Successfully parsed at least one digit
        // Check if the character after the number is a valid separator (whitespace, ')', EOF)
        if (*end_num_ptr == '\0' || isspace((unsigned char)*end_num_ptr) || *end_num_ptr == ')' || *end_num_ptr == ';') {
            size_t len = end_num_ptr - start;
            state->current_pos = end_num_ptr;
            state->col += len;
            return l0_make_integer(state->arena, int_val); // Ensure arena passed
        }
        // If not followed by separator, it might be part of a symbol like "123abc"
        // Fall through to symbol parsing. Reset state->col as we didn't consume the number here.
        state->col = start_col;
    }

    // Try parsing as Float
    char* end_float_ptr;
    errno = 0; // Reset errno before calling strtod
    double float_val = strtod(start, &end_float_ptr);
    if (end_float_ptr > start && errno == 0) { // Successfully parsed something and no range error
        // Check if it contains '.' or 'e'/'E' to distinguish from integer
        bool is_float_syntax = false;
        for (const char* p = start; p < end_float_ptr; ++p) {
            if (*p == '.' || *p == 'e' || *p == 'E') {
                is_float_syntax = true;
                break;
            }
        }

        if (is_float_syntax) {
            // Check if the character after the float is a valid separator
            if (*end_float_ptr == '\0' || isspace((unsigned char)*end_float_ptr) || *end_float_ptr == ')' || *end_float_ptr == ';') {
                size_t len = end_float_ptr - start;
                state->current_pos = end_float_ptr;
                state->col += len;
                return l0_make_float(state->arena, float_val); // Ensure arena passed
            }
            // If not followed by separator, it might be part of a symbol like "1.2abc"
            // Fall through to symbol parsing. Reset state->col as we didn't consume the float here.
            state->col = start_col;
        }
        // If it parsed as a number but didn't have float syntax, let integer parsing handle it (already tried)
        // or let it fall through to symbol parsing if integer parsing failed the separator check.
    }
    // Reset errno just in case it was set by strtod but we didn't use the result
    errno = 0;


    // Try parsing as Boolean (#t or #f)
    if (strncmp(start, "#t", 2) == 0) {
         char next_char = *(start + 2);
         if (next_char == '\0' || isspace((unsigned char)next_char) || next_char == ')' || next_char == ';') {
            state->current_pos += 2;
            state->col += 2;
            return l0_make_boolean(state->arena, true); // Ensure arena passed
         }
    } else if (strncmp(start, "#f", 2) == 0) {
         char next_char = *(start + 2);
         if (next_char == '\0' || isspace((unsigned char)next_char) || next_char == ')' || next_char == ';') {
            state->current_pos += 2;
            state->col += 2;
            return l0_make_boolean(state->arena, false); // Ensure arena passed
         }
    }

    // Try parsing as Symbol
    if (is_symbol_start_char(*start)) {
        const char* end = start + 1;
        while (*end != '\0' && is_symbol_char(*end)) {
            end++;
        }
        // Check if the character after the symbol is a valid separator
         if (*end == '\0' || isspace((unsigned char)*end) || *end == ')' || *end == ';') {
            size_t len = end - start;
            // Allocate temporary buffer in arena to hold the symbol name before passing to l0_make_symbol
            // (l0_make_symbol will duplicate it again within the arena)
            // Alternatively, l0_make_symbol could take start/len directly if optimized.
            // Add alignment argument
            char* temp_symbol_name = l0_arena_alloc(state->arena, len + 1, alignof(long double));
            if (!temp_symbol_name) {
                 set_parser_error(state, L0_PARSE_ERROR_MEMORY, "Arena allocation failed for temporary symbol name buffer");
                 return NULL;
            }
            strncpy(temp_symbol_name, start, len);
            temp_symbol_name[len] = '\0';

            state->current_pos = end;
            state->col += len;

            L0_Value* sym_val = l0_make_symbol(state->arena, temp_symbol_name); // Ensure arena passed
            // No need to free temp_symbol_name, it's in the arena.
            if (!sym_val) {
                // Error already set by l0_make_symbol if allocation failed
                return NULL;
            }
            return sym_val;
         }
         // If not followed by separator, maybe invalid syntax? Fall through.
    }

    // If none of the above match, it's an invalid atom
    set_parser_error(state, L0_PARSE_ERROR_INVALID_SYNTAX, NULL); // Clear previous message
    // Find the end of the invalid token for better error message
    const char* error_end = start;
    while (*error_end != '\0' && !isspace((unsigned char)*error_end) && *error_end != '(' && *error_end != ')') {
        error_end++;
    }
    size_t error_len = error_end - start;
    // Allocate error message buffer in arena
    // Max error message length assumption (e.g., 60 chars for prefix + token length)
    size_t error_buf_len = 60 + error_len;
    // Add alignment argument
    char* error_buf = l0_arena_alloc(state->arena, error_buf_len, alignof(char)); // Align for char buffer
     if (error_buf) {
        // Copy the invalid token into the buffer first (safer than snprintf with %.*s if token is huge)
        size_t prefix_len = snprintf(error_buf, error_buf_len, "Invalid atom starting with: ");
        if (prefix_len < error_buf_len) {
            size_t remaining_len = error_buf_len - prefix_len;
            size_t copy_len = (error_len < remaining_len - 1) ? error_len : remaining_len - 1;
            strncpy(error_buf + prefix_len, start, copy_len);
            error_buf[prefix_len + copy_len] = '\0';
        }
        // Set the persistent error message pointer
        l0_parser_error_message = error_buf;
     } else {
        // Fallback if even error buffer allocation fails
        l0_parser_error_message = "Invalid atom syntax (and failed to allocate error buffer)";
     }
    // Update the error status *after* setting the message
    l0_parser_error_status = L0_PARSE_ERROR_INVALID_SYNTAX;

    return NULL;
}


// --- Memory Management Function Implementations (REMOVED) ---
// These are no longer needed as Arena manages memory.
/*
L0_Value* l0_alloc_value(L0_ValueType type) { ... }
void l0_free_value(L0_Value* value) { ... }
*/

// --- Arena-based Constructor Implementations ---
// Definitions moved to l0_types.c

// --- Type Predicate Implementations ---
// Definitions moved to l0_types.c
