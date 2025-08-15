#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include "customAllocator.h"

#define METADATA_SIZE (sizeof(Block))
#define ARENA_COUNT 8
#define ARENA_SIZE 4096

void* sbrk(intptr_t increment);

static pthread_mutex_t sbrk_lock = PTHREAD_MUTEX_INITIALIZER;
static Block* base_head = NULL;

static t_size align4(t_size size) {
    return ALIGN_TO_MULT_OF_4(size);
}

// heap pointer check
static bool is_heap_ptr(Block* head, void* ptr) {
    Block* curr = head;
    while (curr) {
        if (curr->size == 0 || curr->size > (1UL << 30)) break;
        void* user_start = (void*)(curr + 1);
        void* user_end = (void*)((char*)user_start + curr->size);
        if (ptr >= user_start && ptr < user_end) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}

// connect free block until cant anymore
static void connect_blocks(Block* head) {
    if (!head) return;
    int merged;
    do {
        merged = 0;
        Block* curr = head;
        while (curr && curr->next) {
            if (curr->free && curr->next->free) {
                curr->size += METADATA_SIZE + curr->next->size;
                curr->next = curr->next->next;
                merged = 1;
            } else {
                curr = curr->next;
            }
        }
    } while (merged);
    Block* last = head;
    while (last && last->next) last = last->next;
    if (last) last->next = NULL;
}
//find best spot for new block
static Block* find_best_fit(Block* head, t_size size) {
    Block* curr = head;
    Block* best = NULL;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (!best || curr->size < best->size) {
                best = curr;
            }
        }
        curr = curr->next;
    }
    return best;
}
//req mem space
static Block* request_space(Block** head_ref, t_size size) {
    errno = 0;
    pthread_mutex_lock(&sbrk_lock);
    void* ptr = sbrk(METADATA_SIZE + size);
    pthread_mutex_unlock(&sbrk_lock);
    if (ptr == SBRK_FAIL) {
        if (errno == ENOMEM) {
            printf("<sbrk/brk error>: out of memory\n");//out of mem err
            _exit(1);
        } else {
            printf("<sbrk/brk error>: invalid sbrk/brk call\n");
            return NULL;
        }
    }
    Block* block = (Block*)ptr;
    block->size = size;
    block->free = false;
    block->next = NULL;

    if (!*head_ref) {
        *head_ref = block;
    } else {
        Block* last = *head_ref;
        while (last->next) last = last->next;
        last->next = block;
        block->next = NULL;
    }
    return block;
}

static void split_block(Block* block, t_size size) {
    if (block->size >= size + METADATA_SIZE + 4) {
        Block* new_block = (Block*)((char*)block + METADATA_SIZE + size);
        new_block->size = block->size - size - METADATA_SIZE;
        new_block->free = true;
        new_block->next = block->next;
        block->size = size;
        block->next = new_block;
    }
}
//our malloc func
void* customMalloc(t_size size) {
    if (size == 0) return NULL;
    size = align4(size);
    Block* block = find_best_fit(base_head, size);
    if (block) {
        block->free = false;
        split_block(block, size);
        return (void*)(block + 1);
    }
    block = request_space(&base_head, size);
    if (!block) return NULL;
    return (void*)(block + 1);
}
//free mem func
void customFree(void* ptr) {
    if (!ptr) {
        printf("<free error>: passed null pointer\n");//err msg
        return;
    }
    if (!is_heap_ptr(base_head, ptr)) {
        printf("<free error>: passed non-heap pointer\n");
        return;
    }
    Block* block = (Block*)ptr - 1;
    block->free = true;
    connect_blocks(base_head);
    //try feree
    Block* last = base_head, *prev = NULL;
    while (last && last->next) {
        prev = last;
        last = last->next;
    }
    if (last && last->free) {
        if (prev)
            prev->next = NULL;
        else
            base_head = NULL;
        sbrk(-(long)(METADATA_SIZE + last->size));
    }
}

