#include "allocator.h"

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <string.h>
#include <limits.h>

bool is_power_of_two(uintptr_t x)
{
    // check if x is only have one set bit, 
    // if it is, then it must be power of two
    return (x & (x - 1)) == 0;
}

uintptr_t align_forward(uintptr_t address, size_t align)
{
    assert(is_power_of_two(align));
    // same as (address % align) when alignment is power of two
    uintptr_t mod = address & (align - 1);
    if (mod != 0)
    {
        address += (align - mod);
    }
    return address;
}

size_t get_padding_with_header(uintptr_t address, size_t header_size, size_t align)
{
    assert(is_power_of_two(align));

    size_t padding = 0;
    size_t mod = address & (align - 1);
    if (mod != 0) {
        padding += (align - mod);
    }

    if (padding < header_size) {
        size_t remain = header_size - padding;
        if ((remain & (align - 1)) == 0) {
            padding += align * (remain / align);
        }
        else {
            padding += align * ((remain / align) + 1);
        }
    }

    return padding;
}

// arena allocator
void arena_init(ArenaAllocator* arena, void* buffer, size_t buffer_size)
{
    arena->buffer = (unsigned char*)buffer;
    arena->buffer_size = buffer_size;
    arena->offset = 0;
}

void* arena_alloc(ArenaAllocator* arena, size_t size, size_t align)
{
    assert(is_power_of_two(align));

    uintptr_t next_address = 
        align_forward((uintptr_t)arena->buffer + arena->offset, align);
    size_t offset = next_address - (uintptr_t)arena->buffer;

    if (offset + size <= arena->buffer_size) {
        arena->offset = offset + size;
        void* ptr = (void*)&arena->buffer[offset];
        memset(ptr, 0, size);
        return ptr;
    }

    fprintf(stderr, "[ERROR] arena doesn't have enough space for new allocation. " \
        "Require size: %llu, arena available size: %llu\n", size, arena->buffer_size - arena->offset);
    return NULL;
}

void* arena_resize(ArenaAllocator* arena, void* old_ptr, size_t old_size, 
    size_t new_size, size_t align)
{
    assert(is_power_of_two(align));

    if (old_ptr == NULL || old_size == 0)
    {
        return arena_alloc(arena, new_size, align);
    }

    if (arena->buffer <= old_ptr && old_ptr < arena->buffer + arena->buffer_size)
    {
        size_t old_offset = (uintptr_t)old_ptr - (uintptr_t)arena->buffer;
        if (old_offset + old_size == arena->offset)
        {
            if (old_offset + new_size > arena->buffer_size)
            {
                fprintf(stderr, "[ERROR] arena doesn't have enough space for new allocation. " \
                    "Require size: %llu, arena available size: %llu\n", new_size, arena->buffer_size - old_offset);
                return NULL;
            }

            arena->offset = old_offset + new_size;
            if (new_size > old_size)
            {
                memset((void*)&arena->buffer[arena->offset - (new_size - old_size)], 0, new_size - old_size);
            }
            return old_ptr;
        }
        else
        {
            void* new_ptr = arena_alloc(arena, new_size, align);
            memset(new_ptr, 0, new_size);
            size_t min_size = old_size < new_size ? old_size : new_size;
            memcpy(new_ptr, old_ptr, min_size);
            return new_ptr;
        }
    }
    else
    {
        //assert(0);
        fprintf(stderr, "[ERROR] arena_resize failed. old_ptr(%#llX) not in arena scope[%#llX, %#llX).\n",
            (uintptr_t)old_ptr, (uintptr_t)arena->buffer, (uintptr_t)arena->buffer + arena->buffer_size);
        return NULL;
    }
}

void arena_free(ArenaAllocator* arena, void* ptr)
{
    // DO NOTHING
}

void arena_free_all(ArenaAllocator* arena)
{
    arena->offset = 0;
}

