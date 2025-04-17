#include "l0_codegen.h"
#include "l0_types.h"
#include "l0_arena.h"
#include "l0_env.h" // May need env functions if codegen needs type info later
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h> // For variadic string building helper
#include <assert.h>
// --- String Builder Helper ---
// Simple dynamic string builder using the arena.
typedef struct {
    char* buffer;
    size_t capacity;
    size_t length;
    L0_Arena* arena;
} StringBuilder;

// Initialize string builder
static bool sb_init(StringBuilder* sb, L0_Arena* arena, size_t initial_capacity) {
    if (!arena) return false;
    sb->arena = arena;
    sb->capacity = initial_capacity > 0 ? initial_capacity : 1024; // Default initial size
    sb->buffer = l0_arena_alloc(arena, sb->capacity, alignof(char));
    if (!sb->buffer) {
        sb->capacity = 0;
        sb->length = 0;
        return false; // Allocation failed
    }
    sb->length = 0;
    sb->buffer[0] = '\0';
    return true;
}

// Finalize string builder (return buffer, caller owns it via arena)
static char* sb_finalize(StringBuilder* sb) {
    // Optional: Trim excess capacity? Not easy with bump allocator.
    // Just return the current buffer.
    char* result = sb->buffer;
    // Invalidate the builder to prevent further use
    // sb->buffer = NULL;
    // sb->capacity = 0;
    // sb->length = 0;
    return result;
}


// Ensure capacity and reallocate if needed
static bool sb_ensure_capacity(StringBuilder* sb, size_t additional_needed) {
    if (!sb->buffer) return false; // Not initialized
    size_t required_capacity = sb->length + additional_needed + 1; // +1 for null terminator
    if (required_capacity > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        if (new_capacity < required_capacity) {
            new_capacity = required_capacity;
        }
        // IMPORTANT: Arena allocation doesn't support realloc.
        // We need to allocate a new buffer and copy. The old buffer remains in the arena.
        // This is inefficient but necessary with simple bump allocators.
        // A more sophisticated arena might offer realloc or block allocation.
        char* new_buffer = l0_arena_alloc(sb->arena, new_capacity, alignof(char));
        if (!new_buffer) {
            // Allocation failed, cannot continue appending
            // Mark the builder as unusable?
            // fprintf(stdout, "Codegen Error: Failed to reallocate string builder buffer in arena.\n"); fflush(stdout);
            return false;
        }
        // <<< ADD MEMCPY CHECKS >>>
        if (new_buffer == NULL || sb->buffer == NULL) {
            fprintf(stdout, "[FATAL SB CORRUPTION] memcpy null buffer! new_buffer=%p, sb->buffer=%p\\n", (void*)new_buffer, (void*)sb->buffer); fflush(stdout);
            exit(98);
        }
        if (sb->length >= sb->capacity || sb->length >= new_capacity) { // Check length against BOTH capacities
             fprintf(stdout, "[FATAL SB CORRUPTION] memcpy length exceeds capacity! len=%zu, old_cap=%zu, new_cap=%zu\\n", sb->length, sb->capacity, new_capacity); fflush(stdout);
             exit(97);
        }
        // Check for potential overlap (basic check, might not catch all cases)
        // Note: This check is simplified. Real overlap detection is complex.
        // We assume non-overlapping due to arena allocation strategy.
        // if ((new_buffer >= sb->buffer && new_buffer < sb->buffer + sb->length) || (sb->buffer >= new_buffer && sb->buffer < new_buffer + sb->length)) {
        //     fprintf(stdout, "[FATAL SB CORRUPTION] memcpy buffer overlap detected!\\n"); fflush(stdout);
         //     exit(96);
         // }
         // fprintf(stdout, "[DEBUG SB memcpy] src=%p, dst=%p, len=%zu, old_cap=%zu, new_cap=%zu\\n", (void*)sb->buffer, (void*)new_buffer, sb->length, sb->capacity, new_capacity); fflush(stdout); // DEBUG
         memcpy(new_buffer, sb->buffer, sb->length); // Copy existing content
         // Old sb->buffer is leaked within the arena (acceptable for bump allocator)
         sb->buffer = new_buffer;
        sb->capacity = new_capacity;
        sb->buffer[sb->length] = '\0'; // Ensure null termination after copy
    }
    return true;
}

// Append a string
static bool sb_append_str(StringBuilder* sb, const char* str) {
    if (!sb->buffer || !str) return false;
    size_t len = strlen(str);
    if (!sb_ensure_capacity(sb, len)) {
        return false;
    }
    memcpy(sb->buffer + sb->length, str, len);
    sb->length += len;
    sb->buffer[sb->length] = '\0';
    return true;
}

// Append formatted string (like sprintf)
static bool sb_append_fmt(StringBuilder* sb, const char* fmt, ...) {
    if (!sb->buffer || !fmt) return false;
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1); // Need a copy because vsnprintf might be called twice

    // <<< ADD VSNPRINTF CHECKS >>>
    if (fmt == NULL) {
        fprintf(stdout, "[FATAL SB CORRUPTION] sb_append_fmt called with NULL format string!\\n"); fflush(stdout);
        exit(95);
    }
    // Basic sanity check on format string (e.g., not excessively long, contains null terminator within reasonable bounds)
    // This is tricky, but we can check for a null terminator within a plausible distance.
    // bool fmt_ok = false; // Commented out unused variable
    for (int i = 0; i < 1024; ++i) { // Check within first 1KB
        // Check if fmt[i] is accessible; this is inherently unsafe without knowing the allocation size.
        // A crash here would also indicate a problem.
        // Let's rely on vsnprintf's own checks for now, but add a debug print.
        // if (fmt[i] == '\0') { fmt_ok = true; break; }
    }
    // if (!fmt_ok) {
    //     fprintf(stdout, "[FATAL SB CORRUPTION] sb_append_fmt format string missing null terminator or too long?\\n"); fflush(stdout);
     //     exit(94);
     // }
     // fprintf(stdout, "[DEBUG SB vsnprintf] fmt='%.50s'... \\n", fmt); fflush(stdout); // Print first 50 chars of fmt

     // Determine required size (excluding null terminator)
     int required_len = vsnprintf(NULL, 0, fmt, args1);
     va_end(args1);
      // fprintf(stdout, "[DEBUG SB vsnprintf] required_len=%d\\n", required_len); fflush(stdout); // DEBUG

     if (required_len < 0) {
         va_end(args2);
        return false; // Encoding error
    }

    if (!sb_ensure_capacity(sb, (size_t)required_len)) {
        va_end(args2);
        return false;
     }

     // Append the formatted string
      // fprintf(stdout, "[DEBUG SB vsnprintf] writing to buffer=%p + len=%zu, capacity=%zu\\n", (void*)sb->buffer, sb->length, sb->capacity); fflush(stdout); // DEBUG
     int written = vsnprintf(sb->buffer + sb->length, sb->capacity - sb->length, fmt, args2);
     va_end(args2);
      // fprintf(stdout, "[DEBUG SB vsnprintf] written=%d\\n", written); fflush(stdout); // DEBUG


     if (written < 0 || (size_t)written >= sb->capacity - sb->length) {
        // Should not happen if ensure_capacity worked, but check anyway
        return false;
    }

    sb->length += written;
    // Null terminator is handled by vsnprintf and ensure_capacity
    return true;
}


