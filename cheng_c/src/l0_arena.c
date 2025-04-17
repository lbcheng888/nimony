#include "l0_arena.h"
#include <stddef.h> // Explicitly include for max_align_t, even if redundant
#include <stdint.h> // For uintptr_t
#include <stdlib.h> // For malloc, free
#include <string.h> // For strlen, memcpy
#include <assert.h> // For assert
#include <stdio.h>  // For fprintf, stderr in case of errors

// Helper function to align a pointer upwards
// ptr: the pointer value to align
// alignment: the desired alignment (must be a power of 2)
static inline uintptr_t align_up(uintptr_t ptr, size_t alignment) {
    // Ensure alignment is a power of 2. If not, behavior is undefined.
    // A simple check: (alignment && !(alignment & (alignment - 1)))
    // We'll rely on the caller providing a valid alignment for performance.
    // assert((alignment != 0) && ((alignment & (alignment - 1)) == 0));
    return (ptr + alignment - 1) & ~(alignment - 1);
}

// Helper function to allocate a new block for the arena
// min_data_size: the minimum required data size for the new block
static ArenaBlock* allocate_block(size_t min_data_size) {
    // Determine the base data size for the block
    size_t base_data_size = (min_data_size > L0_ARENA_DEFAULT_BLOCK_SIZE) ? min_data_size : L0_ARENA_DEFAULT_BLOCK_SIZE;
    // We still need enough space for the header AND the data.
    // Let's estimate the maximum header size including potential alignment padding for data_start.
    size_t header_estimate = sizeof(ArenaBlock) + alignof(max_align_t) - 1;
    // Allocate enough for the header estimate plus the required data size.
    size_t total_allocation_size = header_estimate + base_data_size;

    // Allocate the memory first
    void* raw_memory = malloc(total_allocation_size);
    if (!raw_memory) {
        fprintf(stderr, "Error: Failed to allocate memory for ArenaBlock (raw size %zu)\n", total_allocation_size);
        return NULL;
    }
    ArenaBlock* block = (ArenaBlock*)raw_memory; // Cast the beginning

    // Now, calculate the actual data_start based on the allocated block pointer
    block->data_start = align_up((uintptr_t)(block + 1), alignof(max_align_t));

    // Calculate the actual usable data size based on the allocated total size and the data_start offset
    block->size = ((uintptr_t)raw_memory + total_allocation_size) - block->data_start;
    // Ensure the usable size is at least what was requested (it might be slightly larger due to alignment/malloc overhead)
    // assert(block->size >= base_data_size); // This might not hold if header padding was large

    // Initialize the rest of the block header
    block->next = NULL;
    block->used = 0;

    // Refined Sanity check: ensure data area start is after header and end is within bounds
    assert(block->data_start >= (uintptr_t)(block + 1));
    assert(block->data_start + block->size <= (uintptr_t)raw_memory + total_allocation_size);
    // Check if we *at least* got the minimum requested data size after alignment padding
    if (block->size < min_data_size) {
         fprintf(stderr, "Warning: Arena block usable size (%zu) is less than requested minimum (%zu) after alignment.\n", block->size, min_data_size);
         // This might indicate a problem if a single allocation needs more than block->size
    }


    return block;
}

L0_Arena* l0_arena_create_with_size(size_t initial_block_size) {
    L0_Arena* arena = (L0_Arena*)malloc(sizeof(L0_Arena));
    if (!arena) {
        fprintf(stderr, "Error: Failed to allocate memory for L0_Arena struct\n");
        return NULL;
    }

    size_t first_block_size = (initial_block_size == 0) ? L0_ARENA_DEFAULT_BLOCK_SIZE : initial_block_size;
    arena->first_block = allocate_block(first_block_size);
    if (!arena->first_block) {
        free(arena);
        return NULL;
    }
    arena->current_block = arena->first_block;

    return arena;
}

L0_Arena* l0_arena_create() {
    return l0_arena_create_with_size(0); // Use default size
}

void l0_arena_destroy(L0_Arena* arena) {
    if (!arena) {
        return;
    }

    ArenaBlock* current = arena->first_block;
    while (current) {
        ArenaBlock* next = current->next;
        free(current); // Free the entire block allocated by malloc
        current = next;
    }
    free(arena); // Free the arena structure itself
}

