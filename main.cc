#include "allocator.h"
#include <malloc.h>
#include <assert.h>

void arena_test()
{
    size_t buf_size = 1024;
    char* buf_1k = (char*)malloc(buf_size);
    
    const size_t align = 8;
    ArenaAllocator arena = { 0 };
    arena_init(&arena, buf_1k, buf_size);
    assert((uintptr_t)arena.buffer == (uintptr_t)buf_1k);
    assert(arena.buffer_size == buf_size);
    
    char* fail_1 = NULL;
    fail_1 = (char*)arena_alloc(&arena, 2 * buf_size, align);
    assert(fail_1 == NULL);
    assert(arena.offset == 0);

    size_t size_1 = 5;
    char* alloc_1 = (char*)arena_alloc(&arena, size_1, align);
    assert(arena.offset == size_1);
    assert((uintptr_t)alloc_1 + 5 == (uintptr_t)arena.buffer + arena.offset);
    for (int i = 0; i < size_1; i++) {
        alloc_1[i] = 65 + i;
    }

    size_t size_2 = align;
    char* alloc_2 = (char*)arena_alloc(&arena, size_2, align);
    assert((uintptr_t)alloc_2 % align == 0);
    assert((uintptr_t)alloc_1 + size_1 +(align - (size_1 % align)) == (uintptr_t)alloc_2);
    assert((uintptr_t)alloc_2 + size_2 == (uintptr_t)arena.buffer + arena.offset);
    for (int i = 0; i < size_2; i++) {
        alloc_2[i] = 65 + i;
    }

    size_t size_3 = 4;
    char* alloc_3 = (char*)arena_alloc(&arena, size_3, align);
    assert((uintptr_t)alloc_3 % align == 0);
    assert((uintptr_t)alloc_3 == (uintptr_t)alloc_2 + align);
    assert((uintptr_t)alloc_3 + size_3 == (uintptr_t)arena.buffer + arena.offset);
    for (int i = 0; i < size_3; i++) {
        alloc_3[i] = 65 + i;
    }

    size_t size_4 = 12;
    char* alloc_4 = (char*)arena_resize(&arena, alloc_3, size_3, size_4, align);
    assert(alloc_4 == alloc_3);
    assert((uintptr_t)alloc_4 + size_4 == (uintptr_t)arena.buffer + arena.offset);
    for (int i = 0; i < size_3; i++) {
        assert(alloc_4[i] == 65 + i);
    }
    for (int i = 4; i < size_4; i++) {
        assert(alloc_4[i] == 0);
    }

    size_t size_5 = size_2 / 2;
    char* alloc_5 = (char*)arena_resize(&arena, alloc_2, size_2, size_2 / 2, align);
    assert(alloc_5 != alloc_2);
    assert((uintptr_t)alloc_5 + size_5 == (uintptr_t)arena.buffer + arena.offset);
    for (int i = 0; i < 4; i++) {
        assert(alloc_5[i] == alloc_2[i]);
    }

    size_t size_6 = 32;
    char* alloc_6 = (char*)arena_resize(&arena, NULL, 1024, size_6, align);
    assert((uintptr_t)alloc_6 + size_6 == (uintptr_t)arena.buffer + arena.offset);
    
    char* fail_2 = (char*)arena_resize(&arena, alloc_6, size_6, 2 * buf_size, align);
    assert(fail_2 == NULL);
    
    char* fail_3 = (char*)arena_resize(&arena, (void*)0x01, 8, 16, align);
    assert(fail_3 == NULL);

    size_t save_offset = arena.offset;
    TempArenaAllocator temp_arena = temp_arena_start(&arena);
    assert(temp_arena.arena == &arena);
    assert(temp_arena.arena->offset == arena.offset);

    size_t temp_size_1 = 7;
    char* temp_alloc_1 = (char*)arena_alloc(&arena, temp_size_1, align);
    assert(temp_alloc_1 != NULL);
    assert(arena.offset == save_offset + temp_size_1);

    size_t temp_size_2 = 5;
    char* temp_alloc_2 = (char*)arena_alloc(&arena, temp_size_2, align);
    assert(temp_alloc_2 != NULL);
    assert(arena.offset == save_offset + temp_size_1 + (align - (temp_size_1 % align)) + temp_size_2);

    temp_arena_end(&temp_arena);
    assert(arena.offset == save_offset);

    arena_free_all(&arena);
    assert(arena.offset == 0);

    free(buf_1k);
}