// --- Forward Declarations for Static Codegen Helpers (with depth) ---
static bool codegen_expr_c(StringBuilder* sb, L0_Value* expr, int depth); // Add depth
static bool codegen_literal_c(StringBuilder* sb, L0_Value* literal_value, int depth); // Add depth
static bool codegen_arg_list_c(StringBuilder* sb, L0_Value* arg_exprs, int depth); // Add depth
static char* escape_c_string_c(L0_Arena* arena, const char* input_str);


// --- Main Codegen Function ---

char* l0_codegen_program(L0_Arena* arena, L0_Value* ast_list) {
    // fprintf(stderr, "[DEBUG CGP] Entered l0_codegen_program\\n"); fflush(stderr); // REMOVED DEBUG
    if (!arena || !l0_is_list(ast_list)) {
        // TODO: Set global error?
        // fprintf(stderr, "[DEBUG CGP] Error: Invalid arena or ast_list\\n"); fflush(stderr); // REMOVED DEBUG
        return NULL;
    }
    // fprintf(stderr, "[DEBUG CGP] Arena and AST list OK\\n"); fflush(stderr); // REMOVED DEBUG

    StringBuilder sb;
    if (!sb_init(&sb, arena, 8192)) { // Start with 8KB buffer
         // TODO: Set global error?
        return NULL;
    }

    bool success = true;

    // Append C Boilerplate Start
    success &= sb_append_str(&sb,
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n"
        "#include <stdbool.h>\n"
        "#include \"l0_arena.h\" // Include first for l0_arena_alloc_type etc.\n"
        "#include \"l0_types.h\" // Defines L0_Value, L0_NIL etc.\n"
        "#include \"l0_parser.h\" // Needed for error status/message\n"
        "#include \"l0_env.h\"   // Defines L0_Env, l0_env_create etc.\n"
        "#include \"l0_primitives.h\" // Declares prim_add etc.\n"
        "#include \"l0_eval.h\" // Needed for potential closure application (though not used in basic codegen)\n\n"
        "// Make argc/argv globally accessible for the command-line-args primitive\n"
        "// (Defined in l0_primitives.c)\n"
        "extern int g_argc;\n"
        "extern char **g_argv;\n\n"
        "// Forward declare print helper (implementation might be linked separately or included)\n"
        "// static int l0_value_to_string_recursive(L0_Value* value, char* buffer, size_t buf_size, L0_Arena* arena, int depth);\n\n"
        "int main(int argc, char *argv[]) {\n"
        "    // Store argc/argv globally for command-line-args primitive\n"
        "    g_argc = argc;\n"
        "    g_argv = argv;\n\n"
        "    L0_Arena* arena = l0_arena_create(1024 * 1024); // 1MB initial size\n"
        "    if (!arena) { fprintf(stdout, \"Failed to create memory arena.\\n\"); fflush(stdout); return 1; }\n\n" // Changed to stdout
        "    L0_Env* env = l0_env_create(arena, NULL);\n"
        "    if (!env) { fprintf(stdout, \"Failed to create global environment.\\n\"); fflush(stdout); l0_arena_destroy(arena); return 1; }\n\n" // Changed to stdout
        "    if (!l0_register_primitives(env, arena)) {\n"
        "         fprintf(stdout, \"Failed to register primitives.\\n\"); fflush(stdout);\n" // Changed to stdout
        "         l0_arena_destroy(arena);\n"
        "         return 1;\n"
        "    }\n\n"
        "    // --- Initialize required global variables ---\n"
        "    fprintf(stdout, \"[DEBUG C main] Initializing global variables (*macro-table*, *c-declarations*, *c-exports-code*, *global-scope-id*)...\\\\n\"); fflush(stdout);\n" // Changed to stdout, escaped backslash for \n
        "    L0_Value* nil_list = L0_NIL; // Use the global NIL\n"
        "    L0_Value* zero_int = l0_make_integer(arena, 0); // For *global-scope-id*\n"
        "    if (!zero_int) { fprintf(stdout, \"Failed to create zero integer for global init.\\\\n\"); fflush(stdout); l0_arena_destroy(arena); return 1; }\n" // Changed to stdout, escaped backslash for \n
        "\n"
        "    const char* global_vars[] = {\"*macro-table*\", \"*c-declarations*\", \"*c-exports-code*\", \"*global-scope-id*\"};\n"
        "    L0_Value* initial_values[] = {nil_list, nil_list, nil_list, zero_int}; // *global-scope-id* starts at 0\n"
        "    int num_globals = sizeof(global_vars) / sizeof(global_vars[0]);\n"
        "\n"
        "    for (int i = 0; i < num_globals; ++i) {\n"
        "        L0_Value* sym = l0_make_symbol(arena, global_vars[i]);\n"
        "        if (!sym) {\n"
        "            fprintf(stdout, \"Failed to create symbol for global variable '%s'.\\\\n\", global_vars[i]); fflush(stdout);\n" // Changed to stdout, escaped backslash for \n
        "            l0_arena_destroy(arena);\n"
        "            return 1;\n"
        "        }\n"
        "        if (!l0_env_define(env, sym, initial_values[i])) {\n"
        "            fprintf(stdout, \"Failed to define global variable '%s'.\\\\n\", global_vars[i]); fflush(stdout);\n" // Changed to stdout, escaped backslash for \n
        "            // Error message might be set by l0_env_define if symbol already exists, though it shouldn't here.\n"
        "            l0_arena_destroy(arena);\n"
        "            return 1;\n"
        "        }\n"
        "        fprintf(stdout, \"[DEBUG C main] Defined global '%s'.\\\\n\", global_vars[i]); fflush(stdout);\n" // Changed to stdout, escaped backslash for \n
        "    }\n"
        "    fprintf(stdout, \"[DEBUG C main] Global variables initialized.\\\\n\"); fflush(stdout);\n" // Changed to stdout, escaped backslash for \n
        "    // --- End Global Variable Initialization ---\n\n"
        "    L0_Value* last_result = L0_NIL; // Initialize\n"
        "    L0_Value* temp_result = NULL; // For individual expression results\n"
        "    int exit_code = 0; // Default success\n"
        "    (void)last_result; // Avoid unused warning for now\n\n"
    );

    // Generate code for each top-level expression
    L0_Value* current_expr_node = ast_list;
    int block_num = 0; // Counter for debug messages
    int current_depth = 0; // Start depth at 0 for top-level
    while (success && l0_is_pair(current_expr_node)) {
        L0_Value* expr = current_expr_node->data.pair.car;
        block_num++;
        current_depth = 0; // Reset depth for each top-level expression

        // Check if the top-level expression is a define
        bool is_top_level_define = false;
        if (l0_is_pair(expr) && l0_is_symbol(expr->data.pair.car)) {
            if (strcmp(expr->data.pair.car->data.symbol, "define") == 0) {
                is_top_level_define = true;
            }
        }

        // Add debug print before each block
        success &= sb_append_fmt(&sb, "    // --- Block %d: Processing top-level expression ---\n", block_num);
        success &= sb_append_fmt(&sb, "    fprintf(stdout, \"[DEBUG C main] Executing Block %d...\\n\"); fflush(stdout); // DEBUG\n", block_num); // Changed to stdout


        if (is_top_level_define) {
            // Generate define code directly (it handles its own environment modification)
            // codegen_expr_c for define should NOT be wrapped in ({...}) when called for top-level
            success &= sb_append_str(&sb, "    "); // Indentation
            success &= codegen_expr_c(&sb, expr, current_depth + 1); // Pass depth + 1
            success &= sb_append_str(&sb, "\n"); // Newline after define block
            // Define evaluates to NIL, set temp_result accordingly for the error check below
            success &= sb_append_str(&sb, "    temp_result = L0_NIL; // Define returns NIL\n");
        } else {
            // Generate expression code into temp_result for other forms
            success &= sb_append_str(&sb, "    temp_result = ");
            success &= codegen_expr_c(&sb, expr, current_depth + 1); // Pass depth + 1
            success &= sb_append_str(&sb, ";\n");
        }

        // <<< ADD BLOCK_NUM INTEGRITY CHECK >>>
        success &= sb_append_fmt(&sb, "    if (%d != %d) { fprintf(stdout, \"[FATAL CODEGEN CORRUPTION] block_num mismatch after codegen_expr_c! Expected %d, got %%d\\n\", %d); fflush(stdout); exit(99); } // Check block_num integrity\n", block_num, block_num, block_num, block_num); // Changed to stdout
            // Check if evaluation failed (l0_parser_error_status is the primary check now)
            // temp_result might be NULL from codegen_expr_c internal errors too
            success &= sb_append_str(&sb,
               "    if (temp_result == NULL || l0_parser_error_status != L0_PARSE_OK) {\n"
               // Removed %d from the error message
               "        fprintf(stdout, \"[DEBUG C main] Error encountered after processing a block.\\n\"); fflush(stdout); // DEBUG\n" // Changed to stdout
               "        fprintf(stdout, \"Runtime Error: %s\\n\", l0_parser_error_message ? l0_parser_error_message : \"(unknown)\"); fflush(stdout);\n" // Changed to stdout
               "        last_result = NULL; // Mark overall result as error\n"
               "        exit_code = 1;\n"
               "        goto cleanup; // Skip remaining expressions\n"
               "    }\n"
               "    last_result = temp_result; // Store successful result\n"
               // Removed %d from the success message
               "    fprintf(stdout, \"[DEBUG C main] Block finished successfully.\\n\"); fflush(stdout); // DEBUG\n" // Changed to stdout
            );
            current_expr_node = current_expr_node->data.pair.cdr;
        }

    // Check if the list was proper
    if (success && !l0_is_nil(current_expr_node)) {
        // TODO: Set error - input was not a proper list
        // fprintf(stdout, "Codegen Error: Input AST list was not a proper list.\n"); fflush(stdout);
        success = false;
    }

    // Append C Boilerplate End
    success &= sb_append_str(&sb,
        "\n"
        "cleanup: // Label for jumping on error\n"
        "    // --- MODIFIED: Don't print L0 result, just use exit_code --- \n"
        "    // if (last_result == NULL) {\n"
        "    //     printf(\"Result: <RUNTIME_ERROR>\\n\");\n"
        "    // } else {\n"
        "    //     char print_buffer[1024];\n"
        "    //     l0_value_to_string_recursive(last_result, print_buffer, sizeof(print_buffer), arena, 0);\n"
        "    //     printf(\"Result: %s\\n\", print_buffer);\n"
        "    // }\n"
        "    // --- END MODIFICATION --- \n\n"
        "    fprintf(stdout, \"[DEBUG C main] Reached cleanup. exit_code = %d\\n\", exit_code); fflush(stdout);\n" // Add debug print
        "    l0_arena_destroy(arena); \n"
        "    fprintf(stdout, \"[DEBUG C main] Arena destroyed. Returning %d\\n\", exit_code); fflush(stdout);\n" // Add debug print
        "    return exit_code;\n" // Return 0 on success, 1 on error
        "}\n\n"
        "// Note: l0_value_to_string_recursive is defined in l0_primitives.c and should be linked.\n"
    );


    if (!success) {
        // Cleanup or indicate error? Arena handles memory.
        // Maybe return NULL or a specific error string?
        // fprintf(stdout, "Codegen Error: Failed during code generation.\n"); fflush(stdout);
        return NULL; // Indicate failure
    }

    return sb_finalize(&sb);
}


