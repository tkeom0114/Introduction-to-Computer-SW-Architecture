/*
 * mm.c
 * Author: Taekang Eom(tkeom0114)
 * 
 * Memory allocator based on segregated list follow best fit pollicy.
 * Frist, each block has header and footer of the form:
 *
 *      31                     3  2  1  0
 *      -----------------------------------
 *     | ( size of the block  )  0  0  a/f(allocate or free)
 *      -----------------------------------
 *  Then, free block is the form: header-prev address in seglist-next address in seglist- paddings ... - footer
 *  and allocated block is the form: header-payload... - footer
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* My additional Macros*/
#define SEGLIST_LEVEL 20
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 11)
#define REALLOCCHUNK (3 << 13)
#define LARGEBLOCK (3 << 5)

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))                             // pack with size and allocation tag
#define GET(p)       (*(unsigned int *)(p))
#define SET(p, val)  (*(unsigned int *)(p) = (unsigned int) val)
#define GET_SIZE(p)  (GET(p) & ~0x7)                                     // all sizes are divided by 8
#define GET_ALLOC(p) (GET(p) & 0x1)                                      // get allocation tag
#define HEAD(ptr)       ((char *)(ptr) - WSIZE)                          // get header of the block
#define FOOT(ptr)       ((char *)(ptr) + GET_SIZE(HEAD(ptr)) - DSIZE)    // get footer of the block
#define PPREV(ptr)  ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DSIZE)))  //get the previous block in the heap
#define PNEXT(ptr)  ((char *)(ptr) + GET_SIZE(((char *)(ptr) - WSIZE)))  //get the next block in the heap
#define PREV_PTR(ptr) ((char *)(ptr))                                    //get address of the pointer of next block in the seglist
#define NEXT_PTR(ptr) ((char *)(ptr) + WSIZE)                            //get address of the pointer of previous block in the seglist
#define PREV(ptr) (*(char **)(ptr))                                      //get address of the previous block in the seglist
#define NEXT(ptr) (*(char **)(NEXT_PTR(ptr)))                            //get address of the next block in the seglist



/* Declare of global variable (segregation list)*/

void* seglist[SEGLIST_LEVEL];

/* Declare of helper functions */

static void *extend_heap(size_t size);
static void *coalesce(void *ptr, int realloc);
static void seg_insert(void *ptr, size_t size);
static void seg_delete(void *ptr);
static void *find_block(size_t size);
static void *allocate_block(void *ptr, void *oldptr, size_t newsize, size_t oldsize, int realloc);
static size_t new_size(size_t size);
static int is_contain(void *address); // Check all free blocks are contained in the seglist
static int check_seglist(); // Check whether all blocks in the seglist are free block
int mm_check(void);

/* Extend the heap if heap space is not enough to allocate memory. Also insert new free heap space to segregation list. */
static void *extend_heap(size_t size)
{
    void *ptr;                   
    size_t newsize;
    
    newsize = ALIGN(size);
    
    ptr = mem_sbrk(newsize);
    if(ptr == (void *)-1)
        return NULL;

    SET(HEAD(ptr), PACK(newsize, 0));  
    SET(FOOT(ptr), PACK(newsize, 0));   
    SET(HEAD(PNEXT(ptr)), PACK(0, 1)); 
    seg_insert(ptr, newsize);

    return coalesce(ptr, 0);
}