void stack_test()
{
    size_t buf_size = 1024;
    char* buf_1k = (char*)malloc(buf_size);

    StackAllocator stack = {0};
    stack_init(&stack, buf_1k, buf_size);
    assert((uintptr_t)stack.buffer == (uintptr_t)buf_1k);
    assert(stack.buffer_size == buf_size);
    assert(stack.offset == 0);
    assert(stack.prev_offset == 0);

    size_t align = 8;

    {
        size_t size = 5;
        char* p = (char*)stack_alloc(&stack, size, align);
        for (int i = 0; i < size; i++) {
            p[i] = 65 + i;
        }
        StackAllocationHeader* header = (StackAllocationHeader*)(p - sizeof(StackAllocationHeader));
        assert(p != NULL);
        assert(stack.offset == size + header->padding);
        assert(stack.prev_offset == 0);
        assert(header->prev_offset == 0);
        assert(header->padding == get_padding_with_header((uintptr_t)buf_1k, sizeof(StackAllocationHeader), align));

        stack_free(&stack, p);
        assert(stack.offset == 0);
        assert(stack.prev_offset == 0);
    }

    {
        size_t offset_before_p1 = stack.offset;

        size_t sz1 = 5;
        char* p1 = (char*)stack_alloc(&stack, sz1, align);
        StackAllocationHeader* h1 = (StackAllocationHeader*)(p1 - sizeof(StackAllocationHeader));
        for (int i = 0; i < sz1; i++) {
            p1[i] = 65 + i;
        }
        
        size_t offset_before_p2 = stack.offset;

        size_t sz2 = 8;
        char* p2 = (char*)stack_alloc(&stack, sz2, align);
        StackAllocationHeader* h2 = (StackAllocationHeader*)(p2 - sizeof(StackAllocationHeader));
        for (int i = 0; i < sz2; i++) {
            p2[i] = 65 + i;
        }
        assert((uintptr_t)p2 == (uintptr_t)p1 + sz1 + h2->padding);
        assert((uintptr_t)p2 + sz2 == (uintptr_t)stack.buffer + stack.offset);
        assert(stack.prev_offset == offset_before_p2);
        assert(h2->prev_offset == offset_before_p1);

        size_t offset_before_p3 = stack.offset;
        
        size_t sz3 = 16;
        char* p3 = (char*)stack_alloc(&stack, sz3, align);
        StackAllocationHeader* h3 = (StackAllocationHeader*)(p3 - sizeof(StackAllocationHeader));
        for (int i = 0; i < sz3; i++) {
            p3[i] = 65 + i;
        }
        assert((uintptr_t)p3 == (uintptr_t)p2 + sz2 + h3->padding);
        assert((uintptr_t)p3 + sz3 == (uintptr_t)stack.buffer + stack.offset);
        assert(stack.prev_offset == offset_before_p3);
        assert(h3->prev_offset == offset_before_p2);

        uint8_t h3_padding = h3->padding;
        size_t prev_offset_before_p4 = stack.prev_offset;

        size_t sz4 = 6;
        char* p4 = (char*)stack_resize(&stack, p3, sz3, sz4, align);
        StackAllocationHeader* h4 = (StackAllocationHeader*)(p4 - sizeof(StackAllocationHeader));
        assert(p4 == p3);
        assert(h4 == h3);
        assert(h4->prev_offset == offset_before_p2);
        assert(h4->padding == h3_padding);
        assert(stack.prev_offset == offset_before_p3);
        assert((uintptr_t)p4 + sz4 == stack.offset + (uintptr_t)stack.buffer);
        for (int i = 0; i < sz4; i++) {
            assert(p4[i] == p3[i]);
        }

        size_t offset_before_p5 = stack.offset;

        size_t sz5 = 2 * sz2;
        char* p5 = (char*)stack_resize(&stack, p2, sz2, sz5, align);
        StackAllocationHeader* h5 = (StackAllocationHeader*)(p5 - sizeof(StackAllocationHeader));
        assert(p5 != p2);
        assert((uintptr_t)p5 + sz5 == (uintptr_t)stack.buffer + stack.offset);
        assert(stack.prev_offset == offset_before_p5);
        assert(h5->prev_offset == offset_before_p3);
        assert(prev_offset_before_p4 == offset_before_p3);
        for (int i = 0; i < sz2; i++) {
            assert(p5[i] == p2[i]);
        }
        
        size_t offset_before_free = stack.offset;
        size_t prev_offset_before_free = stack.prev_offset;
        stack_free(&stack, p4);
        assert(stack.offset = offset_before_free);
        assert(stack.prev_offset = prev_offset_before_free);

        stack_free(&stack, p5);
        assert(stack.offset == offset_before_p5);
        assert(stack.prev_offset == prev_offset_before_p4);

        stack_free(&stack, p4);
        assert(stack.offset == prev_offset_before_p4);
        assert(stack.offset == offset_before_p3);
        assert(stack.prev_offset == offset_before_p2);

        stack_free_all(&stack);
        assert(stack.offset == 0);
        assert(stack.prev_offset == 0);
    }

    {
        void* fail_1 = stack_alloc(&stack, buf_size * 2, align);
        assert(fail_1 == NULL);
        assert(stack.offset == 0);
        assert(stack.prev_offset == 0);

        void* fail_2 = stack_resize(&stack, (void*)0x1, 2, 3, align);
        assert(fail_2 == NULL);
        assert(stack.offset == 0);
        assert(stack.prev_offset == 0);
        
        stack_free(&stack, (void*)0x01);
        assert(stack.offset == 0);
        assert(stack.prev_offset == 0);
    }

    free(buf_1k);
}