// --- Static Codegen Helper Implementations (with depth) ---

// Helper to escape C strings (allocates in arena)
static char* escape_c_string_c(L0_Arena* arena, const char* input_str) {
    if (!input_str) return NULL;
    size_t len = strlen(input_str);
    size_t escaped_len = 0;
    // Calculate required length
    for (size_t i = 0; i < len; ++i) {
        if (input_str[i] == '\\' || input_str[i] == '"') {
            escaped_len += 2;
        } else {
            escaped_len += 1;
        }
    }

    char* buffer = l0_arena_alloc(arena, escaped_len + 1, alignof(char));
    if (!buffer) return NULL;

    char* p_out = buffer;
    for (size_t i = 0; i < len; ++i) {
        if (input_str[i] == '\\' || input_str[i] == '"') {
            *p_out++ = '\\';
            *p_out++ = input_str[i];
        } else {
            *p_out++ = input_str[i];
        }
    }
    *p_out = '\0';
    return buffer;
}

// Helper to escape C string literals (handles ", \, \n)
// No depth needed here
static char* escape_c_string_literal(L0_Arena* arena, const char* input_str) {
    if (!input_str) return NULL;
    size_t len = strlen(input_str);
    size_t escaped_len = 0;
    // Calculate required length
    for (size_t i = 0; i < len; ++i) {
        char c = input_str[i];
        if (c == '\\' || c == '"' || c == '\n') {
            escaped_len += 2; // Needs backslash escape
        } else {
            escaped_len += 1;
        }
    }

    char* buffer = l0_arena_alloc(arena, escaped_len + 1, alignof(char));
    if (!buffer) return NULL;

    char* p_out = buffer;
    for (size_t i = 0; i < len; ++i) {
        char c = input_str[i];
        if (c == '\\') {
            *p_out++ = '\\';
            *p_out++ = '\\';
        } else if (c == '"') {
            *p_out++ = '\\';
            *p_out++ = '"';
        } else if (c == '\n') {
            *p_out++ = '\\';
            *p_out++ = 'n';
        }
         else {
            *p_out++ = c;
        }
    }
    *p_out = '\0';
    return buffer;
}