/* Insert new free block to segregation list. */
static void seg_insert(void *ptr, size_t size)
{
    int list_index = 0;
    void *search_ptr;
    void *insert_ptr = NULL;
    
    /* Find the list index that the new free block is inserted. */
    while((list_index < SEGLIST_LEVEL - 1) && (size > 1))
    {
        size >>= 1;
        list_index++;
    }
    
    search_ptr = seglist[list_index];
    while((search_ptr != NULL) && (size > GET_SIZE(HEAD(search_ptr))))
    {
        insert_ptr = search_ptr;
        search_ptr = NEXT(search_ptr);
    }

    if(search_ptr != NULL)
    {
        /* Insert the block among the list */
        if(insert_ptr != NULL)
        {
            SET(NEXT_PTR(ptr), search_ptr);
            SET(PREV_PTR(search_ptr), ptr);
            SET(NEXT_PTR(insert_ptr), ptr);
            SET(PREV_PTR(ptr), insert_ptr);
        }
        /* Insert the block front of the list */
        else
        {
            SET(NEXT_PTR(ptr), search_ptr);
            SET(PREV_PTR(search_ptr), ptr);
            SET(PREV_PTR(ptr), NULL);
            seglist[list_index] = ptr;
        }
    }
    else
    {
         /* Insert the block back of the list */
        if(insert_ptr != NULL)
        {
            SET(PREV_PTR(ptr),insert_ptr);
            SET(NEXT_PTR(ptr), NULL);
            SET(NEXT_PTR(insert_ptr), ptr);
        }
        /* The block is the first element of the list */
        else
        {
            SET(PREV_PTR(ptr), NULL);
            SET(NEXT_PTR(ptr), NULL);
            seglist[list_index] = ptr;
        }
    }
    return;
}

/* Delete the memory block when the block is allocated or coalesced. */
static void seg_delete(void *ptr) {
    int list_index = 0;
    size_t size = GET_SIZE(HEAD(ptr));

    /* Find the list index that the block is deleted. */
    while((list_index < SEGLIST_LEVEL - 1) && (size > 1))
    {
        size >>= 1;
        list_index++;
    }
    
    if(PREV(ptr) != NULL)
    {
        /* Delete the block among the list */
        if(NEXT(ptr) != NULL)
        {
            SET(NEXT_PTR(PREV(ptr)), NEXT(ptr));
            SET(PREV_PTR(NEXT(ptr)), PREV(ptr));
        }
        /* Delete the block front of the list */
        else
        {
            SET(NEXT_PTR(PREV(ptr)), NULL);
        }
    } 
    else
    {
        /* Insert the block back of the list */
        if(NEXT(ptr) != NULL)
        {
            SET(PREV_PTR(NEXT(ptr)), NULL);
            seglist[list_index] = NEXT(ptr);
        }
        /* The block is the unique element of the list */
        else
        {
            seglist[list_index] = NULL;
        }
    }
    
    return;
}

/* Coalesce the free block if new free block is between of free blocks. */
static void *coalesce(void *ptr, int realloc)
{
    size_t prev_alloc = GET_ALLOC(HEAD(PPREV(ptr)));
    size_t next_alloc = GET_ALLOC(HEAD(PNEXT(ptr)));
    size_t size = GET_SIZE(HEAD(ptr));
    /* Previous block and next block are all allocated already */
    if (prev_alloc && next_alloc)
    {
        return ptr;
    }
    /* Next of the block is free. */
    else if (prev_alloc && !next_alloc)
    {
        if(!realloc)
            seg_delete(ptr);
        seg_delete(PNEXT(ptr));
        size += GET_SIZE(HEAD(PNEXT(ptr)));
        SET(HEAD(ptr), PACK(size, 0));
        SET(FOOT(ptr), PACK(size, 0));
    }
    /* Previous of the block is free. */
    else if (!prev_alloc && next_alloc)
    {
        if(!realloc)
            seg_delete(ptr);
        seg_delete(PPREV(ptr));
        size += GET_SIZE(HEAD(PPREV(ptr)));
        SET(FOOT(ptr), PACK(size, 0));
        SET(HEAD(PPREV(ptr)), PACK(size, 0));
        ptr = PPREV(ptr);
    }
    /* Previous block and next block are free block */
    else
    {
        if(!realloc)
            seg_delete(ptr);
        seg_delete(PPREV(ptr));
        seg_delete(PNEXT(ptr));
        size += GET_SIZE(HEAD(PPREV(ptr))) + GET_SIZE(HEAD(PNEXT(ptr)));
        SET(HEAD(PPREV(ptr)), PACK(size, 0));
        SET(FOOT(PNEXT(ptr)), PACK(size, 0));
        ptr = PPREV(ptr);
    }
    /* Insert the coalesce block to the seglist */
    if(!realloc)
        seg_insert(ptr, size);
    
    return ptr;
}