void pool_test()
{
    struct s16B
    {
        unsigned long long a;
        unsigned long long b;
    };

    size_t buf_size = 1024;
    char* buf_1k = (char*)malloc(buf_size);

    size_t align = 8;
    size_t chunk_size = 16;
    PoolAllocator pool = {0};
    pool_init(&pool, buf_1k, buf_size, chunk_size, align);
    assert(pool.buffer_size <= buf_size);
    assert(pool.chunk_size >= chunk_size);
    assert(pool.head != NULL);

    int node_count = 0;
    PoolListNode* node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count == (pool.buffer_size / pool.chunk_size));

    char* p1 = (char*)pool_alloc(&pool);
    assert(p1 != NULL);
    for (int i = 0; i < pool.chunk_size; i++) {
        p1[i] = 65 + i;
    }
    for (int i = 0; i < pool.chunk_size; i++) {
        assert(p1[i] == 65 + i);
    }

    int* p2 = (int*)pool_alloc(&pool);
    assert(p2 != NULL);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 2 == (pool.buffer_size / pool.chunk_size));

    s16B* p3_1 = (s16B*)pool_alloc(&pool);
    s16B* p3_2 = (s16B*)pool_alloc(&pool);
    s16B* p3_3 = (s16B*)pool_alloc(&pool);
    s16B* p3_4 = (s16B*)pool_alloc(&pool);
    assert(p3_1 != NULL);
    assert(p3_2 != NULL);
    assert(p3_3 != NULL);
    assert(p3_4 != NULL);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 6 == (pool.buffer_size / pool.chunk_size));

    pool_free(&pool, p2);
    assert((uintptr_t)pool.head == (uintptr_t)p2);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 5 == (pool.buffer_size / pool.chunk_size));

    pool_free(&pool, NULL);
    assert(pool.head != NULL);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 5 == (pool.buffer_size / pool.chunk_size));

    pool_free(&pool, (void*)0x01);
    assert((uintptr_t)pool.head != 0x01);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 5 == (pool.buffer_size / pool.chunk_size));

    pool_free(&pool, p1);
    assert((uintptr_t)pool.head == (uintptr_t)p1);

    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count + 4 == (pool.buffer_size / pool.chunk_size));

    pool_free_all(&pool);
    node_count = 0;
    node = pool.head;
    while (node != NULL) {
        node_count++;
        node = node->next;
    }
    assert(node_count == (pool.buffer_size / pool.chunk_size));

    free(buf_1k);
}

