#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/l0_arena.h"
#include "../include/l0_types.h"
#include "../include/l0_parser.h"
#include "../include/l0_codegen.h" // Include the codegen header
#include "../include/l0_primitives.h" // <<< ADDED for l0_value_to_string_recursive
#include "../include/l0_env.h"      // <<< ADDED for environment handling
#include "../include/l0_eval.h"     // <<< ADDED for macro expansion

// Forward declaration for error reset function
static void repl_reset_error_state();

// Helper function to read an entire file into a string buffer (allocates in arena)
static char* read_file_content(L0_Arena* arena, const char* filename) {
    fprintf(stderr, "[DEBUG C read_file_content] Entry. filename=%s\n", filename); fflush(stderr); // DEBUG
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("fopen");
        fprintf(stderr, "Error: Could not open input file '%s'\n", filename);
        return NULL;
    }
    fprintf(stderr, "[DEBUG C read_file_content] fopen successful for %s\n", filename); fflush(stderr); // DEBUG

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fprintf(stderr, "[DEBUG C read_file_content] fseek/ftell successful. file_size=%ld\n", file_size); fflush(stderr); // DEBUG

    if (file_size < 0) {
        perror("ftell/fseek");
        fprintf(stderr, "Error: Could not determine size of file '%s'\n", filename);
        fclose(file);
        return NULL;
    }

    // Allocate buffer in arena (+1 for null terminator)
    char* buffer = l0_arena_alloc(arena, file_size + 1, alignof(char));
    if (!buffer) {
        fprintf(stderr, "Error: Arena allocation failed for reading file '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    fprintf(stderr, "[DEBUG C read_file_content] l0_arena_alloc successful. buffer=%p\n", (void*)buffer); fflush(stderr); // DEBUG

    size_t bytes_read = fread(buffer, 1, file_size, file);
    fprintf(stderr, "[DEBUG C read_file_content] fread finished. bytes_read=%zu\n", bytes_read); fflush(stderr); // DEBUG
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Error: Failed to read entire file '%s' (read %zu bytes, expected %ld)\n", filename, bytes_read, file_size);
        // Buffer is allocated in arena, no need to free manually
        return NULL;
    }

    buffer[file_size] = '\0'; // Null-terminate
    fprintf(stderr, "[DEBUG C read_file_content] Returning buffer. buffer=%p\n", (void*)buffer); fflush(stderr); // DEBUG
    return buffer;
}

// Helper function to write string content to a file
static bool write_file_content(const char* filename, const char* content) {
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("fopen");
        fprintf(stderr, "Error: Could not open output file '%s' for writing\n", filename);
        return false;
    }

    if (content) { // Handle case where codegen might return NULL
        size_t content_len = strlen(content);
        size_t bytes_written = fwrite(content, 1, content_len, file);
        fclose(file); // Close file regardless of write success

        if (bytes_written != content_len) {
            fprintf(stderr, "Error: Failed to write entire content to file '%s' (wrote %zu bytes, expected %zu)\n", filename, bytes_written, content_len);
            return false;
        }
    } else {
        // If content is NULL (codegen failed), write nothing and close.
        fclose(file);
        fprintf(stderr, "Warning: Generated code was NULL, writing empty output file '%s'\n", filename);
        // Still return true as we handled the condition, but the output is empty.
        // Or return false? Let's return false to indicate the overall process failed.
        return false;
    }

    return true;
}


// --- Compiler Driver Entry Point ---

// Make argc/argv globally accessible for the command-line-args primitive
// (Defined in l0_primitives.c)
extern int g_argc;
extern char **g_argv;