// Temporary arena allocator
TempArenaAllocator temp_arena_start(ArenaAllocator* arena)
{
    TempArenaAllocator temp_arena = { 0 };
    temp_arena.arena = arena;
    temp_arena.offset = arena->offset;
    return temp_arena;    
}

void temp_arena_end(TempArenaAllocator* temp_arena)
{
    temp_arena->arena->offset = temp_arena->offset;
}

// stack allocator
void stack_init(StackAllocator* stack, void* buffer, size_t buffer_size)
{
    stack->buffer = (unsigned char*)buffer;
    stack->buffer_size = buffer_size;
    stack->offset = 0;
    stack->prev_offset = 0;
}

void* stack_alloc(StackAllocator* stack, size_t size, size_t align)
{
    uintptr_t start_address = (uintptr_t)stack->buffer + stack->offset;
    
    size_t max_align = 1 << (8 * sizeof(StackAllocationHeader::padding) - 1);
    if (align > max_align)
    {
        align = max_align;
    }

    size_t padding = get_padding_with_header(start_address, sizeof(StackAllocationHeader), align);
    
    if (stack->offset + padding + size > stack->buffer_size)
    {
        fprintf(stderr, "[ERROR] stack doesn't have enough space for new allocation. " \
            "Require size: %lld, require padding: %lld, stack available size: %lld\n", 
            size, padding, stack->buffer_size - stack->offset);
        return NULL;
    }

    unsigned char* ptr = &stack->buffer[stack->offset + padding];

    StackAllocationHeader* header = (StackAllocationHeader*)(ptr - sizeof(StackAllocationHeader));
    header->padding = padding;
    header->prev_offset = stack->prev_offset;

    stack->prev_offset = stack->offset;
    stack->offset += (padding + size);

    return memset((void*)ptr, 0, size);
}

void* stack_resize(StackAllocator* stack, void* old_ptr, size_t old_size, size_t new_size, size_t align)
{
    size_t min_size = old_size < new_size ? old_size : new_size;

    if (old_ptr == NULL)
    {
        return stack_alloc(stack, new_size, align);
    }

    if (new_size == 0)
    {
        stack_free(stack, old_ptr);
        return NULL;
    }

    if (old_ptr < stack->buffer || old_ptr >= stack->buffer + stack->buffer_size)
    {
        //assert(0);
        fprintf(stderr, "[ERROR] stack_resize failed. old_ptr not in stack scope.\n");
        return NULL;
    }

    // Treat as double free;
    if (old_ptr > stack->buffer + stack->offset)
    {
        return NULL;
    }

    StackAllocationHeader* header = (StackAllocationHeader*)((uintptr_t)old_ptr - sizeof(StackAllocationHeader));
    if ((uintptr_t)old_ptr + old_size != (uintptr_t)stack->buffer + stack->offset)
    {
        void* new_ptr = stack_alloc(stack, new_size, align);
        memcpy(new_ptr, old_ptr, min_size);
        return new_ptr;
    }

    stack->offset = stack->offset - old_size + new_size;
    if (new_size > old_size)
    {
        memset((void*)&stack->buffer[stack->offset - (new_size - old_size)], 0, new_size - old_size);
    }
    return old_ptr;
}

void stack_free(StackAllocator* stack, void* ptr)
{
    if (ptr < stack->buffer || ptr >= stack->buffer + stack->buffer_size)
    {
        //assert(0);
        fprintf(stderr, "[ERROR] stack_free failed. ptr not in stack scope.\n");
        return;
    }

    // Allow double free
    if (ptr > stack->buffer + stack->offset)
    {
        return;
    }

    StackAllocationHeader* header = (StackAllocationHeader*)((uintptr_t)ptr - sizeof(StackAllocationHeader));
    size_t prev_offset = (uintptr_t)ptr - (uintptr_t)stack->buffer - header->padding;
    if (prev_offset != stack->prev_offset)
    {
        //assert(0);
        fprintf(stderr, "[ERROR] stack_free failed. out of order free.\n");
        return;
    }

    stack->offset = stack->prev_offset;
    stack->prev_offset = header->prev_offset;
}

