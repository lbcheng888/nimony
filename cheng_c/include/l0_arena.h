#ifndef L0_ARENA_H
#define L0_ARENA_H

#include <stddef.h> // For size_t, max_align_t
#include <stdint.h> // For uintptr_t
#include <stdalign.h> // For alignas, alignof

// --- Arena Structure ---

// A simple linear allocator (bump allocator)

// Define a reasonable default block size, can be tuned
#define L0_ARENA_DEFAULT_BLOCK_SIZE (4 * 1024 * 1024) // 4MB blocks

typedef struct ArenaBlock {
    struct ArenaBlock* next; // Pointer to the next block in the chain
    size_t size;           // Size of this block's data area (excluding header)
    size_t used;           // Bytes used in this block's data area
    // Use a pointer to the start of the data area to simplify alignment calculations
    uintptr_t data_start;
    // The actual data follows the header in memory
} ArenaBlock;

// Use a named struct tag consistent with forward declaration in l0_types.h
typedef struct L0_Arena {
    ArenaBlock* current_block; // The block currently being allocated from
    // We need to keep track of the first block to free the entire chain
    ArenaBlock* first_block;
} L0_Arena;


// --- Arena Management Functions ---

/**
 * @brief 创建一个新的 Arena，使用默认块大小。
 *
 * @return 指向新创建 Arena 的指针，失败时返回 NULL。
 *         调用者负责稍后调用 l0_arena_destroy 释放 Arena。
 */
L0_Arena* l0_arena_create();

/**
 * @brief 创建一个新的 Arena，指定初始块大小。
 *
 * @param initial_block_size 第一个块的大小（字节）。如果为 0，则使用默认大小。
 * @return 指向新创建 Arena 的指针，失败时返回 NULL。
 */
L0_Arena* l0_arena_create_with_size(size_t initial_block_size);


/**
 * @brief 销毁一个 Arena 及其分配的所有内存块。
 *
 * @param arena 要销毁的 Arena。如果为 NULL，则不执行任何操作。
 */
void l0_arena_destroy(L0_Arena* arena);

/**
 * @brief 从 Arena 分配指定大小和对齐方式的内存。
 *
 * 这是 Arena 的核心分配函数。它尝试从当前块分配，
 * 如果空间不足，则分配一个新块。
 * 分配的内存未初始化。
 *
 * @param arena 要从中分配的 Arena。
 * @param size 要分配的字节数。
 * @param alignment 所需的内存对齐方式（必须是 2 的幂）。
 * @return 指向已分配内存的指针，失败时返回 NULL。
 */
void* l0_arena_alloc(L0_Arena* arena, size_t size, size_t alignment);

/**
 * @brief 重置 Arena，使其可以重新使用，但保留已分配的块。
 *
 * 将所有块的 'used' 标记重置为 0。
 * 这比销毁和重新创建更快，如果 Arena 会被重复用于相似大小的任务。
 *
 * @param arena 要重置的 Arena。
 */
void l0_arena_reset(L0_Arena* arena);

/**
 * @brief 在 Arena 中分配并复制一个字符串。
 *
 * @param arena 要分配的 Arena。
 * @param str 要复制的以 null 结尾的字符串。
 * @return 指向 Arena 中新分配的字符串副本的指针，失败时返回 NULL。
 */
char* l0_arena_strdup(L0_Arena* arena, const char* str);


// --- Convenience Macros/Inline Functions ---

// Allocate memory for a specific type, ensuring correct alignment
#define l0_arena_alloc_type(arena, type) \
    ((type*)l0_arena_alloc((arena), sizeof(type), alignof(type)))

// Allocate memory for an array of a specific type
#define l0_arena_alloc_array(arena, type, count) \
    ((type*)l0_arena_alloc((arena), sizeof(type) * (count), alignof(type)))


#endif // L0_ARENA_H