int main(int argc, char* argv[]) {
    fprintf(stderr, "[DEBUG C MAIN] Entry Point. argc=%d\n", argc); fflush(stderr); // DEBUG
    // Store argc/argv globally
    g_argc = argc;
    g_argv = argv;

    // --- Argument Parsing ---
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_l0_file> <output_c_file>\n", argv[0]);
        return 1;
    }
    const char* input_filename = argv[1];
    const char* output_filename = argv[2];

    // --- Initialization ---
    L0_Arena* arena = l0_arena_create(16 * 1024 * 1024); // 16MB arena for compilation (Increased size)
    if (!arena) {
        fprintf(stderr, "Fatal: Failed to create compiler arena.\n");
        return 1;
    }

    // --- Read Input File ---
    printf("Reading input file: %s\n", input_filename);
    fprintf(stderr, "[DEBUG C MAIN] Before calling read_file_content for %s\n", input_filename); fflush(stderr); // DEBUG
    char* input_content = read_file_content(arena, input_filename);
    fprintf(stderr, "[DEBUG C MAIN] After calling read_file_content. input_content=%p\n", (void*)input_content); fflush(stderr); // DEBUG
    if (!input_content) {
        l0_arena_destroy(arena);
        return 1;
    }
    printf("Read %zu bytes.\n", strlen(input_content));

    // --- Parse L0 Code ---
    printf("Parsing L0 code...\n");
    repl_reset_error_state(); // Reset parser error state
    // fprintf(stderr, "[DEBUG main] Before calling l0_parse_string_all. arena=%p, input_content=%p, filename=%s\n", (void*)arena, (void*)input_content, input_filename); fflush(stderr); // REMOVED DEBUG
    L0_Value* ast_list = l0_parse_string_all(arena, input_content, input_filename);
    // <<< ADD DEBUG PRINT IMMEDIATELY AFTER PARSE >>>
    // fprintf(stderr, "[DEBUG main] After l0_parse_string_all: ast_list=%p, status=%d\n", (void*)ast_list, l0_parser_error_status); fflush(stderr); // REMOVED DEBUG

    if (!ast_list || l0_parser_error_status != L0_PARSE_OK) {
        // fprintf(stderr, "[DEBUG main] Entering parse error block.\n"); fflush(stderr); // REMOVED DEBUG
        fprintf(stderr, "Parse Error (%s:%lu:%lu): %s\n",
                input_filename, l0_parser_error_line, l0_parser_error_col,
                l0_parser_error_message ? l0_parser_error_message : "Unknown parse error");
        l0_arena_destroy(arena);
        return 1;
    }
    printf("Parsing successful.\n");

    // --- Setup Compiler Environment ---
    printf("Setting up compiler environment...\n");
    L0_Env* compiler_env = l0_env_create(arena, NULL); // Use l0_env_create with NULL outer for global env
    if (!compiler_env) {
        fprintf(stderr, "Fatal: Failed to create compiler environment.\n");
        l0_arena_destroy(arena);
        return 1;
    }
    // Initialize *macro-table* to nil
    L0_Value* macro_table_sym = l0_make_symbol(arena, "*macro-table*");
    L0_Value* nil_val = l0_make_nil(arena);
    if (!macro_table_sym || !nil_val || !l0_env_define(compiler_env, macro_table_sym, nil_val)) {
         fprintf(stderr, "Fatal: Failed to initialize *macro-table* in compiler environment.\n");
         l0_arena_destroy(arena);
         return 1;
    }
    // Register primitives (needed for macro transformers)
    if (!l0_register_primitives(compiler_env, arena)) {
         fprintf(stderr, "Fatal: Failed to register primitives in compiler environment.\n");
         l0_arena_destroy(arena);
         return 1;
    }
    printf("Compiler environment ready.\n");


    // --- Macro Expansion ---
    printf("Expanding macros...\n");
    repl_reset_error_state(); // Reset error state before expansion
    L0_Value* expanded_ast_list = l0_macroexpand(ast_list, compiler_env, arena);
    if (!expanded_ast_list || l0_parser_error_status != L0_PARSE_OK) {
        fprintf(stderr, "Macro Expansion Error (%s:%lu:%lu): %s\n",
                input_filename, l0_parser_error_line, l0_parser_error_col, // Use parser error vars for now
                l0_parser_error_message ? l0_parser_error_message : "Unknown macro expansion error");
        l0_arena_destroy(arena);
        return 1;
    }
     printf("Macro expansion successful.\n");


    // --- Generate C Code ---
    printf("Generating C code...\n");
    // <<< ADD DEBUG PRINT BEFORE CODEGEN CALL >>>
    // fprintf(stderr, "[DEBUG main] Before calling l0_codegen_program: expanded_ast_list=%p\n", (void*)expanded_ast_list);
    // if (expanded_ast_list) {
    //     fprintf(stderr, "[DEBUG main] ast_list type: %d\n", ast_list->type); // REMOVED DEBUG
    //     if (l0_is_pair(ast_list)) { // REMOVED DEBUG
    //          fprintf(stderr, "[DEBUG main] ast_list is PAIR. car_type=%d, cdr_type=%d\n", // REMOVED DEBUG
    //              ast_list->data.pair.car ? ast_list->data.pair.car->type : -1, // REMOVED DEBUG
    //              ast_list->data.pair.cdr ? ast_list->data.pair.cdr->type : -1); // REMOVED DEBUG
    //     } else if (l0_is_nil(ast_list)) { // REMOVED DEBUG
    //          fprintf(stderr, "[DEBUG main] ast_list is NIL.\n"); // REMOVED DEBUG
    //     } else { // REMOVED DEBUG
    //          fprintf(stderr, "[DEBUG main] ast_list is not PAIR or NIL.\n"); // REMOVED DEBUG
    //     } // REMOVED DEBUG
    // } else { // REMOVED DEBUG
    //     fprintf(stderr, "[DEBUG main] ast_list is NULL.\n"); // REMOVED DEBUG
    // } // REMOVED DEBUG
    // fflush(stderr);
    // <<< END DEBUG PRINT >>>
    char* generated_c_code = l0_codegen_program(arena, expanded_ast_list); // Use expanded AST

    // --- DEBUG ---
    // fprintf(stderr, "[DEBUG main] After l0_codegen_program. generated_c_code pointer: %p\n", (void*)generated_c_code);
    // --- END DEBUG ---

    if (!generated_c_code) {
        // Codegen function should ideally set a more specific error
        fprintf(stderr, "Error: Code generation failed.\n");
        l0_arena_destroy(arena);
        return 1;
    }
    printf("Code generation successful.\n");
    // Optional: Print generated code for debugging
    // printf("--- Generated C Code ---\n%s\n------------------------\n", generated_c_code);

    // --- Write Output File ---
    printf("Writing output file: %s\n", output_filename);
    if (!write_file_content(output_filename, generated_c_code)) {
        l0_arena_destroy(arena);
        return 1;
    }
    printf("Output written successfully.\n");

    // --- Cleanup ---
    l0_arena_destroy(arena);
    printf("Compilation finished.\n");

    // --- DEBUG ---
    // fprintf(stderr, "[DEBUG main] Reached end of main, returning 0.\n"); // REMOVED DEBUG
    // --- END DEBUG ---

    return 0;
}

// Need to link l0_parser.c, l0_codegen.c, l0_types.c, l0_arena.c, l0_env.c, l0_primitives.c
// Implementation for error reset function
static void repl_reset_error_state() {
    // fprintf(stderr, "[DEBUG main] Inside repl_reset_error_state.\n"); fflush(stderr); // REMOVED DEBUG
    l0_parser_error_status = L0_PARSE_OK;
    l0_parser_error_message = NULL; // Reset message pointer
    l0_parser_error_line = 0;
    l0_parser_error_col = 0;
}
