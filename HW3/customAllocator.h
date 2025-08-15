#ifndef CUSTOM_ALLOCATOR
#define CUSTOM_ALLOCATOR


#include <stdbool.h>



/*=============================================================================
 * Do not edit lines below!// but need t_size?
=============================================================================*/
#include <stddef.h> //for size_t
typedef size_t t_size;
void* customMalloc(t_size size);
void customFree(void* ptr);
void* customCalloc(t_size nmemb, t_size size);
void* customRealloc(void* ptr, t_size size);
/*=============================================================================
 * Do not edit lines above!
=============================================================================*/

void heapCreate();
void heapKill();

#define SBRK_FAIL (void*)(-1)
#define ALIGN_TO_MULT_OF_4(x) (((((x) - 1) >> 2) << 2) + 4)

/*=============================================================================
 * If writing bonus - uncomment lines below
=============================================================================*/
#ifndef BONUS
#define BONUS
#endif

void* customMTMalloc(t_size size);
void customMTFree(void* ptr);
void* customMTCalloc(t_size nmemb, t_size size);
void* customMTRealloc(void* ptr, t_size size);

/*=============================================================================
 * Block 
=============================================================================*/
typedef struct Block {
    t_size size;
    bool free;
    struct Block* next;
} Block;

#endif // CUSTOM_ALLOCATOR