/* Find the address which can allocate newsize and retrun the address.
 * Return NULL if find address is failed
 */
static void *find_block(size_t newsize)
{
    void *ptr = NULL;
    int list_index = 0;
    size_t tempsize = newsize;
    while (list_index < SEGLIST_LEVEL)
    {
        if((list_index == SEGLIST_LEVEL - 1) || ((tempsize <= 1) && (seglist[list_index] != NULL)))
        {
            ptr = seglist[list_index];
            while((ptr != NULL) && (newsize > GET_SIZE(HEAD(ptr))))
            {
                ptr = NEXT(ptr);
            }
            if(ptr != NULL)
                break;
        }
        tempsize >>= 1;
        list_index++;
    }
    return ptr;
}

/* Allocate block to the address which find using find_block. */
static void *allocate_block(void *ptr, void *oldptr, size_t newsize, size_t oldsize, int realloc)
{
    size_t ptr_size = GET_SIZE(HEAD(ptr));
    size_t remainder = ptr_size - newsize;
    if(!realloc)
        seg_delete(ptr); 
    
    if(remainder <= DSIZE * 2)
    {
        if(oldptr != NULL)
            memmove(ptr, oldptr, MIN(newsize - DSIZE, oldsize - DSIZE));
        SET(HEAD(ptr), PACK(ptr_size, 1));
        SET(FOOT(ptr), PACK(ptr_size, 1)); 
    }
    else if(newsize > LARGEBLOCK)
    {
        if(oldptr != NULL)
            memmove(ptr+remainder, oldptr, MIN(newsize - DSIZE, oldsize - DSIZE));
        SET(HEAD(ptr), PACK(remainder, 0)); 
        SET(FOOT(ptr), PACK(remainder, 0)); 
        SET(HEAD(PNEXT(ptr)), PACK(newsize, 1)); 
        SET(FOOT(PNEXT(ptr)), PACK(newsize, 1)); 
        seg_insert(ptr, remainder);
        return PNEXT(ptr);
    }
    else
    {
        if(oldptr != NULL)
            memmove(ptr, oldptr, MIN(newsize - DSIZE, oldsize - DSIZE));
        SET(HEAD(ptr), PACK(newsize, 1)); 
        SET(FOOT(ptr), PACK(newsize, 1)); 
        SET(HEAD(PNEXT(ptr)), PACK(remainder, 0)); 
        SET(FOOT(PNEXT(ptr)), PACK(remainder, 0)); 
        seg_insert(PNEXT(ptr), remainder);
    }
    return ptr;
}

/* The function allign the block. */
static size_t new_size(size_t size)
{
    if(size <= DSIZE)
        return 2 *DSIZE;
    else
        return ALIGN(size + DSIZE);
}


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    int list;         
    char *heap; // Pointer to beginning of heap
    
    // Initialize segregated free lists
    for (list = 0; list < SEGLIST_LEVEL; list++) {
        seglist[list] = NULL;
    }
    
    // Allocate memory for the initial empty heap 
    heap = mem_sbrk(4 * WSIZE);
    if(heap == (void *)-1)
        return -1;
    
    SET(heap, 0);
    SET(heap + (1 * WSIZE), PACK(DSIZE, 1));
    SET(heap + (2 * WSIZE), PACK(DSIZE, 1));
    SET(heap + (3 * WSIZE), PACK(0, 1));

    return 0;
}

/* 
 * mm_malloc - Allocate a block by best fit pollicy 
 * and increase brk pointer if heap space is not enough.
 */
void *mm_malloc(size_t size)
{
    size_t newsize = new_size(size);
    void *ptr = find_block(newsize);

    if(ptr == NULL)
    {
        ptr = extend_heap(MAX(newsize,CHUNKSIZE));
        if(ptr == NULL)
            return NULL;
    }

    return allocate_block(ptr, NULL, newsize,0, 0);
}