void* customCalloc(t_size nmemb, t_size size) {
    t_size total = nmemb * size;
    void* ptr = customMalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* customRealloc(void* ptr, t_size size) {
    if (!ptr) return customMalloc(size);
    if (size == 0) {
        customFree(ptr);
        return NULL;
    }
    if (!is_heap_ptr(base_head, ptr)) {
        printf("<realloc error>: passed non-heap pointer\n");//err msg
        return NULL;
    }
    Block* old_block = (Block*)ptr - 1;
    size = align4(size);
    if (size <= old_block->size) {
        split_block(old_block, size);
        return ptr;
    }
    void* new_ptr = customMalloc(size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, old_block->size < size ? old_block->size : size);
    customFree(ptr);
    return new_ptr;
}

//bonus part
typedef struct Arena {
    Block* head;
    pthread_mutex_t lock;
    char buffer[ARENA_SIZE];
    size_t used;
} Arena;

static Arena arenas[ARENA_COUNT];
static int current_arena = 0;


void heapCreate() {
    for (int i = 0; i < ARENA_COUNT; ++i) {
        arenas[i].head = NULL;
        arenas[i].used = 0;
        pthread_mutex_init(&arenas[i].lock, NULL);
    }
}

void heapKill() {
    for (int i = 0; i < ARENA_COUNT; ++i) {
        pthread_mutex_destroy(&arenas[i].lock);
        arenas[i].head = NULL;
    }
}

//alloc new block from buffer
static Block* arena_alloc_block(Arena* arena, t_size size) {
    if (arena->used + METADATA_SIZE + size > ARENA_SIZE) return NULL;
    Block* block = (Block*)(arena->buffer + arena->used);
    block->size = size;
    block->free = false;
    block->next = NULL;
    arena->used += METADATA_SIZE + size;
    if (!arena->head) {
        arena->head = block;
    } else {
        Block* last = arena->head;
        while (last->next) last = last->next;
        last->next = block;
    }
    return block;
}

void* customMTMalloc(t_size size) {
    if (size == 0) return NULL;
    size = align4(size);
    for (int i = 0; i < ARENA_COUNT; ++i) {
        int arena_idx = (current_arena + i) % ARENA_COUNT;
        Arena* arena = &arenas[arena_idx];
        pthread_mutex_lock(&arena->lock);
        Block* block = find_best_fit(arena->head, size);
        if (block) {
            block->free = false;
            split_block(block, size);
            pthread_mutex_unlock(&arena->lock);
            current_arena = (arena_idx + 1) % ARENA_COUNT;
            return (void*)(block + 1);
        }
        block = arena_alloc_block(arena, size);
        if (block) {
            pthread_mutex_unlock(&arena->lock);
            current_arena = (arena_idx + 1) % ARENA_COUNT;
            return (void*)(block + 1);
        }
        pthread_mutex_unlock(&arena->lock);
    }
    return NULL;
}

void customMTFree(void* ptr) {
    if (!ptr) {
        printf("<free error>: passed null pointer\n");
        return;
    }
    for (int i = 0; i < ARENA_COUNT; ++i) {
        Arena* arena = &arenas[i];
        pthread_mutex_lock(&arena->lock);
        if (is_heap_ptr(arena->head, ptr)) {
            Block* block = (Block*)ptr - 1;
            block->free = true;
            connect_blocks(arena->head);
            // free mem
            Block* last = arena->head, *prev = NULL;
            while (last && last->next) {
                prev = last;
                last = last->next;
            }
            if (last && last->free) {
                if (prev)
                    prev->next = NULL;
                else
                    arena->head = NULL;
                sbrk(-(long)(METADATA_SIZE + last->size));
            }
            pthread_mutex_unlock(&arena->lock);
            return;
        }
        pthread_mutex_unlock(&arena->lock);
    }
    printf("<free error>: passed non-heap pointer\n");
}

void* customMTCalloc(t_size nmemb, t_size size) {
    t_size total = nmemb * size;
    void* ptr = customMTMalloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void* customMTRealloc(void* ptr, t_size size) {
    if (!ptr) return customMTMalloc(size);
    if (size == 0) {
        customMTFree(ptr);
        return NULL;
    }
    for (int i = 0; i < ARENA_COUNT; ++i) {
        Arena* arena = &arenas[i];
        pthread_mutex_lock(&arena->lock);
        if (is_heap_ptr(arena->head, ptr)) {
            Block* old_block = (Block*)ptr - 1;
            size = align4(size);
            if (size <= old_block->size) {
                split_block(old_block, size);
                pthread_mutex_unlock(&arena->lock);
                return ptr;
            }
            pthread_mutex_unlock(&arena->lock);
            void* new_ptr = customMTMalloc(size);
            if (!new_ptr) return NULL;
            memcpy(new_ptr, ptr, old_block->size);
            customMTFree(ptr);
            return new_ptr;
        }
        pthread_mutex_unlock(&arena->lock);
    }
    printf("<realloc error>: passed non-heap pointer\n");
    return NULL;
}
