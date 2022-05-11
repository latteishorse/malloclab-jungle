#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    "ateam",
    "Harry Bovik",
    "bovik@cs.cmu.edu",
    "",
    ""
};

#define ALIGNMENT           8
#define WSIZE               4
#define DSIZE               8
#define CHUNKSIZE           (1<<12)
#define MAX(x, y)           ((x) > (y) ? (x) : (y))
#define PACK(size, alloc)   ((size) | (alloc))
#define GET(p)              (*(unsigned int *)(p))
#define PUT(p, val)         (*(unsigned int *)(p) = (val))
#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define HDRP(bp)            ((char *)(bp) - WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))
#define PREV_FREE(bp)       (*(char **)(bp))
#define NEXT_FREE(bp)       (*(char **)(bp + WSIZE))
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)

static char * free_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit_first(size_t asize);
static void place(void *bp, size_t asize);
static void add_block(void *bp);
static void remove_block(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char * heap_listp;
    if((heap_listp = (char*)mem_sbrk(WSIZE*6)) == (void *)-1)
        return -1;

    PUT(heap_listp,             0);     
    PUT(heap_listp + WSIZE,     PACK(4*WSIZE, 1));    /* Prologue Header */ 
    PUT(heap_listp + 2*WSIZE,   PACK(0, 0));  /* Pred */ 
    PUT(heap_listp + 3*WSIZE,   PACK(0, 0));  /* Succ */ 
    PUT(heap_listp + 4*WSIZE,   PACK(4*WSIZE, 1));  /* Prologue Footer */ 
    PUT(heap_listp + 5*WSIZE,   PACK(0, 1));  /* Epilogue Header */ 

    free_listp = heap_listp + 2*WSIZE;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize = ALIGN(size + DSIZE); 
    size_t extendsize = MAX(newsize, CHUNKSIZE);
    char * bp;

    if(!size){
        return NULL;
    }
    
    if((bp = find_fit_first(newsize)) != NULL){
        place(bp, newsize);
        remove_block(bp);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, newsize);
    remove_block(bp);
    return bp; 
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

// void *mm_realloc(void *ptr, size_t size)
// {
//     if(!size){
//         mm_free(ptr);
//         return ptr;
//     }
//     if(ptr == NULL){
//         return mm_malloc(size);
//     }
//     else{
//         size_t new_size = ALIGN(size + DSIZE); // Adjusted block size
//         size_t block_size = GET_SIZE(HDRP(ptr));
//         if (new_size <= block_size){
//             place(ptr, new_size);
//             return ptr;
//         }
//         else{
//             void * new_ptr = mm_malloc(size);
//             if (new_ptr == NULL)
//                 return NULL;
//             memcpy(new_ptr, ptr, size);
//             mm_free(ptr);
//             return new_ptr;
//         }
//     }    
// }

/*
 * extend_heap - extend heap by words 
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size = (words & 1) ? (words+1) * WSIZE : words * WSIZE;
    if((size_t)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));           
    PUT(FTRP(bp), PACK(size, 0));           
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   

    return coalesce(bp);
}

static void *coalesce(void * bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1
    if (prev_alloc && next_alloc){
        add_block(bp);
        return bp;
    }

    //case 2
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    //case 3
    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 

    //case 4
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_block(NEXT_BLKP(bp));
        remove_block(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    add_block(bp);
    return bp;
}


static void add_block(void *bp){
    NEXT_FREE(bp) = free_listp;
    PREV_FREE(bp) = NULL;
    PREV_FREE(free_listp) = bp;
    free_listp = bp;
}


static void remove_block(void *bp){
    if(bp == free_listp){
        PREV_FREE(NEXT_FREE(bp)) = NULL;
        free_listp = NEXT_FREE(bp);
    }
    else {
        PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
        NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
    }
}


static void *find_fit_first(size_t asize)
{
    void * bp;
    for(bp = free_listp; !GET_ALLOC(HDRP(bp)); bp = NEXT_FREE(bp)){
        if(GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
    }
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t block_size = GET_SIZE(HDRP(bp));
    /* case: block should be splitted */
    if(block_size - asize >= 2*DSIZE){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(block_size-asize, 0));
        PUT(FTRP(bp), PACK(block_size-asize, 0));
        add_block(bp);
    }
    /* case: block is not splitted */
    else{
        PUT(HDRP(bp), PACK(block_size, 1));
        PUT(FTRP(bp), PACK(block_size, 1));
    }
}