void stack_free_all(StackAllocator* stack)
{
    stack->offset = 0;
    stack->prev_offset = 0;
}

void pool_init(PoolAllocator* pool, void* buffer, size_t buffer_size, size_t chunk_size, size_t align) 
{
    uintptr_t start_addr = (uintptr_t)buffer;
    uintptr_t start_addr_align = align_forward(start_addr, align);
    size_t buffer_size_align = buffer_size - (start_addr_align - start_addr);
    size_t chunk_size_align = align_forward(chunk_size, align);

    assert(chunk_size_align >= sizeof(PoolListNode));

    pool->buffer = (unsigned char*)start_addr_align;
    pool->buffer_size = buffer_size_align;
    pool->chunk_size = chunk_size_align;
    pool->head = NULL;

    pool_free_all(pool);
}

void* pool_alloc(PoolAllocator* pool)
{
    PoolListNode* node = pool->head;
    if (node == NULL)
    {
        fprintf(stderr, "[ERROR] pool doesn't have enough space for new allocation.\n");
        return NULL;
    }

    pool->head = node->next;

    void* ptr = node;
    return memset(ptr, 0, pool->chunk_size);
}

void pool_free(PoolAllocator* pool, void* ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    if (ptr < pool->buffer || ptr >= pool->buffer + pool->buffer_size)
    {
        //assert(0);
        fprintf(stderr, "[ERROR] pool_free failed. ptr not in pool buffer scope.\n");
        return;
    }

    PoolListNode* node = (PoolListNode*)ptr;
    node->next = pool->head;
    pool->head = node;
}

void pool_free_all(PoolAllocator* pool)
{
    pool->head = NULL;
    size_t chunk_count = pool->buffer_size / pool->chunk_size;
    for (int i = 0; i < chunk_count; i++)
    {
        void* ptr = (void*)&pool->buffer[i * pool->chunk_size];
        PoolListNode* node = (PoolListNode*)ptr;
        node->next = pool->head;
        pool->head = node;
    }
}

// free list allocator
void free_list_init(FreeListAllocator* free_list, void* buffer, size_t buffer_size, FreeListAllocationPolicy allocation_policy)
{
    assert(buffer_size >= sizeof(FreeListNode));
    if (buffer_size < sizeof(FreeListNode))
    {
        fprintf(stderr, "[ERROR] free_list_init failed. Buffer size=%lld is smaller then sizeof(FreeListNode)=%lld.\n",
            buffer_size, sizeof(FreeListNode));
        return;
    }
    free_list->buffer = (unsigned char*)buffer;
    free_list->buffer_size = buffer_size;
    free_list->allocation_policy = allocation_policy;
    free_list_free_all(free_list);
}