void* l0_arena_alloc(L0_Arena* arena, size_t size, size_t alignment) {
    if (!arena || !arena->current_block) {
         fprintf(stderr, "Error: Invalid or uninitialized arena passed to l0_arena_alloc\n");
        return NULL; // Invalid arena
    }

    // Ensure alignment is a power of 2, default if necessary
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
         alignment = alignof(max_align_t);
    }

    ArenaBlock* block = arena->current_block;

    // Calculate the next available aligned pointer within the current block's data area
    uintptr_t current_ptr = block->data_start + block->used;
    uintptr_t aligned_ptr = align_up(current_ptr, alignment);

    // Handle zero-size allocation: return the next aligned pointer, but don't advance 'used' significantly.
    // We might need to advance 'used' just by the padding amount if alignment matters.
    if (size == 0) {
        // Calculate padding needed for alignment
        size_t padding = aligned_ptr - current_ptr;
        // Check if even the padding fits
        if (block->used + padding <= block->size) {
             // Optionally, advance 'used' by padding to maintain alignment for subsequent allocations.
             // block->used += padding;
             return (void*)aligned_ptr; // Return the unique non-NULL pointer
        }
        // If even padding doesn't fit, fall through to new block allocation,
        // which will return a valid pointer from the new block.
    }


    // Calculate padding added by alignment (re-calculate or use previous)
    size_t padding = aligned_ptr - current_ptr;
    size_t total_needed = padding + size;

    // Check if the current block has enough space
    if (block->used + total_needed <= block->size) {
        block->used += total_needed; // Bump the used counter
        return (void*)aligned_ptr;   // Return the aligned pointer
    }

    // Not enough space in the current block, need to allocate a new one.
    // Determine the minimum size needed for the new block's data area.
    // It should be at least the default size, or large enough for the requested allocation.
    size_t new_block_min_data_size = (size > L0_ARENA_DEFAULT_BLOCK_SIZE) ? size : L0_ARENA_DEFAULT_BLOCK_SIZE;

    ArenaBlock* new_block = allocate_block(new_block_min_data_size);
    if (!new_block) {
        fprintf(stderr, "Error: Failed to allocate new block in l0_arena_alloc\n");
        return NULL; // Failed to allocate new block
    }

    // Link the new block into the arena's chain (prepend)
    new_block->next = arena->first_block;
    arena->first_block = new_block;
    arena->current_block = new_block; // Switch allocation to the new block

    // Allocate from the beginning of the new block's data area
    current_ptr = new_block->data_start;
    aligned_ptr = align_up(current_ptr, alignment); // Align start of data

    // Handle zero-size allocation in the new block
    if (size == 0) {
        // Calculate padding needed for alignment in the new block
        padding = aligned_ptr - current_ptr;
        // Check if padding fits (should always fit in a new block unless alignment is huge)
        if (new_block->used + padding <= new_block->size) {
            // Optionally advance used by padding
            // new_block->used += padding;
            return (void*)aligned_ptr;
        } else {
             // This case is highly unlikely (alignment > block size) but return NULL if it happens.
             fprintf(stderr, "Error: Cannot satisfy alignment for zero-size allocation even in new block.\n");
             return NULL;
        }
    }

    padding = aligned_ptr - current_ptr;
    total_needed = padding + size;

    // The new block was allocated to be large enough, this assert should pass
    // Check if the allocation fits in the new block's usable size
    if (total_needed > new_block->size) {
         fprintf(stderr, "Error: Cannot fit allocation (size %zu, align %zu, padding %zu, total %zu) in new block (size %zu)\n",
                 size, alignment, padding, total_needed, new_block->size);
         // Ideally, handle error more gracefully than just returning NULL,
         // as the block is already linked. But for now, NULL indicates failure.
         return NULL;
    }

    // Mark space as used in the new block
    new_block->used = total_needed; // Set 'used' to the end offset of this allocation

    return (void*)aligned_ptr;       // Return the allocated pointer
}

void l0_arena_reset(L0_Arena* arena) {
    if (!arena) {
        return;
    }
    // Reset 'used' counter in all blocks back to 0
    ArenaBlock* current = arena->first_block;
    while (current) {
        current->used = 0;
        current = current->next;
    }
    // Reset the current block pointer back to the first block
    arena->current_block = arena->first_block;
}

char* l0_arena_strdup(L0_Arena* arena, const char* str) {
    if (!str) {
        return NULL;
    }
    size_t len = strlen(str);
    // Allocate space for the string plus the null terminator
    // Use alignof(char) which is typically 1
    char* new_str = (char*)l0_arena_alloc(arena, len + 1, alignof(char));
    if (!new_str) {
        fprintf(stderr, "Error: Failed to allocate memory in arena for string duplication\n");
        return NULL;
    }
    memcpy(new_str, str, len + 1); // Copy including the null terminator
    return new_str;
}