static bool codegen_literal_c(StringBuilder* sb, L0_Value* literal_value, int depth) { // Add depth param
    if (!literal_value) return false;
    // Add depth limit check
    if (depth > 100) { // Arbitrary limit to prevent stack overflow
        fprintf(stdout, "[FATAL CODEGEN] Exceeded max recursion depth (%d) in codegen_literal_c\\n", depth); fflush(stdout); // Changed to stdout
        // return sb_append_str(sb, "/* ERROR: Max recursion depth exceeded */ NULL"); // OLD: Inserted comment
        return false; // NEW: Return false on error
    }

    // Debug print at the start of the function
    // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] Processing type %d\\n", depth, literal_value->type); fflush(stdout);

    switch (literal_value->type) {
        case L0_TYPE_NIL:
            return sb_append_str(sb, "L0_NIL");
        case L0_TYPE_BOOLEAN:
            return sb_append_fmt(sb, "l0_make_boolean(arena, %s)", literal_value->data.boolean ? "true" : "false");
        case L0_TYPE_INTEGER:
            return sb_append_fmt(sb, "l0_make_integer(arena, %ldL)", literal_value->data.integer); // Add L suffix for long
        case L0_TYPE_SYMBOL: {
            char* escaped_sym = escape_c_string_c(sb->arena, literal_value->data.symbol);
            if (!escaped_sym) return false;
            return sb_append_fmt(sb, "l0_make_symbol(arena, \"%s\")", escaped_sym);
        }
        case L0_TYPE_STRING: {
             // Use the new literal escaper
             char* escaped_str = escape_c_string_literal(sb->arena, literal_value->data.string);
             if (!escaped_str) return false;
            return sb_append_fmt(sb, "l0_make_string(arena, \"%s\")", escaped_str);
        }
         case L0_TYPE_FLOAT:
            // Use %g for a reasonable default representation of double
            // Use %.17g if full precision is strictly required for round-tripping
            return sb_append_fmt(sb, "l0_make_float(arena, %g)", literal_value->data.float_num); // Use float_num here
          case L0_TYPE_PAIR: { // Handle quoted lists like '(1 2)
             // This requires recursively building the C code for pairs
             // size_t len_before = sb->length; // Commented out unused variable
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR start: car_type=%d, cdr_type=%d. SB len=%zu, cap=%zu\\n", // Changed to stdout
             //         depth,
             //         literal_value->data.pair.car ? literal_value->data.pair.car->type : -1,
             //         literal_value->data.pair.cdr ? literal_value->data.pair.cdr->type : -1,
             //         sb->length, sb->capacity);
             // fflush(stdout); // Added flush

             bool success = sb_append_fmt(sb, "l0_make_pair(arena, ");
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR after make_pair open. SB len=%zu\\n", depth, sb->length); fflush(stdout); // Changed to stdout

             // fprintf(stderr, "[DEBUG CLC %d] Recursing for CAR\\n", depth); fflush(stderr); // REMOVED DEBUG
             success = success && codegen_literal_c(sb, literal_value->data.pair.car, depth + 1); // Recurse for car with depth+1
             // fprintf(stderr, "[DEBUG CLC %d] Returned from CAR. Success=%d\\n", depth, success); fflush(stderr); // REMOVED DEBUG
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR after car recursion. Success=%d. SB len=%zu\\n", depth, success, sb->length); fflush(stdout); // Changed to stdout

             success = success && sb_append_str(sb, ", ");
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR after comma. Success=%d. SB len=%zu\\n", depth, success, sb->length); fflush(stdout); // Changed to stdout

             // fprintf(stderr, "[DEBUG CLC %d] Recursing for CDR\\n", depth); fflush(stderr); // REMOVED DEBUG
             success = success && codegen_literal_c(sb, literal_value->data.pair.cdr, depth + 1); // Recurse for cdr with depth+1
             // fprintf(stderr, "[DEBUG CLC %d] Returned from CDR. Success=%d\\n", depth, success); fflush(stderr); // REMOVED DEBUG
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR after cdr recursion. Success=%d. SB len=%zu\\n", depth, success, sb->length); fflush(stdout); // Changed to stdout

             success = success && sb_append_str(sb, ")");
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR after close paren. Success=%d. SB len=%zu\\n", depth, success, sb->length); fflush(stdout); // Changed to stdout

             // size_t len_after = sb->length; // Commented out unused variable
             // fprintf(stdout, "[DEBUG codegen_literal_c depth=%d] PAIR end: success=%d. SB len before=%zu, after=%zu, cap=%zu\\n", depth, success, len_before, len_after, sb->capacity); fflush(stdout); // Changed to stdout
             return success;
          }
         default:
             // Cannot generate literal for other types (primitives, closures)
             // fprintf(stdout, "[WARN codegen_literal_c depth=%d] Cannot generate literal for type %d\\n", depth, literal_value->type); fflush(stdout); // Changed to stdout
             // return sb_append_str(sb, "/* ERROR: Cannot generate literal for this type */"); // OLD: Inserted comment
             return false; // NEW: Return false on error
     }
 }