void* free_list_alloc(FreeListAllocator* free_list, size_t size, size_t align)
{
    if ((free_list->buffer_size - free_list->buffer_used) < size
        || free_list->head == NULL)
    {
        fprintf(stderr, "[ERROR] free_list_alloc failed. Allocator doesn't have enough memory for the allocation.\n");
        return NULL;
    }

    if (size < sizeof(FreeListNode)) {
        size = sizeof(FreeListNode);
    }

    FreeListNode* prev_node = NULL;
    FreeListNode* found_node = NULL;
    size_t require_size = 0;
    size_t padding = 0;

    switch (free_list->allocation_policy)
    {
    case Allocation_Policy_First_Fit:
    {
        FreeListNode* node = free_list->head;
        while (node != NULL)
        {
            size_t padd = get_padding_with_header((uintptr_t)node, sizeof(FreeListAllocationHeader), align);
            size_t req_size = padd + size;
            if (node->block_size >= req_size)
            {
                require_size = req_size;
                padding = padd;
                found_node = node;
                break;
            }
            prev_node = node;
            node = node->next;
        }
        break;
    }
    case Allocation_Policy_Best_Fit:
    {
        FreeListNode* node = free_list->head;
        size_t minimum_diff_size = ~(size_t)0;
        while (node != NULL)
        {
            size_t padd = get_padding_with_header((uintptr_t)node, sizeof(FreeListAllocationHeader), align);
            size_t req_size = padd + size;
            if (node->block_size >= req_size && (node->block_size - req_size) < minimum_diff_size)
            {
                require_size = req_size;
                padding = padd;
                minimum_diff_size = node->block_size - req_size;
                found_node = node;
            }
            prev_node = node;
            node = node->next;
        }
        break;
    }
    default:
    {
        assert(0 && "Not implement allocation policy!");
        return NULL;
    }
    }

    if (found_node == NULL) {
        fprintf(stderr, "[ERROR] free_list_alloc failed. Allocator doesn't have suitable block for size=%lld.\n", size);
        return NULL;
    }

    if (found_node->block_size - require_size > sizeof(FreeListNode))
    {
        FreeListNode* new_node = (FreeListNode*)((unsigned char*)found_node + require_size);
        new_node->block_size = found_node->block_size - require_size;
        found_node->block_size = require_size;
        free_list_insert_node(free_list, found_node, new_node);
    }

    free_list_remove_node(free_list, prev_node, found_node);

    free_list->buffer_used += found_node->block_size;

    unsigned char* ptr = (unsigned char*)found_node + padding;
    FreeListAllocationHeader* header = 
        (FreeListAllocationHeader*)(ptr - sizeof(FreeListAllocationHeader));
    header->block_size = found_node->block_size;
    header->padding = padding;
    return memset(ptr, 0, size);
}

void free_list_free(FreeListAllocator* free_list, void* ptr)
{
    FreeListAllocationHeader* header = 
        (FreeListAllocationHeader*)((uintptr_t)ptr - sizeof(FreeListAllocationHeader));

    FreeListNode* new_node = (FreeListNode*)((uintptr_t)ptr - header->padding);
    size_t block_size = header->block_size;
    new_node->block_size = block_size;

    FreeListNode* node = free_list->head;
    FreeListNode* prev_node = NULL;
    while (node != NULL)
    {
        if (node > new_node)
        {
            break;
        }
        prev_node = node;
        node = node->next;
    }

    free_list_insert_node(free_list, prev_node, new_node);
    free_list->buffer_used -= new_node->block_size;
    free_list_coalescence_node(prev_node, new_node);
}

void free_list_insert_node(FreeListAllocator* free_list, FreeListNode* prev_node, FreeListNode* node)
{
    if (prev_node == NULL)
    {
        node->next = free_list->head;
        free_list->head = node;
    }
    else
    {
        node->next = prev_node->next;
        prev_node->next = node;
    }
}

void free_list_remove_node(FreeListAllocator* free_list, FreeListNode* prev_node, FreeListNode* node)
{
    if (prev_node == NULL)
    {
        free_list->head = node->next;
    }
    else
    {
        prev_node->next = node->next;
    }
}

void free_list_coalescence_node(FreeListNode* prev_node, FreeListNode* node)
{
    if (node != NULL && node->next != NULL && (uintptr_t)node + node->block_size == (uintptr_t)node->next)
    {
        node->block_size += node->next->block_size;
        node->next = node->next->next;
    }

    if (prev_node != NULL && node != NULL && (uintptr_t)prev_node + prev_node->block_size == (uintptr_t)node)
    {
        prev_node->block_size += node->block_size;
        prev_node->next = node->next;
    }
}

void free_list_free_all(FreeListAllocator* free_list)
{
    free_list->buffer_used = 0;
    FreeListNode* node = (FreeListNode*)free_list->buffer;
    node->block_size = free_list->buffer_size;
    node->next = NULL;
    free_list->head = node;
}

