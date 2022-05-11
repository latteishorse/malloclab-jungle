#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

team_t team = {
    "SWJungle",
    "latteishorse",
    "week06",
    "",
    ""};

// Macro for malloc 
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE   4  
#define DSIZE   8   
#define CHUNKSIZE (1<<12) 
#define MAX(x, y) ((x)>(y)?(x):(y))

// Pack a size and allocated bit into a word
/*
alloc: 가용 여부
size: block size
*/
#define PACK(size, alloc) ((size)|(alloc))

// Read and write a word at address p 
#define GET(p)  (*(unsigned int *)(p)) // 주소값 조회
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 블록의 주소를 넣음

// Read the size and allocation bit from address p 
#define GET_SIZE(p)     (GET(p)&~0x7)
// ~ -> 역수
// 헤더에서 블록 size만 조회

#define GET_ALLOC(p)    (GET(p)&0x1)
// 00000001 -> 헤더 가용 여부 판단
// 1 -> allocated


// Address of block's header and footer 
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

// Address of (physically) next and previous blocks 
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 연결 리스트 구현을 위한 포인터 */
#define PRED_P(bp)  (*(void **)(bp)) // 이전 가용 블록 주소
#define SUCC_P(bp)  (*(void **)((bp)+WSIZE)) // 다음 가용 블록 주소

// global var
static void *heap_listp;

// functions
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *p, size_t size);
// explicit
static void list_add(void *p);
static void list_remove(void *p);

/* -------------------- block information  -----------------------

  * * * Free block * * *         

---------------------------
|      block size     | a |   allocated 1:True 0:False
---------------------------
|      next pointer       |   
---------------------------
|      prev pointer       |
---------------------------
|                         |
|                         |
|                         |
---------------------------
|      block size     | a |
---------------------------


// ------------------- end of block information --------------------- */

int mm_init(void) {   
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0); // padding
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1)); // prolog -> double word size & allocated 1
    PUT(heap_listp + (2*WSIZE), heap_listp+(3*WSIZE));
    PUT(heap_listp + (3*WSIZE), heap_listp+(2*WSIZE));
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1)); // prolog footer
    PUT(heap_listp + (5*WSIZE), PACK(0,1)); // epilogue header

    heap_listp += 2*WSIZE;
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
* extend heap
* 1) 힙이 초기화 될 때
* 2) mm_malloc이 적당한 fit을 찾지 못했을 때 
*/
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE; // 인접한 힙을 2워드의 배수로 반올림, 그 후에 메모리 시스템으로부터 추가적인 힙 공간을 요청
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0)); // HDRP bp -> wsize 앞으로

    PUT(FTRP(bp), PACK(size, 0)); // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //추가한 블럭, allocate 
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;  //= ALIGN(size + DSIZE);
    size_t extendsize; //= MAX(asize, CHUNKSIZE);
    char *bp;

    if (size == 0)
        return NULL;
    if (size<=DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1))/DSIZE);
        // size보다 클 때 -> 큰 범위 내에서 만족하는 가장 작은 값 (8*n + (0~7))으로 분할 가능
        // size + (header) + (padding)

    // 들어갈 free block이 있으면 넣어줌
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize); // asize로 할당
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
* find_fit -> using first fit 
*/
static void *find_fit(size_t asize){
    void *ptr;
    for (ptr = SUCC_P(heap_listp); !GET_ALLOC(HDRP(ptr)) ; ptr = SUCC_P(ptr)){
        if (asize <= GET_SIZE(HDRP(ptr))){
            return ptr;
        }
    }
    return NULL;
}

/*
* place
*/
static void place(void *p, size_t size){
    size_t free_block = GET_SIZE(HDRP(p));
    list_remove(p);
    if ((free_block-size)>=(2*DSIZE)){
        PUT(HDRP(p), PACK(size, 1));
        PUT(FTRP(p), PACK(size, 1));
        p = NEXT_BLKP(p);
        PUT(HDRP(p), PACK(free_block-size, 0));
        PUT(FTRP(p), PACK(free_block-size, 0));
        list_add(p);
    } 
    else {
    // 자리가 없다면
    // 현재 사이즈에서 헤더, 푸터 할당
    PUT(HDRP(p), PACK(free_block, 1));
    PUT(FTRP(p), PACK(free_block, 1));
    }
}

static void list_add(void *p){
    SUCC_P(p) = SUCC_P(heap_listp);
    PRED_P(p) = heap_listp;
    PRED_P(SUCC_P(heap_listp)) = p;
    SUCC_P(heap_listp) = p;
}

static void list_remove(void *p){
    SUCC_P(PRED_P(p)) = SUCC_P(p);
    PRED_P(SUCC_P(p)) = PRED_P(p);
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
*  coalesce - 블록 연결
*/
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc){
        list_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } 
    else if (!prev_alloc && next_alloc){
        list_remove(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } 
    else if (!prev_alloc && !next_alloc){
        list_remove(PREV_BLKP(bp));
        list_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    list_add(bp);
    return bp;
}

void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copy_size;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copy_size = GET_SIZE(HDRP(oldptr));
    if (size < copy_size)
      copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    mm_free(oldptr);
    return newptr;
}