static bool codegen_arg_list_c(StringBuilder* sb, L0_Value* arg_exprs, int depth) { // Add depth param
     if (l0_is_nil(arg_exprs)) {
        return sb_append_str(sb, "L0_NIL");
    }
    // Add depth limit check
    if (depth > 100) {
        fprintf(stdout, "[FATAL CODEGEN] Exceeded max recursion depth (%d) in codegen_arg_list_c\\n", depth); fflush(stdout); // Changed to stdout
        // return sb_append_str(sb, "/* ERROR: Max recursion depth exceeded */ NULL"); // OLD: Inserted comment
        return false; // NEW: Return false on error
    }
    if (!l0_is_pair(arg_exprs)) {
        // Should not happen for valid L0 code
        // return sb_append_str(sb, "/* ERROR: Invalid argument list structure */"); // OLD: Inserted comment
        return false; // NEW: Return false on error
    }

    // Generate code for the first argument
    bool success = sb_append_str(sb, "l0_make_pair(arena, ");
    if (success) success = codegen_expr_c(sb, arg_exprs->data.pair.car, depth + 1); // Generate code for car with depth+1
    if (success) success = sb_append_str(sb, ", ");
    if (success) success = codegen_arg_list_c(sb, arg_exprs->data.pair.cdr, depth + 1); // Recurse for cdr with depth+1
    if (success) success = sb_append_str(sb, ")");
    return success;
}