// buddy
// 0b00 Free  0b01 Split  0b10 Alloc
#define BUDDY_BIT 2
#define BUDDY_SLOT(i) ((i) / 4)
#define BUDDY_MASK(i) ((1 << (((i) * BUDDY_BIT) % 8)) | (1 << (((i) * BUDDY_BIT) % 8 + 1)))
#define BUDDY_INDEX(arr, i) ((arr)[BUDDY_SLOT(i)] & BUDDY_MASK(i))
#define BUDDY_SET_FREE(arr, i) ((arr)[BUDDY_SLOT(i)] &= ~BUDDY_MASK(i))
#define BUDDY_SET_SPLIT(arr, i) ((arr)[BUDDY_SLOT(i)] |= (1 << (((i) * BUDDY_BIT) % 8)))
#define BUDDY_SET_ALLOC(arr, i) ((arr)[BUDDY_SLOT(i)] |= (1 << (((i) * BUDDY_BIT) % 8 + 1)))
#define BUDDY_IS_FREE(arr, i) ((BUDDY_INDEX(arr, i) & BUDDY_MASK(i)) == 0)
#define BUDDY_IS_SPLIT(arr, i) ((BUDDY_INDEX(arr, i) & (1 << (((i) * BUDDY_BIT) % 8))) != 0)
#define BUDDY_IS_ALLOC(arr, i) ((BUDDY_INDEX(arr, i) & (1 << (((i) * BUDDY_BIT) % 8 + 1))) != 0)

void buddy_init(BuddyAllocator* allocator, void* buffer, size_t size, size_t align)
{
    assert(buffer != NULL);
    assert(is_power_of_two(size));
    assert(is_power_of_two(align));
    assert((uintptr_t)buffer % align == 0);
    assert(size % align == 0);

    size_t leaf_count = size / align;
    assert(leaf_count > 1);

    // The height of a perfect binary tree with one node is 0
    size_t tree_height = 0;
    for (size_t i = leaf_count; i > 1; i >>= 1) {
        tree_height++;
    }
    assert(tree_height > 0);

    size_t node_count = 2 * leaf_count - 1;
    size_t tree_size = (node_count * BUDDY_BIT) / CHAR_BIT + 1;

    allocator->tree = (unsigned char*)malloc(tree_size);
    assert(allocator->tree != NULL); // TODO
    memset(allocator->tree, 0, tree_size);
    allocator->buffer = (unsigned char*)buffer;
    allocator->tree_height = tree_height;
    allocator->alignment = align;
}

void* buddy_alloc(BuddyAllocator* allocator, size_t size)
{
    size_t require_size = align_forward(size, allocator->alignment);

    size_t queue[1024];
    int head = 0, tail = 0;

    const size_t buffer_size = POW_OF_2(allocator->tree_height) * allocator->alignment;

    bool found = false;
    size_t buddy_index = 0;
    size_t buddy_size = ~0;
    size_t buddy_height = 0;
    // enqueue
    queue[tail] = 0; 
    tail = (tail + 1) % 1024;
    assert(head != tail);
    while (head != tail) {
        // dequeue
        size_t index = queue[head];
        head = (head + 1) % 1024;

        size_t block_size = buffer_size;
        size_t height = 0;
        size_t i = index + 1;
        while (i > 1) {
            block_size >>= 1;
            i >>= 1;
            height++;
        }

        if (block_size < require_size) continue;

        if (BUDDY_IS_FREE(allocator->tree, index) && block_size < buddy_size) {
            found = true;
            buddy_index = index;
            buddy_size = block_size;
            buddy_height = height;
        }
        else if (BUDDY_IS_SPLIT(allocator->tree, index)) {
            size_t left = index * 2 + 1;
            size_t right = index * 2 + 2;
            
            // enqueue
            queue[tail] = left; 
            tail = (tail + 1) % 1024;
            assert(head != tail);
            
            // enqueue
            queue[tail] = right; 
            tail = (tail + 1) % 1024;
            assert(head != tail);
        }
    }

    if (found) {
        while (require_size <= (buddy_size >> 1)) {
            BUDDY_SET_SPLIT(allocator->tree, buddy_index);
            buddy_size >>= 1;
            buddy_index = buddy_index * 2 + 1;
            buddy_height++;
        }
        BUDDY_SET_ALLOC(allocator->tree, buddy_index);
        size_t offset = buddy_size * (buddy_index + 1 - POW_OF_2(buddy_height));
        void* ptr = &allocator->buffer[offset];
        return memset(ptr, 0, buddy_size);
    }

    fprintf(stderr, "[ERROR] buddy_allocator_alloc failed. Allocator doesn't have suitable buddy for size=%lld.", size);
    return NULL;
}

