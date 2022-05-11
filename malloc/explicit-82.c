#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "SWJungle",
    /* First member's full name */
    "latteishorse",
    /* First member's email address */
    "week06",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE   4  
#define DSIZE   8   
#define CHUNKSIZE (1<<12) 
#define MAX(x, y) ((x)>(y)?(x):(y))

#define PACK(size, alloc) ((size)|(alloc))

#define GET(p)  (*(unsigned int *)(p)) 
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)     (GET(p)&~0x7)
#define GET_ALLOC(p)    (GET(p)&0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PRED_P(bp)  (*(void **)(bp))
#define SUCC_P(bp)  (*(void **)((bp)+WSIZE))
static void *heap_listp;
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *p, size_t size);
static void list_add(void *p);
static void list_remove(void *p);


/*
---------------------------
  * * * Free block * * *         
---------------------------

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


 패딩 다음에는 prolog 블록
    - 프롤로그는 8바이트 할당 블록

prolog는 스택의 프레임을 생성하기위한 과정
 - 다음 어셈블리어로 스택 프레임 구성
 - push ebp
 - mov ebp, esp

### 프롤로그 역할
- 프레임 포인터를 설정
- 로컬 변수를 위한 공간 할당
- 호출한 함수의 실행 상태를 보존

초기화 과정에서 생성되며, 반환하지 않는다.
프롤로그 다음 블록은 일반 블록


* next pointer
* perv pointer
---------------------------
힙은 항상 에필로그로 끝남
 - 헤더만으로 구성된 크기가 0으로 할당된 블록
### 에필로그 역할
- 호출한 함수의 실행 상태를 복구
- 스택을 정리하고, 프레임 포인터를 복구
- 함수로부터 복귀 -> 리턴 주소를 통해 원래 함수로 돌아감

*/


/* 
 * mm_init - initialize the malloc package.
---------------------------
|      block size     | a |   allocated 1:True 0:False
---------------------------
|   next pointer (pred)   |   
---------------------------
|   prev pointer (succ)   |
---------------------------
|        pro footer   | a |
---------------------------
|        epil header  | a |
---------------------------

 */
int mm_init(void) {   
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), heap_listp+(3*WSIZE));
    PUT(heap_listp + (3*WSIZE), heap_listp+(2*WSIZE));
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0,1));
    heap_listp += 2*WSIZE;
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    size = (words%2) ? (words+1)*WSIZE : words*WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
    return coalesce(bp);
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size == 0)
        return NULL;
    if (size<=DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1))/DSIZE);
    
    if ((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

static void *find_fit(size_t asize){
    void *ptr;
    for (ptr = SUCC_P(heap_listp); !GET_ALLOC(HDRP(ptr)) ; ptr = SUCC_P(ptr)){
        if (asize <= GET_SIZE(HDRP(ptr))){
            return ptr;
        }
    }
    return NULL;
}
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
    } else {
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
static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && !next_alloc){
        list_remove(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc){
        list_remove(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    } else if (!prev_alloc && !next_alloc){
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