static bool codegen_expr_c(StringBuilder* sb, L0_Value* expr, int depth) { // Add depth param
    if (!expr) return false;
    // Add depth limit check
    if (depth > 100) {
        fprintf(stdout, "[FATAL CODEGEN] Exceeded max recursion depth (%d) in codegen_expr_c\\n", depth); fflush(stdout); // Changed to stdout
        // return sb_append_str(sb, "/* ERROR: Max recursion depth exceeded */ NULL"); // OLD: Inserted comment
        return false; // NEW: Return false on error
    }

    // Debug print at the start of the function
    // fprintf(stdout, "[DEBUG codegen_expr_c depth=%d] Processing type %d\\n", depth, expr->type); fflush(stdout);


    switch (expr->type) {
        case L0_TYPE_NIL:
        case L0_TYPE_BOOLEAN:
        case L0_TYPE_INTEGER:
        case L0_TYPE_STRING:
        case L0_TYPE_FLOAT: // Add FLOAT here
            // Literals (except symbols) are handled directly
            return codegen_literal_c(sb, expr, depth + 1); // Pass depth + 1

        case L0_TYPE_SYMBOL: {
            // Generate symbol lookup (no significant recursion here)
            char* escaped_sym = escape_c_string_c(sb->arena, expr->data.symbol);
            if (!escaped_sym) return false;
            // Assumes 'env' and 'arena' are available in the generated C code context
            return sb_append_fmt(sb, "l0_env_lookup(env, l0_make_symbol(arena, \"%s\"))", escaped_sym);
        }

        case L0_TYPE_PAIR: {
            // Handle special forms and function calls
            if (!l0_is_pair(expr)) return false; // Should always be true here, but check
            L0_Value* op_val = expr->data.pair.car;
            L0_Value* args = expr->data.pair.cdr;

            // const char* op = op_val->data.symbol; // Removed: op_val might not be a symbol

            // --- Special Forms ---
            // Check if op_val is a symbol before comparing for special forms
            if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "quote") == 0) {
                if (!l0_is_pair(args) || !l0_is_nil(args->data.pair.cdr)) {
                    // return sb_append_str(sb, "/* ERROR: quote requires exactly one argument */"); // OLD: Inserted comment
                    return false; // NEW: Return false on error
                }
                return codegen_literal_c(sb, args->data.pair.car, depth + 1); // Pass depth + 1
            }
            else if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "if") == 0) {
                 // (if cond then else?)
                 // ... (argument checking) ...
                 if (!l0_is_pair(args) || !l0_is_pair(args->data.pair.cdr)) {
                     // return sb_append_str(sb, "/* ERROR: 'if' requires at least condition and then branch */"); // OLD: Inserted comment
                     return false; // NEW: Return false on error
                 }
                 L0_Value* cond_expr = args->data.pair.car;
                 L0_Value* then_expr = args->data.pair.cdr->data.pair.car;
                 L0_Value* else_expr = NULL; // Use NULL to indicate no else branch initially
                 bool has_else = false;
                 if (l0_is_pair(args->data.pair.cdr->data.pair.cdr)) { // Check if else branch exists
                     else_expr = args->data.pair.cdr->data.pair.cdr->data.pair.car;
                     has_else = true;
                     // Check for too many arguments
                     if (!l0_is_nil(args->data.pair.cdr->data.pair.cdr->data.pair.cdr)) {
                         // return sb_append_str(sb, "/* ERROR: 'if' takes at most 3 arguments */"); // OLD: Inserted comment
                         return false; // NEW: Return false on error
                     }
                 }

                 // --- Generate C if/else block using a block expression ---
                 bool success = sb_append_str(sb, "({ L0_Value* cond_val = "); // Start block expr
                 if (success) success = codegen_expr_c(sb, cond_expr, depth + 1);      // Generate condition code with depth+1
                 if (success) success = sb_append_str(sb, "; L0_Value* if_res = L0_NIL; "); // Declare result var
                 if (success) success = sb_append_str(sb, "if (L0_IS_TRUTHY(cond_val)) { if_res = "); // Start C if
                 if (success) success = codegen_expr_c(sb, then_expr, depth + 1);      // Generate then branch code with depth+1
                 if (success) success = sb_append_str(sb, "; }");                      // End then branch

                 if (has_else) {
                     if (success) success = sb_append_str(sb, " else { if_res = ");        // Start C else
                     if (success) success = codegen_expr_c(sb, else_expr, depth + 1);      // Generate else branch code with depth+1
                     if (success) success = sb_append_str(sb, "; }");                      // End else branch
                 } else {
                     // If no else branch, if_res remains L0_NIL if condition is false
                     if (success) success = sb_append_str(sb, " else { if_res = L0_NIL; }"); // Explicitly set to NIL
                 }

                 if (success) success = sb_append_str(sb, " if_res; })"); // Return result, end block expr
                 return success;
             }
             else if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "begin") == 0) { // <<< ADD CHECK FOR BEGIN HERE
                 if (l0_is_nil(args)) {
                     return sb_append_str(sb, "L0_NIL"); // (begin) -> NIL
                 }
                 if (!l0_is_list(args)) {
                      // return sb_append_str(sb, "/* ERROR: 'begin' arguments must form a proper list */"); // OLD: Inserted comment
                      return false; // NEW: Return false on error
                 }
                 // Use C comma operator for sequencing
                 bool first = true;
                 bool success = sb_append_str(sb, "("); // Wrap in parentheses
                 L0_Value* current = args;
                 while (success && l0_is_pair(current)) {
                     if (!first) {
                         success &= sb_append_str(sb, ", ");
                     }
                     success &= codegen_expr_c(sb, current->data.pair.car, depth + 1); // Pass depth + 1
                     first = false;
                     current = current->data.pair.cdr;
                 }
                 success &= sb_append_str(sb, ")");
                 return success;
             }
             else if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "define") == 0) {
                  // fprintf(stdout, "[DEBUG define depth=%d] Entered define block. args type=%d. Arena=%p\\n", depth, args ? (int)args->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout // Cast type to int
                  // Check if args is a pair (basic check for structure)
                  if (!l0_is_pair(args)) {
                      fprintf(stdout, "[FATAL define depth=%d] args is not a pair! type=%d. Arena=%p\\n", depth, args ? (int)args->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout // Cast type to int
                      // return sb_append_str(sb, "/* ERROR: Invalid 'define' form, requires arguments */"); // OLD: Inserted comment
                      return false; // NEW: Return false on error
                  }
                  // fprintf(stdout, "[DEBUG define depth=%d] args is a pair. Checking args->car type. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout

                  // Handle (define (func args...) body...) syntax sugar
                  L0_Value* define_target = args->data.pair.car; // Get the first argument
                  // fprintf(stdout, "[DEBUG define depth=%d] define_target type=%d. Arena=%p\\n", depth, define_target ? define_target->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout

                  if (l0_is_pair(define_target)) { // Check if it's the (func ...) form
                     // fprintf(stdout, "[DEBUG define depth=%d] Handling (define (func ...)...) form. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                      // Transform (define (f x) y) into (define f (lambda (x) y))
                      L0_Value* signature = define_target; // Already checked it's a pair
                      if (!l0_is_pair(signature) || !l0_is_symbol(signature->data.pair.car)) { // Re-check inner structure for safety
                          fprintf(stdout, "[FATAL define depth=%d] Invalid function signature structure. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                          // return sb_append_str(sb, "/* ERROR: Invalid function signature in define */"); // OLD: Inserted comment
                          return false; // NEW: Return false on error
                      }
                      L0_Value* func_sym = signature->data.pair.car; // Already checked it's a symbol
                      L0_Value* params = signature->data.pair.cdr;
                      L0_Value* body_exprs = args->data.pair.cdr;

                      // fprintf(stdout, "[DEBUG define func depth=%d] Parsed func_sym, params (type=%d), body (type=%d). Arena=%p\\n", depth, params ? (int)params->type : -1, body_exprs ? (int)body_exprs->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout // Cast types to int


                      if (!l0_is_list(params)) {
                          fprintf(stdout, "[FATAL define func depth=%d] params is not a list! type=%d. Arena=%p\\n", depth, params ? (int)params->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout // Cast type to int
                          // return sb_append_str(sb, "/* ERROR: Function parameters must be a proper list in define */"); // OLD: Inserted comment
                          return false; // NEW: Return false on error
                      }
                      if (l0_is_nil(body_exprs)) {
                          fprintf(stdout, "[FATAL define func depth=%d] body_exprs is nil! Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                           // return sb_append_str(sb, "/* ERROR: Function body cannot be empty in define */"); // OLD: Inserted comment
                           return false; // NEW: Return false on error
                      }
                      // Check if body_exprs is at least a pair (list structure)
                      if (!l0_is_pair(body_exprs)) {
                          fprintf(stdout, "[FATAL define func depth=%d] body_exprs is not a pair (list)! type=%d. Arena=%p\\n", depth, body_exprs ? (int)body_exprs->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout // Cast type to int
                          // return sb_append_str(sb, "/* ERROR: Function body must be a list of expressions */"); // OLD: Inserted comment
                          return false; // NEW: Return false on error
                      }

                      // Construct the lambda expression: (lambda params body...)
                      // fprintf(stdout, "[DEBUG define func depth=%d] Constructing lambda AST. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                      L0_Value* lambda_sym = l0_make_symbol(sb->arena, "lambda");
                      if (!lambda_sym) { fprintf(stdout, "[FATAL define func depth=%d] Failed to make lambda_sym. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); return false; } // Changed to stdout
                      // fprintf(stdout, "[DEBUG define func depth=%d] lambda_sym created. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout

                      L0_Value* lambda_args = l0_make_pair(sb->arena, params, body_exprs);
                      if (!lambda_args) { fprintf(stdout, "[FATAL define func depth=%d] Failed to make lambda_args. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); return false; } // Changed to stdout
                      // fprintf(stdout, "[DEBUG define func depth=%d] lambda_args created. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout

                      L0_Value* lambda_expr = l0_make_pair(sb->arena, lambda_sym, lambda_args);
                      if (!lambda_expr) { fprintf(stdout, "[FATAL define func depth=%d] Failed to make lambda_expr. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); return false; } // Changed to stdout
                      // fprintf(stdout, "[DEBUG define func depth=%d] lambda_expr created: type=%d. Arena=%p\\n", depth, lambda_expr->type, (void*)sb->arena); fflush(stdout); // Changed to stdout


                      // --- Generate code for (define func lambda_expr) ---
                      // 1. Generate code for the lambda expression itself into a temporary variable
                      // 2. Generate code to define the symbol using the temporary variable

                     // --- Generate code for (define (func ...) ...) ---
                     // Generate the lambda creation and the define call directly.
                     // No ({...}) wrapper needed as 'define' itself doesn't return a value in C.
                     bool success = true;
                     char* escaped_func_sym = escape_c_string_c(sb->arena, func_sym->data.symbol);
                     if (!escaped_func_sym) return false;

                      // 1. Generate lambda creation into a temporary C variable
                      success &= sb_append_str(sb, "{ L0_Value* lambda_val = "); // Use a simple C block for scope
                      // fprintf(stdout, "[DEBUG define func depth=%d] Before recursive codegen_expr_c for lambda. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                      success &= codegen_expr_c(sb, lambda_expr, depth + 1); // Generate lambda creation code with depth+1
                      // fprintf(stdout, "[DEBUG define func depth=%d] After recursive codegen_expr_c for lambda. Success=%d. Arena=%p\\n", depth, success, (void*)sb->arena); fflush(stdout); // Changed to stdout
                      success &= sb_append_str(sb, "; ");

                      // 2. Check if lambda creation succeeded (basic check)
                     success &= sb_append_str(sb, "if (lambda_val != NULL) { ");

                     // 3. Generate the define call
                     success &= sb_append_fmt(sb, "(void)l0_env_define(env, l0_make_symbol(arena, \"%s\"), lambda_val); ", escaped_func_sym);

                     success &= sb_append_str(sb, "} else { /* Lambda creation failed */ } }"); // Close if and block

                     // Define itself doesn't produce a C expression value here.
                     // The caller (l0_codegen_program) will handle setting temp_result = L0_NIL.
                     return success;


                  } else if (l0_is_symbol(define_target)) { // Check if it's the (define symbol ...) form
                      // fprintf(stdout, "[DEBUG define depth=%d] Handling (define symbol ...) form. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                      // --- Handle (define symbol value) ---
                      if (!l0_is_pair(args->data.pair.cdr) || !l0_is_nil(args->data.pair.cdr->data.pair.cdr)) {
                          fprintf(stdout, "[FATAL define depth=%d] Invalid (define symbol value) structure. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout
                          // return sb_append_str(sb, "/* ERROR: 'define symbol value' requires exactly two arguments */"); // OLD: Inserted comment
                          return false; // NEW: Return false on error
                      }
                      L0_Value* sym = define_target; // Already checked it's a symbol
                      L0_Value* val_expr = args->data.pair.cdr->data.pair.car;
                      // fprintf(stdout, "[DEBUG define sym depth=%d] Parsed sym, val_expr (type=%d). Arena=%p\\n", depth, val_expr ? val_expr->type : -1, (void*)sb->arena); fflush(stdout); // Changed to stdout

                      // --- Generate code for (define symbol value) ---
                      // Generate value evaluation and the define call directly.
                      bool success = true;
                      char* escaped_sym = escape_c_string_c(sb->arena, sym->data.symbol);
                      if (!escaped_sym) return false;

                      // 1. Generate value evaluation into a temporary C variable
                      success &= sb_append_str(sb, "{ L0_Value* define_val = "); // Use a simple C block for scope
                      success &= codegen_expr_c(sb, val_expr, depth + 1); // Generate value evaluation code with depth+1
                      success &= sb_append_str(sb, "; ");

                      // 2. Check if value evaluation succeeded
                      success &= sb_append_str(sb, "if (define_val != NULL && l0_parser_error_status == L0_PARSE_OK) { ");

                      // 3. Generate the define call
                      success &= sb_append_fmt(sb, "(void)l0_env_define(env, l0_make_symbol(arena, \"%s\"), define_val); ", escaped_sym);

                      success &= sb_append_str(sb, "} else { /* Value evaluation failed */ } }"); // Close if and block

                      // Define itself doesn't produce a C expression value here.
                      // The caller (l0_codegen_program) will handle setting temp_result = L0_NIL.
                      return success;
                  } else {
                       // Invalid define form
                       // Append an error comment and generate code that results in NULL to signal error
                       fprintf(stdout, "[FATAL define depth=%d] Invalid define form, target not pair or symbol. Arena=%p\\n", depth, (void*)sb->arena); fflush(stdout); // Changed to stdout // Added debug here too
                       // return sb_append_str(sb, "/* ERROR: Invalid 'define' form */ NULL"); // OLD: Inserted comment
                       return false; // NEW: Return false on error
                  }
             } // <<< THIS CLOSING BRACE IS FOR `if (strcmp(op, "define") == 0)`
             // --- CORRECTED PLACEMENT FOR LAMBDA ---
            else if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "lambda") == 0) {
                 if (!l0_is_pair(args) || !l0_is_list(args->data.pair.car) || // Params must be a list
                     !l0_is_pair(args->data.pair.cdr)) { // Must have at least one body expr
                     // return sb_append_str(sb, "/* ERROR: 'lambda' requires params list and body */"); // OLD: Inserted comment
                     return false; // NEW: Return false on error
                 }
                 L0_Value* params = args->data.pair.car;
                 L0_Value* body_exprs = args->data.pair.cdr; // This is the list of body expressions

                 // --- REMOVED effective_body logic ---
                 // The body_exprs list is passed directly to l0_make_closure.
                 // The evaluator (l0_eval_sequence) handles the sequence.

                 // --- Generate code for lambda ---
                 // 1. Generate literal code for params list into a temp var
                 // 2. Generate literal code for the body expressions list into a temp var
                 // 3. Generate call to l0_make_closure using temp vars

                 // Use C block expressions ({ ... }) to scope temp variables
                 bool success = sb_append_str(sb, "({ L0_Value* lambda_params = ")
                             && codegen_literal_c(sb, params, depth + 1) // Generate literal params list with depth+1
                             && sb_append_str(sb, "; L0_Value* lambda_body = ")
                             && codegen_literal_c(sb, body_exprs, depth + 1) // Generate literal body list directly with depth+1
                             && sb_append_str(sb, "; l0_make_closure(arena, lambda_params, lambda_body, env); })"); // Call make_closure
                 return success;
            }
            // --- ADDED: Handle quasiquote (simplified) ---
            else if (l0_is_symbol(op_val) && strcmp(op_val->data.symbol, "quasiquote") == 0) {
                // Simplified handling for bootstrap: treat like quote if arg is simple literal.
                // Assumes no unquote/unquote-splicing within the argument for now.
                if (!l0_is_pair(args) || !l0_is_nil(args->data.pair.cdr)) {
                    // return sb_append_str(sb, "/* ERROR: quasiquote requires exactly one argument */ NULL"); // OLD: Inserted comment
                    return false; // NEW: Return false on error
                }
                // Attempt to generate as literal. If the argument contains unquote etc.,
                // codegen_literal_c will likely fail or produce an error comment.
                // This is sufficient for simple cases like `()` or `symbol`.
                // fprintf(stdout, "[DEBUG codegen quasiquote depth=%d] Treating as quote for: type=%d\\n", depth, args->data.pair.car ? args->data.pair.car->type : -1); fflush(stdout); // DEBUG
                return codegen_literal_c(sb, args->data.pair.car, depth + 1); // Pass depth + 1
            }
            // --- Primitive Function Calls & General Application ---
            else { // This else follows the special form checks
                // Check if the operator is a known primitive symbol
                const char* c_func_name = NULL;
                if (l0_is_symbol(op_val)) { // Only check primitives if op is a symbol
                    const char* op_sym_name = op_val->data.symbol;
                    if (strcmp(op_sym_name, "+") == 0) c_func_name = "prim_add";
                    else if (strcmp(op_sym_name, "-") == 0) c_func_name = "prim_subtract";
                    else if (strcmp(op_sym_name, "*") == 0) c_func_name = "prim_multiply";
                    else if (strcmp(op_sym_name, "/") == 0) c_func_name = "prim_divide";
                    else if (strcmp(op_sym_name, "=") == 0) c_func_name = "prim_equal";
                    else if (strcmp(op_sym_name, "<") == 0) c_func_name = "prim_less_than";
                    else if (strcmp(op_sym_name, ">") == 0) c_func_name = "prim_greater_than";
                    else if (strcmp(op_sym_name, "cons") == 0) c_func_name = "prim_cons";
                    else if (strcmp(op_sym_name, "car") == 0) c_func_name = "prim_car";
                    else if (strcmp(op_sym_name, "cdr") == 0) c_func_name = "prim_cdr";
                    else if (strcmp(op_sym_name, "pair?") == 0) c_func_name = "prim_pair_q";
                    else if (strcmp(op_sym_name, "null?") == 0) c_func_name = "prim_null_q";
                    else if (strcmp(op_sym_name, "integer?") == 0) c_func_name = "prim_integer_q";
                    else if (strcmp(op_sym_name, "boolean?") == 0) c_func_name = "prim_boolean_q";
                    else if (strcmp(op_sym_name, "symbol?") == 0) c_func_name = "prim_symbol_q";
                    else if (strcmp(op_sym_name, "string?") == 0) c_func_name = "prim_string_q";
                    else if (strcmp(op_sym_name, "float?") == 0) c_func_name = "prim_float_q"; // <<< Add float? mapping
                    else if (strcmp(op_sym_name, "string-append") == 0) c_func_name = "prim_string_append";
                    else if (strcmp(op_sym_name, "string->symbol") == 0) c_func_name = "prim_string_to_symbol";
                    else if (strcmp(op_sym_name, "symbol->string") == 0) c_func_name = "prim_symbol_to_string";
                    else if (strcmp(op_sym_name, "print") == 0) c_func_name = "prim_print"; // <<< Added print mapping
                    else if (strcmp(op_sym_name, "read-file") == 0) c_func_name = "prim_read_file";
                    else if (strcmp(op_sym_name, "write-file") == 0) c_func_name = "prim_write_file";
                    else if (strcmp(op_sym_name, "string-length") == 0) c_func_name = "primitive_string_length";
                    else if (strcmp(op_sym_name, "string-ref") == 0) c_func_name = "primitive_string_ref";
                    else if (strcmp(op_sym_name, "substring") == 0) c_func_name = "primitive_substring";
                    else if (strcmp(op_sym_name, "number->string") == 0) c_func_name = "primitive_number_to_string";
                    else if (strcmp(op_sym_name, "eval") == 0) c_func_name = "prim_eval";
                    else if (strcmp(op_sym_name, "apply") == 0) c_func_name = "prim_apply";
                    // else if (strcmp(op_sym_name, "list") == 0) c_func_name = "prim_list"; // <<< REMOVED incorrect list mapping
                    else if (strcmp(op_sym_name, "append") == 0) c_func_name = "prim_append"; // <<< ADD append mapping
                    else if (strcmp(op_sym_name, "closure?") == 0) c_func_name = "prim_closure_p"; // <<< FIX closure? mapping
                    else if (strcmp(op_sym_name, "command-line-args") == 0) c_func_name = "prim_command_line_args";
                    else if (strcmp(op_sym_name, "parse-string") == 0) c_func_name = "prim_parse_string";
                    else if (strcmp(op_sym_name, "codegen-program") == 0) c_func_name = "prim_codegen_program";
                    else if (strcmp(op_sym_name, "get-last-error-message") == 0) c_func_name = "prim_get_last_error_message";
                    else if (strcmp(op_sym_name, "get-last-error-line") == 0) c_func_name = "prim_get_last_error_line";
                    else if (strcmp(op_sym_name, "get-last-error-col") == 0) c_func_name = "prim_get_last_error_col";
                    else if (strcmp(op_sym_name, "eval-in-compiler-env") == 0) c_func_name = "prim_eval_in_compiler_env"; // <<< ADD eval-in-compiler-env mapping
                    // ... add other primitives ...
                } // End if (l0_is_symbol(op_val))

                if (c_func_name) {
                    // Generate call to primitive C function
                    return sb_append_fmt(sb, "%s(", c_func_name)
                        && codegen_arg_list_c(sb, args, depth + 1) // Generate argument list code with depth+1
                        && sb_append_str(sb, ", env, arena)"); // Primitives receive env and arena
                } else {
                    // --- General Function Application (Operator might be an expression) ---
                    // Generate code to:
                    // 1. Evaluate the operator expression.
                    // 2. Build the argument list value.
                    // 3. Call l0_apply.
                    return sb_append_str(sb, "l0_apply(") // Call the runtime apply function
                        && codegen_expr_c(sb, op_val, depth + 1) // 1. Evaluate the operator expression
                        && sb_append_str(sb, ", ")
                        && codegen_arg_list_c(sb, args, depth + 1) // 2. Generate argument list with depth+1
                        && sb_append_str(sb, ", env, arena)"); // 3. Pass env and arena to apply
                }
            }
        } // end case L0_TYPE_PAIR

        default:
            // Should not happen for valid AST nodes
            // return sb_append_str(sb, "/* ERROR: Unknown AST node type */"); // OLD: Inserted comment
            return false; // NEW: Return false on error
    }
}
