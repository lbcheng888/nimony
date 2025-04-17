#ifndef L0_PARSER_H
#define L0_PARSER_H

#include "l0_arena.h" // Include Arena definition FIRST
#include "l0_types.h" // Then include types which might use forward declarations
#include <stdio.h>    // For FILE* stream input

// --- Error Handling (Placeholder) ---
// 需要一个更健壮的错误报告机制。
typedef enum {
    L0_PARSE_OK,
    L0_PARSE_ERROR_UNEXPECTED_EOF,
    L0_PARSE_ERROR_INVALID_SYNTAX,
    L0_PARSE_ERROR_MEMORY,
    L0_PARSE_ERROR_RUNTIME, // Added for general runtime errors (e.g., in primitives)
    // 根据需要添加更具体的错误
} L0_ParseStatus;

// 用于报告错误的全局变量（简化方式，后续可改进）
extern L0_ParseStatus l0_parser_error_status;
extern const char* l0_parser_error_message;
extern unsigned long l0_parser_error_line; // 行号
extern unsigned long l0_parser_error_col;  // 列号

// --- Main Parsing Functions ---

/**
 * @brief 从以 null 结尾的字符串解析单个 S 表达式。
 *
 * 函数尝试从字符串开头解析一个完整的 S 表达式。
 * 它为结果 AST 节点分配内存。
 * 调用者负责使用适当的内存管理函数（例如 l0_free_value）释放返回的 L0_Value*。
 *
 * @param arena 用于分配 AST 节点的内存区域。
 * @param input 包含 S 表达式的以 null 结尾的字符串。
 * @param end_ptr 如果非 NULL，它将被更新为指向输入字符串中紧随已解析 S 表达式之后的字符。
 *                这允许从单个字符串解析多个表达式。
 * @return 指向已解析 L0_Value AST 的指针，错误时返回 NULL。
 *         检查 l0_parser_error_status 等变量以获取错误详情。
 */
L0_Value* l0_parse_string(L0_Arena* arena, const char* input, const char** end_ptr);

/**
 * @brief 从 FILE 流解析单个 S 表达式。
 *
 * 从流中读取，直到解析完一个完整的 S 表达式或发生错误。
 * 为结果 AST 节点在提供的 arena 中分配内存。
 *
 * @param arena 用于分配 AST 节点的内存区域。
 * @param stream 输入 FILE 流。
 * @param filename (可选) 用于错误报告的文件名。
 * @return 指向已解析 L0_Value AST 的指针，如果出错或在找到完整表达式之前遇到 EOF，则返回 NULL。
 *         检查 l0_parser_error_status 等变量以获取错误详情。
 */
L0_Value* l0_parse_stream(L0_Arena* arena, FILE* stream, const char* filename);


// --- Utility (可能对测试或特定用例有用) ---

/**
 * @brief 将以 null 结尾的字符串中的所有 S 表达式解析到一个列表中。
 *
 * 解析整个输入字符串，期望零个或多个 S 表达式。
 * 返回一个类型为 L0_TYPE_PAIR 的 L0_Value，表示包含所有已解析顶层表达式的列表，所有节点都在 arena 中分配。
 * 如果输入为空或仅包含空白/注释，则返回 L0_TYPE_NIL (也在 arena 中分配)。
 *
 * @param arena 用于分配 AST 节点的内存区域。
 * @param input 包含零个或多个 S 表达式的以 null 结尾的字符串。
 * @param filename (可选) 用于错误报告的文件名。
 * @return 指向包含已解析 AST 的 L0_Value 列表的指针，错误时返回 NULL。
 *         检查 l0_parser_error_status 等变量以获取错误详情。
 */
L0_Value* l0_parse_string_all(L0_Arena* arena, const char* input, const char* filename);


#endif // L0_PARSER_H