/*
 * mm_free - Set the block to free and coalese with neighborhood free blocks.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HEAD(ptr));
    SET(HEAD(ptr), PACK(size, 0));
    SET(FOOT(ptr), PACK(size, 0));
    seg_insert(ptr, size);
    coalesce(ptr, 0);
    return;
}

/*
 * mm_realloc - To improve utilize, coleasce with neighborhood of the block first 
 * and find new free block to copy memory.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *tempptr;
    void *newptr;
    size_t newsize = new_size(size);
    size_t oldsize = GET_SIZE(HEAD(oldptr));
    SET(HEAD(oldptr), PACK(oldsize, 1));
    SET(FOOT(oldptr), PACK(oldsize, 1));
    tempptr = coalesce(oldptr, 1);
    newptr = find_block(newsize);
   
    if(newsize > GET_SIZE(HEAD(tempptr)) || (newptr != NULL && GET_SIZE(HEAD(newptr)) < GET_SIZE(HEAD(tempptr))))
    {
        if (newptr == NULL)
        {
            newptr = extend_heap(MAX(newsize, REALLOCCHUNK));
            if (newptr == NULL)
                return NULL;
        }
        
        newptr = allocate_block(newptr, oldptr, newsize, oldsize, 0);
        seg_insert(tempptr, GET_SIZE(HEAD(tempptr)));
    }
    else
    {
        newptr = allocate_block(tempptr, oldptr, newsize, oldsize, 1);
    }
    
    return newptr;
}

/**
 * mm_check check consistency of the heap by follwing tests.
 * 1.check size header and footer of all blocks are correctly
 * 2.check tag of header and footer of all blocks are correctly by serching seglist
 * 3.free blocks in the heap are coalesced correctly
 * 4.allocated blocks are not contained in seglist
 * 5.each seglist sorted correctly
 */
int mm_check(void)
{
    int prev_alloc = 1;
    void *cur_block = mem_heap_lo() + DSIZE;
    while(GET_SIZE(HEAD(cur_block)) > 0)
    {
        if(GET_ALLOC(HEAD(cur_block)))
        {
            prev_alloc = 1;
        }
        else
        {
            if(!is_contain(cur_block))
            {
                printf("Seglist doesn't contain a free block in %lx.\n",(unsigned long) cur_block);
                return 0;
            }
            if(prev_alloc == 0)
            {
                printf("Previous block of %lx is not coalesced.\n",(unsigned long) cur_block);
                return 0;
            }
            prev_alloc = 0;
        }
        if(GET(HEAD(cur_block)) != GET(FOOT(cur_block)))
        {
            printf("Head and foot of the block at %lx is different.\n",(unsigned long) cur_block);
            return 0;
        }
        cur_block = PNEXT(cur_block);
    }
    if(cur_block - 1 != mem_heap_hi())
    {
        printf("Last block is not aligned at the last part of heap!\n");
        return 0;
    }

    return check_seglist();
}

static int is_contain(void *address)
{
    void *ptr = NULL;
    int list_index = 0;
    while (list_index < SEGLIST_LEVEL)
    {
        ptr = seglist[list_index];
        while (ptr != NULL)
        {
            if(ptr == address)
                break;
            ptr = NEXT(ptr);
        }
        if (ptr == address)
            break;
        list_index++;
    }
    return (ptr != NULL);
}

static int check_seglist()
{
    void *ptr = NULL;
    int list_index = 0;
    int prev_size;
    while (list_index < SEGLIST_LEVEL)
    {
        ptr = seglist[list_index];
        prev_size = 0;
        while (ptr != NULL)
        {
            if(GET_ALLOC(HEAD(ptr)) || GET_ALLOC(FOOT(ptr)))
            {
                printf("Allocated block at %lx contained in seglist.\n", (unsigned long) ptr);
                return 0;
            }
            if(prev_size > GET_SIZE(HEAD(ptr)))
            {
                printf("Free block at %lx is bigger than previous node of seglist.\n", (unsigned long) ptr);
                return 0;
            }
            ptr = NEXT(ptr);
        }
        list_index++;
    }
    return 1;
}