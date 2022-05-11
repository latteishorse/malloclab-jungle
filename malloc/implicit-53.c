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

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4  
#define DSIZE 8 
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x)>(y)? (x) : (y)) 
#define PACK(size,alloc) ((size)| (alloc)) 

#define GET(p) (*(unsigned int*)(p))
#define PUT(p,val) (*(unsigned int*)(p)=(int)(val)) 

#define GET_SIZE(p) (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1) 
#define HDRP(bp) ((char*)(bp) - WSIZE) 
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE))) 
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) 

static char *heap_listp;
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);

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
|      next pointer       |   
---------------------------
|      prev pointer       |
---------------------------
|        pro footer   | a |
---------------------------
|        epil header  | a |
---------------------------

 */


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
* extend heap
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if  ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}
/*
*  coalesce
*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 전 블록 가용 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 가용 여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록 사이즈 확인

    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc) 
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc) 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
    {
        return NULL;
    }

    if (size <= DSIZE)
    {
        asize = 2*DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    if ((bp = find_fit(asize)) != NULL) 
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
* find_fit
*/
static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) 
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
        {
            return bp;
        }
    }
    return NULL;
}

/*
* place
*/
static void place(void *bp, size_t asize)
{ 
    size_t csize = GET_SIZE(HDRP(bp)); 
    if ( (csize-asize) >= (2 * DSIZE))
    { 
        PUT(HDRP(bp), PACK(asize,1)); 
        PUT(FTRP(bp), PACK(asize,1)); 
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0)); 
        PUT(FTRP(bp), PACK(csize-asize,0)); 
    }
    else
    {
        PUT(HDRP(bp), PACK(csize,1)); 
        PUT(FTRP(bp), PACK(csize,1));
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
// CLRS code
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
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

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
    {
        copySize = size;
    }

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

