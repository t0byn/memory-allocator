#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define DEFAULT_ALIGNMENT 8

#define POW_OF_2(x) (1 << (x))

bool is_power_of_two(uintptr_t x);

uintptr_t align_forward(uintptr_t address, size_t align);

size_t get_padding_with_header(uintptr_t address, size_t header_size, size_t align);

////////////////////////////////
// arena/linear allocator
struct ArenaAllocator
{
    unsigned char* buffer;
    size_t buffer_size;
    size_t offset;
};

void arena_init(ArenaAllocator* arena, void* buffer, size_t buffer_size);
void* arena_alloc(ArenaAllocator* arena, size_t size, size_t align = DEFAULT_ALIGNMENT);
void* arena_resize(ArenaAllocator* arena, void* old_memory, size_t old_size, 
    size_t new_size, size_t align = DEFAULT_ALIGNMENT);
void arena_free(ArenaAllocator* arena, void* ptr);
void arena_free_all(ArenaAllocator* arena);

struct TempArenaAllocator
{
    ArenaAllocator* arena;
    size_t offset;
};

TempArenaAllocator temp_arena_start(ArenaAllocator* arena);
void temp_arena_end(TempArenaAllocator* temp_arena);

////////////////////////////////
// stack allocator (FILO)
struct StackAllocator
{
    unsigned char* buffer;
    size_t buffer_size;
    size_t offset;
    size_t prev_offset;
};

struct StackAllocationHeader
{
    size_t prev_offset;
    uint8_t padding;
};

void stack_init(StackAllocator* stack, void* buffer, size_t buffer_size);
void* stack_alloc(StackAllocator* stack, size_t size, size_t align = DEFAULT_ALIGNMENT);
void* stack_resize(StackAllocator* stack, void* old_ptr, size_t old_size, 
    size_t new_size, size_t align = DEFAULT_ALIGNMENT);
void stack_free(StackAllocator* stack, void* ptr);
void stack_free_all(StackAllocator* stack);

////////////////////////////////
// pool allocator
struct PoolListNode
{
    PoolListNode* next;
};

struct PoolAllocator
{
    unsigned char* buffer;
    size_t buffer_size;
    size_t chunk_size;
    PoolListNode* head;
};

void pool_init(PoolAllocator* pool, void* buffer, size_t buffer_size, 
    size_t chunk_size, size_t align = DEFAULT_ALIGNMENT);
void* pool_alloc(PoolAllocator* pool);
void pool_free(PoolAllocator* pool, void* ptr);
void pool_free_all(PoolAllocator* pool);

////////////////////////////////
// free list based allocator (linked list implementation)
enum FreeListAllocationPolicy
{
    Allocation_Policy_First_Fit,
    Allocation_Policy_Best_Fit,
};

struct FreeListAllocationHeader
{
    size_t padding;
    size_t block_size;
};

struct FreeListNode
{
    FreeListNode* next;
    size_t block_size;
};

struct FreeListAllocator
{
    unsigned char* buffer;
    size_t buffer_size;
    size_t buffer_used;
    FreeListNode* head;
    FreeListAllocationPolicy allocation_policy;
};

void free_list_init(FreeListAllocator* free_list, void* buffer, size_t buffer_size, FreeListAllocationPolicy allocation_policy);
void* free_list_alloc(FreeListAllocator* free_list, size_t size, size_t align = DEFAULT_ALIGNMENT);
void free_list_free(FreeListAllocator* free_list, void* ptr);
void free_list_insert_node(FreeListAllocator* free_list, FreeListNode* prev_node, FreeListNode* node);
void free_list_remove_node(FreeListAllocator* free_list, FreeListNode* prev_node, FreeListNode* node);
void free_list_coalescence_node(FreeListNode* prev_node, FreeListNode* node);
void free_list_free_all(FreeListAllocator* free_list);

////////////////////////////////
// buddy allocator
struct BuddyAllocator
{
    unsigned char* tree;
    unsigned char* buffer;
    size_t tree_height;
    size_t alignment;
};

void buddy_init(BuddyAllocator* allocator, void* buffer, size_t size, size_t align=DEFAULT_ALIGNMENT);
void* buddy_alloc(BuddyAllocator* allocator, size_t size);
void buddy_free(BuddyAllocator* allocator, void* ptr);
void buddy_coalescence(BuddyAllocator* allocator);
void buddy_free_all(BuddyAllocator* allocator);
void buddy_destory(BuddyAllocator* allocator);
void buddy_debug_print(BuddyAllocator* allocator);

#endif
