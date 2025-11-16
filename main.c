#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

typedef struct block {
    size_t size;        // size of the user area
    int free;           // 1 = free, 0 = used
    struct block *next; // next block in the list
} block_t;

#define BLOCK_SIZE sizeof(block_t)

static block_t *head = NULL;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static block_t *find_free_block(size_t size)
{
    block_t *curr = head;
    while (curr) {
        if (curr->free && curr->size >= size)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

static block_t *request_space(size_t size)
{
    void *p = sbrk(size + BLOCK_SIZE);
    if (p == (void*)-1)
        return NULL;

    block_t *block = (block_t*)p;
    block->size = size;
    block->free = 0;
    block->next = NULL;
    return block;
}

static void split_block(block_t *block, size_t size)
{
    if (block->size <= size + BLOCK_SIZE)
        return;

    block_t *new_block = (block_t*)((char*)block + BLOCK_SIZE + size);
    new_block->size = block->size - size - BLOCK_SIZE;
    new_block->free = 1;
    new_block->next = block->next;

    block->size = size;
    block->next = new_block;
}

static void merge_blocks()
{
    block_t *curr = head;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void *malloc(size_t size)
{
    if (size == 0)
        return NULL;

    pthread_mutex_lock(&lock);

    block_t *block = find_free_block(size);
    if (block) {
        block->free = 0;
        split_block(block, size);
        pthread_mutex_unlock(&lock);
        return (block + 1);
    }

    block_t *new_block = request_space(size);
    if (!new_block) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }

    if (!head)
        head = new_block;
    else {
        block_t *last = head;
        while (last->next)
            last = last->next;
        last->next = new_block;
    }

    pthread_mutex_unlock(&lock);
    return (new_block + 1);
}

void free(void *ptr)
{
    if (!ptr)
        return;

    pthread_mutex_lock(&lock);

    block_t *block = (block_t*)ptr - 1;
    block->free = 1;

    merge_blocks();

    pthread_mutex_unlock(&lock);
}

void *calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void *p = malloc(total);
    if (!p)
        return NULL;

    memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr)
        return malloc(size);

    block_t *block = (block_t*)ptr - 1;

    if (block->size >= size)
        return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr)
        return NULL;

    memcpy(new_ptr, ptr, block->size);
    free(ptr);
    return new_ptr;
}

void print_heap()
{
    block_t *curr = head;
    printf("\n");
    while (curr) {
        printf("%p | size=%zu | free=%d | next=%p\n",
               (void*)curr, curr->size, curr->free, (void*)curr->next);
        curr = curr->next;
    }
    printf("\n");
}