void free_list_test()
{
    size_t b2k = 2 * 1024 * sizeof(char);
    char* b2k_buf = (char*)malloc(b2k);
    FreeListAllocator free_list = { 0 };

    free_list_init(&free_list, b2k_buf, b2k, Allocation_Policy_First_Fit);

    int* a = (int*)free_list_alloc(&free_list, 4 * sizeof(int));
    for (int i = 0; i < 4; i++)
    {
        a[i] = i;
    }

    char* b = (char*)free_list_alloc(&free_list, 8 * sizeof(char));
    for (int i = 0; i < 8; i++)
    {
        b[i] = 'a' + i;
    }

    struct struct8byte
    {
        char c;
        int n;
    };
    assert(sizeof(struct8byte) == 8);
    struct8byte* c = (struct8byte*)free_list_alloc(&free_list, sizeof(struct8byte));
    c->c = '@';
    c->n = 42;

    free_list_free(&free_list, b);
    free_list_free(&free_list, c);

    int* d = (int*)free_list_alloc(&free_list, 16 * sizeof(int));
    for (int i = 0; i < 16; i++)
    {
        d[i] = i + 1;
    }
    
    char* e = (char*)free_list_alloc(&free_list, 32 * sizeof(char));
    for (int i = 0; i < 32; i++)
    {
        e[i] = 'A' + i;
    }

    free_list_free(&free_list, a);

    free_list_free_all(&free_list);

    free(b2k_buf);
    return;
}

void buddy_test()
{
    void* buf_128B = malloc(8 * POW_OF_2(4));

    BuddyAllocator buddy = { 0 };
    buddy_init(&buddy, buf_128B, 8 * POW_OF_2(4), 8);
    buddy_debug_print(&buddy);
    
    char* a = (char*)buddy_alloc(&buddy, 4);
    buddy_debug_print(&buddy);
    for (int i = 0; i < 4; i++) {
        a[i] = 65 + i;
    }

    char* b = (char*)buddy_alloc(&buddy, 9);
    buddy_debug_print(&buddy);
    for (int i = 0; i < 9; i++) {
        b[i] = 65 + i;
    }

    char* c = (char*)buddy_alloc(&buddy, 5);
    buddy_debug_print(&buddy);
    for (int i = 0; i < 5; i++) {
        c[i] = 65 + i;
    }

    char* d = (char*)buddy_alloc(&buddy, 10);
    buddy_debug_print(&buddy);
    for (int i = 0; i < 10; i++) {
        d[i] = 65 + i;
    }
    
    char* e = (char*)buddy_alloc(&buddy, 6);
    buddy_debug_print(&buddy);
    for (int i = 0; i < 6; i++) {
        e[i] = 65 + i;
    }

    buddy_free(&buddy, b);
    buddy_debug_print(&buddy);

    buddy_free(&buddy, d);
    buddy_debug_print(&buddy);

    buddy_free(&buddy, a);
    buddy_debug_print(&buddy);

    buddy_free(&buddy, c);
    buddy_debug_print(&buddy);

    //buddy_free(&buddy, e);
    //buddy_allocator_debug_print_tree(&buddy);
    buddy_free_all(&buddy);
    buddy_debug_print(&buddy);

    buddy_destory(&buddy);
    free(buf_128B);
}

void memory_test()
{
    arena_test();

    stack_test();

    pool_test();

    free_list_test();
    
    buddy_test();
}

int main(void)
{
    memory_test();
}