void buddy_free(BuddyAllocator* allocator, void* ptr)
{
    size_t offset = (uintptr_t)ptr - (uintptr_t)allocator->buffer;
    size_t index =
        POW_OF_2(allocator->tree_height) - 1 + (offset / allocator->alignment);

    bool free = false;
    while (index != 0) {
        if (BUDDY_IS_ALLOC(allocator->tree, index)) {
            BUDDY_SET_FREE(allocator->tree, index);
            free = true;
            break;
        }
        if (index % 2 == 0) break;
        index = (index - 1) / 2;
    }
    if (!free && offset == 0) {
        if (BUDDY_IS_ALLOC(allocator->tree, 0)) {
            BUDDY_SET_FREE(allocator->tree, 0);
            free = true;
        }
    }
    assert(free);

    buddy_coalescence(allocator);
}

void buddy_coalescence(BuddyAllocator* allocator)
{
    assert(allocator->tree_height > 0);
    size_t height = allocator->tree_height;

    while (height > 0) {
        size_t parent_height = height - 1;
        size_t parent_count = POW_OF_2(parent_height);
        for (size_t i = parent_count - 1; i < POW_OF_2(height) - 1; i++) {
            if (!BUDDY_IS_SPLIT(allocator->tree, i)) {
                continue;
            }
            size_t left = i * 2 + 1;
            size_t right = i * 2 + 2;
            if (BUDDY_IS_FREE(allocator->tree, left)
                && BUDDY_IS_FREE(allocator->tree, right))
            {
                BUDDY_SET_FREE(allocator->tree, i);
            }
        }
        height--;
    }
}

void buddy_free_all(BuddyAllocator* allocator)
{
    size_t tree_size = ((POW_OF_2(allocator->tree_height + 1) - 1) * BUDDY_BIT) / CHAR_BIT + 1;
    memset(allocator->tree, 0, tree_size);
}

void buddy_destory(BuddyAllocator* allocator)
{
    allocator->buffer = NULL;
    allocator->alignment = 0;
    allocator->tree_height = 0;
    free(allocator->tree);
}

void buddy_debug_print(BuddyAllocator* allocator)
{
    size_t height = allocator->tree_height;
    size_t indent = 1;
    fprintf(stdout, "\n");
    while (height != 0) {
        for (size_t i = POW_OF_2(height) - 1; i < POW_OF_2(height + 1) - 1; i++)
        {
            if (BUDDY_IS_FREE(allocator->tree, i)) {
                fprintf(stdout, "0");
                for (size_t j = 0; j < indent; j++) {
                    fprintf(stdout, " ");
                }
            }
            else if (BUDDY_IS_SPLIT(allocator->tree, i)) {
                fprintf(stdout, "1");
                for (size_t j = 0; j < indent; j++) {
                    fprintf(stdout, " ");
                }
            }
            else if (BUDDY_IS_ALLOC(allocator->tree, i)) {
                fprintf(stdout, "2");
                for (size_t j = 0; j < indent; j++) {
                    fprintf(stdout, " ");
                }
            }
        }
        height--;
        indent = indent * 2 + 1;
        fprintf(stdout, "\n");
    }
    if (BUDDY_IS_FREE(allocator->tree, 0)) {
        fprintf(stdout, "0");
    }
    else if (BUDDY_IS_SPLIT(allocator->tree, 0)) {
        fprintf(stdout, "1");
    }
    else if (BUDDY_IS_ALLOC(allocator->tree, 0)) {
        fprintf(stdout, "2");
    }
    fprintf(stdout, "\n");
}
